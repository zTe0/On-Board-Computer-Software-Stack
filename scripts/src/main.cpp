#include "SatelliteConfig.h"
#include "TelemetryFrame.h"
#include "health/FaultDetector.h"
#include "telemetry/TelemetryDispatcher.h"
#include "serialization/TelemetrySerializer.h"
#include "serialization/ConfigLoader.h"
#include "serialization/CommandDeserializer.h"
#include "command/CommandHandler.h"
#include "command/CommandQueue.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace {
    std::atomic<bool> g_running{true};
}

void signalHandler(int) {
    g_running = false;
}

/// Simulate realistic satellite telemetry for demo/testing purposes.
obcsw::TelemetryFrame generateFrame(
    uint64_t frame_id,
    const std::string& satellite_id,
    double t_s,
    bool fault_active = false,
    bool safe_mode = false,
    uint32_t last_fault_code = 0)
{
    obcsw::TelemetryFrame f;
    f.frame_id      = frame_id;
    f.timestamp_ms  = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    f.satellite_id  = satellite_id;

    // Sinusoidal battery sim (6-hour eclipse/sunlight cycle)
    const double cycle = std::sin(t_s / 21600.0 * 3.14159);
    f.battery_voltage_v  = 7.4 + 0.8 * cycle;
    f.battery_current_ma = 300.0 + 150.0 * std::abs(cycle);
    f.solar_input_w      = std::max(0.0, 12.0 * cycle);
    f.charge_pct         = static_cast<uint8_t>(std::min(100.0, 50.0 + 40.0 * cycle));

    // Temperature oscillates with orbit
    f.obc_temp_c     = 25.0 + 15.0 * std::sin(t_s / 5400.0 * 3.14159);
    f.battery_temp_c = 20.0 + 10.0 * std::sin(t_s / 5400.0 * 3.14159 + 0.5);
    f.payload_temp_c = 30.0 + 20.0 * std::sin(t_s / 5400.0 * 3.14159 + 1.0);

    // CPU/memory slowly drift upward then reset
    f.cpu_usage_pct    = static_cast<uint8_t>(30 + (frame_id % 60));
    f.memory_usage_pct = static_cast<uint8_t>(40 + (frame_id % 45));
    f.uptime_s         = static_cast<uint64_t>(t_s);

    f.fault_active    = fault_active;
    f.safe_mode       = safe_mode;
    f.last_fault_code = last_fault_code;

    return f;
}

