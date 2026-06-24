#pragma once
#include "TelemetryFrame.h"
#include <string>
#include <vector>

namespace obcsw {

/**
 * @brief JSON ↔ TelemetryFrame serialization.
 *
 * Uses nlohmann/json (header-only, MIT).
 * All methods are static — no state is held.
 */
class TelemetrySerializer {
public:
    /// Serialize a single frame to a compact JSON string.
    static std::string toJson(const TelemetryFrame& frame);

    /// Deserialize a JSON string back to a TelemetryFrame.
    /// Throws std::runtime_error on malformed JSON.
    static TelemetryFrame fromJson(const std::string& json_str);

    /// Serialize a batch of frames as a JSON array.
    static std::string batchToJson(const std::vector<TelemetryFrame>& frames);
};

} // namespace obcsw
