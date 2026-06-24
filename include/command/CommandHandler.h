#pragma once
#include "serialization/CommandDeserializer.h"
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace obcsw {

/// @brief Result returned by every command dispatch.
enum class CommandResult : uint8_t {
    SUCCESS  = 0,
    REJECTED = 1,
    TIMEOUT  = 2,
    ERROR    = 3,
};

/**
 * @brief Routes deserialized Commands to registered handler functions.
 *
 * Usage:
 * @code
 *   CommandHandler h;
 *   h.registerHandler(Opcode::NOOP, [](const Command&) {
 *       return CommandResult::SUCCESS;
 *   });
 *   h.dispatchRaw(json_string);
 * @endcode
 */
class CommandHandler {
public:
    using CommandCallback = std::function<CommandResult(const Command&)>;

    /// Register a handler for the given opcode (replaces any existing one).
    void registerHandler(Opcode opcode, CommandCallback callback);

    /// Dispatch an already-parsed command.
    CommandResult dispatch(const Command& cmd);

    /// Parse JSON, then dispatch. Returns REJECTED on deserialization failure.
    CommandResult dispatchRaw(const std::string& json_str);

private:
    std::unordered_map<uint8_t, CommandCallback> m_handlers;
};

} // namespace obcsw
