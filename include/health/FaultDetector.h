#pragma once
#include "SatelliteConfig.h"
#include "TelemetryFrame.h"
#include <functional>
#include <cstdint>
#include <string>

namespace obcsw {

/// @brief Classification of detected faults.
enum class FaultCode : uint32_t {
    NONE             = 0,
    LOW_BATTERY      = 1,
    OVER_TEMPERATURE = 2,
    UNDER_TEMPERATURE = 3,
    CPU_OVERLOAD     = 4,
    MEMORY_OVERLOAD  = 5,
};

/**
 * @brief Payload delivered to the fault callback on every threshold violation.
 */
struct FaultEvent {
    FaultCode   code;
    std::string message;
    double      measured_value;
    double      threshold_value;
};

/**
 * @brief Stateless (per-frame) fault detection engine.
 *
 * Receives a TelemetryFrame, compares values against SatelliteConfig
 * thresholds, and fires the registered callback for every violation found.
 *
 * Thread-safety: evaluate() must be called from a single thread.
 * The callback itself is invoked synchronously inside evaluate().
 */
class FaultDetector {
public:
    using FaultCallback = std::function<void(const FaultEvent&)>;

    explicit FaultDetector(const SatelliteConfig& cfg);

    /// Register the callback that receives every FaultEvent.
    void setFaultCallback(FaultCallback cb) { m_fault_cb = std::move(cb); }

    /// Evaluate all subsystems in the supplied frame.
    void evaluate(const TelemetryFrame& frame);

private:
    void checkBattery    (const TelemetryFrame& frame);
    void checkTemperature(const TelemetryFrame& frame);
    void checkResources  (const TelemetryFrame& frame);

    const SatelliteConfig& m_cfg;
    FaultCallback          m_fault_cb;
};

} // namespace obcsw
