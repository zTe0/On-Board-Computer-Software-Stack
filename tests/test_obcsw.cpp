#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "SatelliteConfig.h"
#include "TelemetryFrame.h"
#include "health/FaultDetector.h"
#include "serialization/TelemetrySerializer.h"
#include "serialization/CommandDeserializer.h"
#include "command/CommandHandler.h"
#include "command/CommandQueue.h"

#include <thread>
#include <chrono>

using namespace obcsw;

// ============================================================
//  Helpers
// ============================================================
static SatelliteConfig defaultConfig() {
    SatelliteConfig cfg;
    cfg.battery_voltage_min_v  = 6.5;
    cfg.battery_voltage_max_v  = 8.4;
    cfg.temperature_min_c      = -20.0;
    cfg.temperature_max_c      = 85.0;
    cfg.cpu_usage_max_pct      = 90;
    cfg.memory_usage_max_pct   = 85;
    return cfg;
}

static TelemetryFrame nominalFrame() {
    TelemetryFrame f;
    f.frame_id           = 42;
    f.timestamp_ms       = 1700000000000ULL;
    f.satellite_id       = "SAT-1";
    f.battery_voltage_v  = 7.4;
    f.battery_current_ma = 250.0;
    f.solar_input_w      = 8.0;
    f.charge_pct         = 75;
    f.obc_temp_c         = 25.0;
    f.battery_temp_c     = 22.0;
    f.payload_temp_c     = 30.0;
    f.cpu_usage_pct      = 45;
    f.memory_usage_pct   = 50;
    f.uptime_s           = 3600;
    f.fault_active       = false;
    f.safe_mode          = false;
    f.last_fault_code    = 0;
    return f;
}

// ============================================================
//  FaultDetector — battery
// ============================================================
TEST_CASE("FaultDetector: no fault on nominal frame", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    int faults = 0;
    fd.setFaultCallback([&](const FaultEvent&) { ++faults; });
    fd.evaluate(nominalFrame());
    REQUIRE(faults == 0);
}

TEST_CASE("FaultDetector: LOW_BATTERY on under-voltage", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    FaultEvent captured{};
    fd.setFaultCallback([&](const FaultEvent& e) { captured = e; });

    auto frame = nominalFrame();
    frame.battery_voltage_v = 6.0; // below min 6.5

    fd.evaluate(frame);
    REQUIRE(captured.code == FaultCode::LOW_BATTERY);
    REQUIRE(captured.measured_value == Approx(6.0));
    REQUIRE(captured.threshold_value == Approx(6.5));
}

TEST_CASE("FaultDetector: LOW_BATTERY on over-voltage", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    FaultEvent captured{};
    fd.setFaultCallback([&](const FaultEvent& e) { captured = e; });

    auto frame = nominalFrame();
    frame.battery_voltage_v = 9.0; // above max 8.4

    fd.evaluate(frame);
    REQUIRE(captured.code == FaultCode::LOW_BATTERY);
    REQUIRE(captured.measured_value == Approx(9.0));
}

// ============================================================
//  FaultDetector — temperature
// ============================================================
TEST_CASE("FaultDetector: OVER_TEMPERATURE on high OBC temp", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    FaultEvent captured{};
    fd.setFaultCallback([&](const FaultEvent& e) { captured = e; });

    auto frame = nominalFrame();
    frame.obc_temp_c = 90.0; // above max 85

    fd.evaluate(frame);
    REQUIRE(captured.code == FaultCode::OVER_TEMPERATURE);
}

TEST_CASE("FaultDetector: UNDER_TEMPERATURE on cold OBC temp", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    FaultEvent captured{};
    fd.setFaultCallback([&](const FaultEvent& e) { captured = e; });

    auto frame = nominalFrame();
    frame.obc_temp_c = -25.0; // below min -20

    fd.evaluate(frame);
    REQUIRE(captured.code == FaultCode::UNDER_TEMPERATURE);
}

// ============================================================
//  FaultDetector — resources
// ============================================================
TEST_CASE("FaultDetector: CPU_OVERLOAD on high CPU usage", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    FaultEvent captured{};
    fd.setFaultCallback([&](const FaultEvent& e) { captured = e; });

    auto frame = nominalFrame();
    frame.cpu_usage_pct = 95; // above max 90

    fd.evaluate(frame);
    REQUIRE(captured.code == FaultCode::CPU_OVERLOAD);
    REQUIRE(captured.measured_value == Approx(95.0));
}

