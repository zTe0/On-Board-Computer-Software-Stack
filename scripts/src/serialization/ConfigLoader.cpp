#include "serialization/ConfigLoader.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <sstream>

namespace obcsw {

SatelliteConfig ConfigLoader::load(const std::string& yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    } catch (const YAML::BadFile& e) {
        throw std::runtime_error("ConfigLoader: cannot open '" + yaml_path + "': " + e.what());
    } catch (const YAML::ParserException& e) {
        throw std::runtime_error("ConfigLoader: YAML parse error in '" + yaml_path + "': " + e.what());
    }

    SatelliteConfig cfg;

    // [satellite]
    auto sat = root["satellite"];
    if (!sat) throw std::runtime_error("ConfigLoader: missing 'satellite' section");
    cfg.satellite_name    = sat["name"].as<std::string>("UNKNOWN");
    cfg.mission_id        = sat["mission_id"].as<std::string>("UNKNOWN");
    cfg.software_version  = sat["software_version"].as<std::string>("0.0.0");

    // [telemetry]
    if (auto tlm = root["telemetry"]) {
        cfg.telemetry_dispatch_interval_ms  = tlm["dispatch_interval_ms"].as<uint32_t>(1000);
        cfg.telemetry_max_frame_queue_depth = tlm["max_frame_queue_depth"].as<uint32_t>(64);
        if (auto udp = tlm["udp"]) {
            cfg.ground_station_ip   = udp["ground_station_ip"].as<std::string>("127.0.0.1");
            cfg.ground_station_port = udp["ground_station_port"].as<uint16_t>(5005);
            cfg.bind_port           = udp["bind_port"].as<uint16_t>(5004);
        }
    }

    // [command]
    if (auto cmd = root["command"]) {
        cfg.command_max_queue_depth = cmd["max_queue_depth"].as<uint32_t>(32);
        cfg.command_timeout_ms      = cmd["command_timeout_ms"].as<uint32_t>(5000);
    }

    // [health]
    if (auto health = root["health"]) {
        cfg.watchdog_interval_ms = health["watchdog_interval_ms"].as<uint32_t>(2000);
        if (auto thr = health["fault_thresholds"]) {
            cfg.battery_voltage_min_v  = thr["battery_voltage_min_v"].as<double>(6.5);
            cfg.battery_voltage_max_v  = thr["battery_voltage_max_v"].as<double>(8.4);
            cfg.temperature_min_c      = thr["temperature_min_c"].as<double>(-20.0);
            cfg.temperature_max_c      = thr["temperature_max_c"].as<double>(85.0);
            cfg.cpu_usage_max_pct      = thr["cpu_usage_max_pct"].as<uint8_t>(90);
            cfg.memory_usage_max_pct   = thr["memory_usage_max_pct"].as<uint8_t>(85);
        }
    }

    // Basic validation
    if (cfg.ground_station_ip.empty())
        throw std::runtime_error("ConfigLoader: ground_station_ip must not be empty");
    if (cfg.battery_voltage_min_v >= cfg.battery_voltage_max_v)
        throw std::runtime_error("ConfigLoader: battery_voltage_min_v must be < max_v");

    return cfg;
}

} // namespace obcsw
