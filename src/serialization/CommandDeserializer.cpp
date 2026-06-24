#include "serialization/CommandDeserializer.h"
#include <stdexcept>
#include <chrono>

using json = nlohmann::json;

namespace obcsw {

static const std::unordered_map<uint8_t, std::string> kOpcodeNames = {
    {0x01, "NOOP"},
    {0x10, "SET_MODE"},
    {0x11, "RESET_SUBSYSTEM"},
    {0x20, "PAYLOAD_ON"},
    {0x21, "PAYLOAD_OFF"},
    {0x30, "DOWNLINK_TELEMETRY"},
};

Command CommandDeserializer::fromJson(const std::string& json_str) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("CommandDeserializer: JSON parse error: ") + e.what());
    }

    if (!j.contains("opcode") || !j.contains("sequence_number")) {
        throw std::runtime_error("CommandDeserializer: missing required fields 'opcode' or 'sequence_number'");
    }

    Command cmd;
    cmd.sequence_number = j.at("sequence_number").get<uint32_t>();

    const uint8_t raw_opcode = j.at("opcode").get<uint8_t>();
    cmd.opcode = resolveOpcode(raw_opcode);

    if (cmd.opcode == Opcode::UNKNOWN) {
        throw std::invalid_argument(
            "CommandDeserializer: unknown or disallowed opcode 0x"
            + [raw_opcode]() {
                char buf[5];
                snprintf(buf, sizeof(buf), "%02X", raw_opcode);
                return std::string(buf);
            }()
        );
    }

    cmd.subsystem   = j.value("subsystem", "");
    cmd.parameters  = j.value("parameters", json::object());
    cmd.timestamp_ms = j.value("timestamp_ms",
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        )
    );

    return cmd;
}

Opcode CommandDeserializer::resolveOpcode(uint8_t raw) {
    switch (raw) {
        case 0x01: return Opcode::NOOP;
        case 0x10: return Opcode::SET_MODE;
        case 0x11: return Opcode::RESET_SUBSYSTEM;
        case 0x20: return Opcode::PAYLOAD_ON;
        case 0x21: return Opcode::PAYLOAD_OFF;
        case 0x30: return Opcode::DOWNLINK_TELEMETRY;
        default:   return Opcode::UNKNOWN;
    }
}

std::string CommandDeserializer::opcodeName(Opcode opcode) {
    auto it = kOpcodeNames.find(static_cast<uint8_t>(opcode));
    return it != kOpcodeNames.end() ? it->second : "UNKNOWN";
}

} // namespace obcsw