TEST_CASE("FaultDetector: MEMORY_OVERLOAD on high memory usage", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    FaultEvent captured{};
    fd.setFaultCallback([&](const FaultEvent& e) { captured = e; });

    auto frame = nominalFrame();
    frame.memory_usage_pct = 90; // above max 85

    fd.evaluate(frame);
    REQUIRE(captured.code == FaultCode::MEMORY_OVERLOAD);
}

TEST_CASE("FaultDetector: multiple faults fired in one frame", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    int count = 0;
    fd.setFaultCallback([&](const FaultEvent&) { ++count; });

    auto frame = nominalFrame();
    frame.obc_temp_c    = 90.0;  // OVER_TEMPERATURE
    frame.cpu_usage_pct = 95;    // CPU_OVERLOAD
    frame.memory_usage_pct = 90; // MEMORY_OVERLOAD

    fd.evaluate(frame);
    REQUIRE(count == 3);
}

TEST_CASE("FaultDetector: no callback set — no crash", "[fault]") {
    auto cfg = defaultConfig();
    FaultDetector fd(cfg);
    // No callback registered — should not crash
    auto frame = nominalFrame();
    frame.battery_voltage_v = 5.0;
    REQUIRE_NOTHROW(fd.evaluate(frame));
}

// ============================================================
//  TelemetrySerializer — round-trip
// ============================================================
TEST_CASE("TelemetrySerializer: round-trip toJson / fromJson", "[serialization]") {
    const auto original = nominalFrame();
    const std::string json = TelemetrySerializer::toJson(original);

    REQUIRE_FALSE(json.empty());

    const auto restored = TelemetrySerializer::fromJson(json);
    REQUIRE(restored.frame_id          == original.frame_id);
    REQUIRE(restored.satellite_id      == original.satellite_id);
    REQUIRE(restored.battery_voltage_v == Approx(original.battery_voltage_v));
    REQUIRE(restored.obc_temp_c        == Approx(original.obc_temp_c));
    REQUIRE(restored.cpu_usage_pct     == original.cpu_usage_pct);
    REQUIRE(restored.fault_active      == original.fault_active);
    REQUIRE(restored.safe_mode         == original.safe_mode);
    REQUIRE(restored.last_fault_code   == original.last_fault_code);
}

TEST_CASE("TelemetrySerializer: fromJson throws on invalid JSON", "[serialization]") {
    REQUIRE_THROWS_AS(
        TelemetrySerializer::fromJson("{not valid json"),
        std::runtime_error);
}

TEST_CASE("TelemetrySerializer: batchToJson produces valid JSON array", "[serialization]") {
    std::vector<TelemetryFrame> frames;
    for (int i = 0; i < 5; ++i) {
        auto f = nominalFrame();
        f.frame_id = static_cast<uint64_t>(i);
        frames.push_back(f);
    }
    const std::string batch = TelemetrySerializer::batchToJson(frames);
    REQUIRE_FALSE(batch.empty());
    REQUIRE(batch.front() == '[');
    REQUIRE(batch.back()  == ']');
}

TEST_CASE("TelemetrySerializer: JSON contains expected keys", "[serialization]") {
    const auto json_str = TelemetrySerializer::toJson(nominalFrame());
    REQUIRE(json_str.find("frame_id")         != std::string::npos);
    REQUIRE(json_str.find("battery_voltage_v") != std::string::npos);
    REQUIRE(json_str.find("obc_temp_c")       != std::string::npos);
    REQUIRE(json_str.find("health_flags")     != std::string::npos);
}

// ============================================================
//  CommandDeserializer
// ============================================================
TEST_CASE("CommandDeserializer: parse valid NOOP command", "[command]") {
    const std::string json = R"({"sequence_number":1,"opcode":1})";
    const auto cmd = CommandDeserializer::fromJson(json);
    REQUIRE(cmd.sequence_number == 1);
    REQUIRE(cmd.opcode == Opcode::NOOP);
}

TEST_CASE("CommandDeserializer: parse SET_MODE with subsystem and parameters", "[command]") {
    const std::string json = R"({
        "sequence_number": 99,
        "opcode": 16,
        "subsystem": "payload",
        "parameters": {"mode": "science", "exposure_ms": 500}
    })";
    const auto cmd = CommandDeserializer::fromJson(json);
    REQUIRE(cmd.opcode    == Opcode::SET_MODE);
    REQUIRE(cmd.subsystem == "payload");
    REQUIRE(cmd.parameters["mode"].get<std::string>() == "science");
}

