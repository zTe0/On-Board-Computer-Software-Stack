#pragma once
#include <string>
#include <cstdint>

namespace obcsw {

/**
 * @brief Central configuration for the satellite OBC software.
 *
 * Loaded once at startup from a YAML file via ConfigLoader.
 * Passed by const-ref throughout the system to keep components
 * decoupled from the file-system.
 */
struct SatelliteConfig {
    // --- Satellite identity ---
    std::string satellite_name   = "UNKNOWN";
    std::string mission_id       = "UNKNOWN";
    std::string software_version = "0.0.0";

    // --- Telemetry ---
    uint32_t    telemetry_dispatch_interval_ms  = 1000;
    uint32_t    telemetry_max_frame_queue_depth = 64;
    std::string ground_station_ip               = "127.0.0.1";
    uint16_t    ground_station_port             = 5005;
    uint16_t    bind_port                       = 5004;

    // --- Command ---
    uint32_t command_max_queue_depth = 32;
    uint32_t command_timeout_ms      = 5000;

    // --- Health / Watchdog ---
    uint32_t watchdog_interval_ms = 2000;

    // --- Fault thresholds ---
    double  battery_voltage_min_v  = 6.5;
    double  battery_voltage_max_v  = 8.4;
    double  temperature_min_c      = -20.0;
    double  temperature_max_c      = 85.0;
    uint8_t cpu_usage_max_pct      = 90;
    uint8_t memory_usage_max_pct   = 85;
};

} // namespace obcsw
