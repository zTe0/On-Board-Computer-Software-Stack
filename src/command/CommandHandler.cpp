#include "command/CommandHandler.h"
#include <iostream>

namespace obcsw {

void CommandHandler::registerHandler(Opcode opcode, CommandCallback callback) {
    m_handlers[static_cast<uint8_t>(opcode)] = std::move(callback);
}

CommandResult CommandHandler::dispatch(const Command& cmd) {
    auto it = m_handlers.find(static_cast<uint8_t>(cmd.opcode));
    if (it == m_handlers.end()) {
        std::cerr << "[CommandHandler] No handler registered for opcode "
                  << CommandDeserializer::opcodeName(cmd.opcode) << "\n";
        return CommandResult::REJECTED;
    }
    return it->second(cmd);
}

CommandResult CommandHandler::dispatchRaw(const std::string& json_str) {
    Command cmd;
    try {
        cmd = CommandDeserializer::fromJson(json_str);
    } catch (const std::invalid_argument& e) {
        std::cerr << "[CommandHandler] Invalid opcode: " << e.what() << "\n";
        return CommandResult::REJECTED;
    } catch (const std::runtime_error& e) {
        std::cerr << "[CommandHandler] Malformed command: " << e.what() << "\n";
        return CommandResult::REJECTED;
    }
    return dispatch(cmd);
}

} // namespace obcsw