TEST_CASE("CommandDeserializer: throws on unknown opcode", "[command]") {
    const std::string json = R"({"sequence_number":1,"opcode":255})";
    REQUIRE_THROWS_AS(CommandDeserializer::fromJson(json), std::invalid_argument);
}

TEST_CASE("CommandDeserializer: throws on malformed JSON", "[command]") {
    REQUIRE_THROWS_AS(CommandDeserializer::fromJson("{bad"), std::runtime_error);
}

TEST_CASE("CommandDeserializer: throws on missing required fields", "[command]") {
    REQUIRE_THROWS_AS(
        CommandDeserializer::fromJson(R"({"opcode":1})"), // no sequence_number
        std::runtime_error);
}

TEST_CASE("CommandDeserializer: opcodeName returns correct string", "[command]") {
    REQUIRE(CommandDeserializer::opcodeName(Opcode::NOOP)    == "NOOP");
    REQUIRE(CommandDeserializer::opcodeName(Opcode::SET_MODE) == "SET_MODE");
    REQUIRE(CommandDeserializer::opcodeName(Opcode::UNKNOWN) == "UNKNOWN");
}

// ============================================================
//  CommandHandler
// ============================================================
TEST_CASE("CommandHandler: dispatch registered handler", "[command]") {
    CommandHandler handler;
    bool called = false;
    handler.registerHandler(Opcode::NOOP, [&](const Command&) {
        called = true;
        return CommandResult::SUCCESS;
    });

    const auto result = handler.dispatchRaw(R"({"sequence_number":1,"opcode":1})");
    REQUIRE(result == CommandResult::SUCCESS);
    REQUIRE(called);
}

TEST_CASE("CommandHandler: REJECTED when no handler registered", "[command]") {
    CommandHandler handler;
    // No handler for NOOP
    const auto result = handler.dispatchRaw(R"({"sequence_number":1,"opcode":1})");
    REQUIRE(result == CommandResult::REJECTED);
}

TEST_CASE("CommandHandler: REJECTED on unknown opcode", "[command]") {
    CommandHandler handler;
    const auto result = handler.dispatchRaw(R"({"sequence_number":1,"opcode":255})");
    REQUIRE(result == CommandResult::REJECTED);
}

// ============================================================
//  CommandQueue
// ============================================================
TEST_CASE("CommandQueue: basic push and tryPop", "[queue]") {
    CommandQueue q(10);
    REQUIRE(q.empty());

    Command cmd;
    cmd.sequence_number = 7;
    cmd.opcode = Opcode::NOOP;
    REQUIRE(q.push(cmd));
    REQUIRE(q.size() == 1);

    auto popped = q.tryPop();
    REQUIRE(popped.has_value());
    REQUIRE(popped->sequence_number == 7);
    REQUIRE(q.empty());
}

TEST_CASE("CommandQueue: push beyond max_depth returns false", "[queue]") {
    CommandQueue q(2);
    Command cmd;
    cmd.opcode = Opcode::NOOP;
    REQUIRE(q.push(cmd));
    REQUIRE(q.push(cmd));
    REQUIRE_FALSE(q.push(cmd)); // queue full
    REQUIRE(q.size() == 2);
}

TEST_CASE("CommandQueue: tryPop returns nullopt when empty", "[queue]") {
    CommandQueue q(10);
    REQUIRE_FALSE(q.tryPop().has_value());
}

TEST_CASE("CommandQueue: popWait times out on empty queue", "[queue]") {
    CommandQueue q(10);
    const auto start = std::chrono::steady_clock::now();
    auto result = q.popWait(100); // 100 ms timeout
    const auto elapsed = std::chrono::steady_clock::now() - start;
    REQUIRE_FALSE(result.has_value());
    REQUIRE(elapsed >= std::chrono::milliseconds(90));
}

TEST_CASE("CommandQueue: popWait receives item pushed from another thread", "[queue]") {
    CommandQueue q(10);
    Command cmd;
    cmd.sequence_number = 55;
    cmd.opcode = Opcode::PAYLOAD_ON;

    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        q.push(cmd);
    });

    auto result = q.popWait(500);
    producer.join();

    REQUIRE(result.has_value());
    REQUIRE(result->sequence_number == 55);
    REQUIRE(result->opcode == Opcode::PAYLOAD_ON);
}
