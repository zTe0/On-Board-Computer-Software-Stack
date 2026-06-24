#include "serialization/TelemetrySerializer.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

namespace obcsw {

std::string TelemetrySerializer::toJson(const TelemetryFrame& f) {
    json j;
    j["frame_id"]      = f.frame_id;
    j["timestamp_ms"]  = f.timestamp_ms;
    j["satellite_id"]  = f.satellite_id;

    j["subsystems"]["power"] = {
        {"battery_voltage_v",  f.battery_voltage_v},
        {"battery_current_ma", f.battery_current_ma},
        {"solar_input_w",      f.solar_input_w},
        {"charge_pct",         f.charge_pct}
    };

    j["subsystems"]["thermal"] = {
        {"obc_temp_c",     f.obc_temp_c},
        {"battery_temp_c", f.battery_temp_c},
        {"payload_temp_c", f.payload_temp_c}
    };

    j["subsystems"]["obc"] = {
        {"cpu_usage_pct",    f.cpu_usage_pct},
        {"memory_usage_pct", f.memory_usage_pct},
        {"uptime_s",         f.uptime_s}
    };

    j["health_flags"] = {
        {"fault_active",     f.fault_active},
        {"safe_mode",        f.safe_mode},
        {"last_fault_code",  f.last_fault_code}
    };

    return j.dump();
}

TelemetryFrame TelemetrySerializer::fromJson(const std::string& json_str) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("TelemetrySerializer: JSON parse error: ") + e.what());
    }

    TelemetryFrame f;
    f.frame_id     = j.at("frame_id").get<uint64_t>();
    f.timestamp_ms = j.at("timestamp_ms").get<uint64_t>();
    f.satellite_id = j.at("satellite_id").get<std::string>();

    const auto& power   = j.at("subsystems").at("power");
    f.battery_voltage_v  = power.at("battery_voltage_v").get<double>();
    f.battery_current_ma = power.at("battery_current_ma").get<double>();
    f.solar_input_w      = power.at("solar_input_w").get<double>();
    f.charge_pct         = power.at("charge_pct").get<uint8_t>();

    const auto& thermal = j.at("subsystems").at("thermal");
    f.obc_temp_c     = thermal.at("obc_temp_c").get<double>();
    f.battery_temp_c = thermal.at("battery_temp_c").get<double>();
    f.payload_temp_c = thermal.at("payload_temp_c").get<double>();

    const auto& obc    = j.at("subsystems").at("obc");
    f.cpu_usage_pct    = obc.at("cpu_usage_pct").get<uint8_t>();
    f.memory_usage_pct = obc.at("memory_usage_pct").get<uint8_t>();
    f.uptime_s         = obc.at("uptime_s").get<uint64_t>();

    const auto& hf   = j.at("health_flags");
    f.fault_active   = hf.at("fault_active").get<bool>();
    f.safe_mode      = hf.at("safe_mode").get<bool>();
    f.last_fault_code = hf.at("last_fault_code").get<uint32_t>();

    return f;
}

std::string TelemetrySerializer::batchToJson(const std::vector<TelemetryFrame>& frames) {
    json arr = json::array();
    for (const auto& f : frames) {
        arr.push_back(json::parse(toJson(f)));
    }
    return arr.dump();
}

} // namespace obcsw