/// Register all uplink command handlers.
void registerCommandHandlers(obcsw::CommandHandler& handler) {
    using obcsw::Opcode;
    using obcsw::CommandResult;
    using obcsw::Command;

    handler.registerHandler(Opcode::NOOP, [](const Command& cmd) {
        std::cout << "[CMD] NOOP seq=" << cmd.sequence_number << "\n";
        return CommandResult::SUCCESS;
    });

    handler.registerHandler(Opcode::SET_MODE, [](const Command& cmd) {
        std::cout << "[CMD] SET_MODE subsystem=" << cmd.subsystem
                  << " params=" << cmd.parameters.dump() << "\n";
        return CommandResult::SUCCESS;
    });

    handler.registerHandler(Opcode::RESET_SUBSYSTEM, [](const Command& cmd) {
        std::cout << "[CMD] RESET_SUBSYSTEM subsystem=" << cmd.subsystem << "\n";
        return CommandResult::SUCCESS;
    });

    handler.registerHandler(Opcode::PAYLOAD_ON, [](const Command& cmd) {
        std::cout << "[CMD] PAYLOAD_ON seq=" << cmd.sequence_number << "\n";
        return CommandResult::SUCCESS;
    });

    handler.registerHandler(Opcode::PAYLOAD_OFF, [](const Command& cmd) {
        std::cout << "[CMD] PAYLOAD_OFF seq=" << cmd.sequence_number << "\n";
        return CommandResult::SUCCESS;
    });

    handler.registerHandler(Opcode::DOWNLINK_TELEMETRY, [](const Command& cmd) {
        std::cout << "[CMD] DOWNLINK_TELEMETRY requested seq=" << cmd.sequence_number << "\n";
        return CommandResult::SUCCESS;
    });
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // ------------------------------------------------------------------
    // Load config
    // ------------------------------------------------------------------
    const std::string config_path = (argc > 1) ? argv[1] : "config/satellite.yaml";
    obcsw::SatelliteConfig cfg;
    try {
        cfg = obcsw::ConfigLoader::load(config_path);
    } catch (const std::exception& e) {
        std::cerr << "[MAIN] Config load failed: " << e.what()
                  << " — using defaults.\n";
    }

    std::cout << "=================================================\n"
              << "  obcsw  |  " << cfg.satellite_name
              << "  |  Mission: " << cfg.mission_id << "\n"
              << "  Software version: " << cfg.software_version << "\n"
              << "=================================================\n";

    // ------------------------------------------------------------------
    // Build subsystems
    // ------------------------------------------------------------------
    obcsw::FaultDetector fault_detector(cfg);
    bool safe_mode       = false;
    uint32_t fault_code  = 0;
    bool fault_active    = false;

    fault_detector.setFaultCallback([&](const obcsw::FaultEvent& evt) {
        fault_active = true;
        fault_code   = static_cast<uint32_t>(evt.code);
        std::cerr << "[FAULT] " << evt.message
                  << " (measured=" << std::fixed << std::setprecision(2)
                  << evt.measured_value
                  << ", threshold=" << evt.threshold_value << ")\n";

        // Escalate CPU/memory overload to safe-mode automatically
        if (evt.code == obcsw::FaultCode::CPU_OVERLOAD ||
            evt.code == obcsw::FaultCode::MEMORY_OVERLOAD) {
            safe_mode = true;
            std::cerr << "[FAULT] Entering safe mode.\n";
        }
    });

    obcsw::DispatcherConfig disp_cfg;
    disp_cfg.dest_ip             = cfg.ground_station_ip;
    disp_cfg.dest_port           = cfg.ground_station_port;
    disp_cfg.dispatch_interval_ms = cfg.telemetry_dispatch_interval_ms;
    disp_cfg.max_queue_depth     = cfg.telemetry_max_frame_queue_depth;

    obcsw::TelemetryDispatcher dispatcher(disp_cfg);
    dispatcher.start();

    obcsw::CommandQueue   cmd_queue(cfg.command_max_queue_depth);
    obcsw::CommandHandler cmd_handler;
    registerCommandHandlers(cmd_handler);

    // ------------------------------------------------------------------
    // Demonstration: inject a few test commands into the queue
    // ------------------------------------------------------------------
    auto injectCmd = [&](const std::string& json) {
        try {
            cmd_queue.push(obcsw::CommandDeserializer::fromJson(json));
        } catch (const std::exception& e) {
            std::cerr << "[MAIN] Bad test command: " << e.what() << "\n";
        }
    };

    injectCmd(R"({"sequence_number":1,"opcode":1})");
    injectCmd(R"({"sequence_number":2,"opcode":16,"subsystem":"payload","parameters":{"mode":"science"}})");
    injectCmd(R"({"sequence_number":3,"opcode":32})");

    // ------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------
    uint64_t frame_id = 0;
    double   t_s      = 0.0;
    const double dt_s = static_cast<double>(cfg.telemetry_dispatch_interval_ms) / 1000.0;

    std::cout << "[MAIN] OBC loop running — Ctrl+C to stop.\n";

    while (g_running.load()) {
        // 1. Generate telemetry frame
        auto frame = generateFrame(frame_id++, cfg.satellite_name, t_s,
                                   fault_active, safe_mode, fault_code);
        t_s += dt_s;

        // 2. Evaluate faults
        fault_active = false;  // reset; callback re-sets if violation found
        fault_detector.evaluate(frame);

        // 3. Update frame flags based on fault evaluation
        frame.fault_active    = fault_active;
        frame.safe_mode       = safe_mode;
        frame.last_fault_code = fault_code;

        // 4. Print human-readable snapshot
        std::cout << "[TLM ] frame=" << std::setw(4) << frame.frame_id
                  << " bat=" << std::fixed << std::setprecision(2)
                  << frame.battery_voltage_v << "V"
                  << " obc_tmp=" << frame.obc_temp_c << "°C"
                  << " cpu=" << static_cast<int>(frame.cpu_usage_pct) << "%"
                  << " mem=" << static_cast<int>(frame.memory_usage_pct) << "%"
                  << (frame.safe_mode ? " [SAFE-MODE]" : "")
                  << "\n";

        // 5. Enqueue for UDP downlink
        if (!dispatcher.enqueue(frame)) {
            std::cerr << "[MAIN] Telemetry queue full — frame dropped.\n";
        }

        // 6. Drain command queue
        while (auto cmd = cmd_queue.tryPop()) {
            const auto result = cmd_handler.dispatch(*cmd);
            if (result != obcsw::CommandResult::SUCCESS) {
                std::cerr << "[CMD ] Dispatch failed for seq=" << cmd->sequence_number << "\n";
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(cfg.telemetry_dispatch_interval_ms));
    }

    // ------------------------------------------------------------------
    // Shutdown
    // ------------------------------------------------------------------
    std::cout << "\n[MAIN] Shutting down...\n";
    dispatcher.stop();
    std::cout << "[MAIN] Frames sent=" << dispatcher.framesSent()
              << " dropped=" << dispatcher.framesDropped() << "\n";
    return 0;
}
