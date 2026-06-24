#pragma once
#include <cstdint>
#include <string>

namespace obcsw {

/**
 * @brief One snapshot of all subsystem telemetry values.
 *
 * Produced by the OBC main loop, passed through FaultDetector,
 * then handed off to TelemetryDispatcher for UDP downlink.
 */
struct TelemetryFrame {
    // --- Frame metadata ---
    uint64_t    frame_id     = 0;
    uint64_t    timestamp_ms = 0;
    std::string satellite_id;

    // --- Power subsystem ---
    double  battery_voltage_v  = 0.0;
    double  battery_current_ma = 0.0;
    double  solar_input_w      = 0.0;
    uint8_t charge_pct         = 0;

    // --- Thermal subsystem ---
    double obc_temp_c     = 0.0;
    double battery_temp_c = 0.0;
    double payload_temp_c = 0.0;

    // --- OBC resources ---
    uint8_t  cpu_usage_pct    = 0;
    uint8_t  memory_usage_pct = 0;
    uint64_t uptime_s         = 0;

    // --- Health flags ---
    bool     fault_active    = false;
    bool     safe_mode       = false;
    uint32_t last_fault_code = 0;
};

} // namespace obcsw
