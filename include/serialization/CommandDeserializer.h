#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace obcsw {

/// @brief Uplink command opcodes.
enum class Opcode : uint8_t {
    NOOP               = 0x01,
    SET_MODE           = 0x10,
    RESET_SUBSYSTEM    = 0x11,
    PAYLOAD_ON         = 0x20,
    PAYLOAD_OFF        = 0x21,
    DOWNLINK_TELEMETRY = 0x30,
    UNKNOWN            = 0xFF,
};

/**
 * @brief Fully-parsed uplink command.
 */
struct Command {
    uint32_t            sequence_number = 0;
    Opcode              opcode          = Opcode::UNKNOWN;
    std::string         subsystem;
    nlohmann::json      parameters;
    uint64_t            timestamp_ms    = 0;
};

/**
 * @brief JSON → Command deserialization.
 *
 * Validates opcode allowlist; throws on unknown opcodes or malformed JSON.
 */
class CommandDeserializer {
public:
    /// Parse a JSON string into a Command.
    /// Throws std::invalid_argument for unknown opcodes.
    /// Throws std::runtime_error for malformed JSON or missing fields.
    static Command fromJson(const std::string& json_str);

    /// Map a raw byte to the Opcode enum (returns UNKNOWN if not found).
    static Opcode resolveOpcode(uint8_t raw);

    /// Human-readable name for logging.
    static std::string opcodeName(Opcode opcode);
};

} // namespace obcsw
