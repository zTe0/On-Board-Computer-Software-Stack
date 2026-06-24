#include "health/FaultDetector.h"
#include <iostream>

namespace obcsw {

FaultDetector::FaultDetector(const SatelliteConfig& cfg)
    : m_cfg(cfg) {}

void FaultDetector::evaluate(const TelemetryFrame& frame) {
    checkBattery(frame);
    checkTemperature(frame);
    checkResources(frame);
}

void FaultDetector::checkBattery(const TelemetryFrame& frame) {
    if (!m_fault_cb) return;

    if (frame.battery_voltage_v < m_cfg.battery_voltage_min_v) {
        m_fault_cb(FaultEvent{
            FaultCode::LOW_BATTERY,
            "Battery voltage below minimum threshold",
            frame.battery_voltage_v,
            m_cfg.battery_voltage_min_v
        });
    } else if (frame.battery_voltage_v > m_cfg.battery_voltage_max_v) {
        // Over-voltage treated as a thermal/charging fault — map to LOW_BATTERY
        // with positive offset for clarity in logs.
        m_fault_cb(FaultEvent{
            FaultCode::LOW_BATTERY,
            "Battery voltage exceeds maximum threshold (overcharge)",
            frame.battery_voltage_v,
            m_cfg.battery_voltage_max_v
        });
    }
}

void FaultDetector::checkTemperature(const TelemetryFrame& frame) {
    if (!m_fault_cb) return;

    // Check OBC temperature (most critical)
    if (frame.obc_temp_c > m_cfg.temperature_max_c) {
        m_fault_cb(FaultEvent{
            FaultCode::OVER_TEMPERATURE,
            "OBC temperature exceeds maximum threshold",
            frame.obc_temp_c,
            m_cfg.temperature_max_c
        });
    } else if (frame.obc_temp_c < m_cfg.temperature_min_c) {
        m_fault_cb(FaultEvent{
            FaultCode::UNDER_TEMPERATURE,
            "OBC temperature below minimum threshold",
            frame.obc_temp_c,
            m_cfg.temperature_min_c
        });
    }
}

void FaultDetector::checkResources(const TelemetryFrame& frame) {
    if (!m_fault_cb) return;

    if (frame.cpu_usage_pct > m_cfg.cpu_usage_max_pct) {
        m_fault_cb(FaultEvent{
            FaultCode::CPU_OVERLOAD,
            "CPU usage exceeds maximum threshold",
            static_cast<double>(frame.cpu_usage_pct),
            static_cast<double>(m_cfg.cpu_usage_max_pct)
        });
    }

    if (frame.memory_usage_pct > m_cfg.memory_usage_max_pct) {
        m_fault_cb(FaultEvent{
            FaultCode::MEMORY_OVERLOAD,
            "Memory usage exceeds maximum threshold",
            static_cast<double>(frame.memory_usage_pct),
            static_cast<double>(m_cfg.memory_usage_max_pct)
        });
    }
}

} // namespace obcsw
