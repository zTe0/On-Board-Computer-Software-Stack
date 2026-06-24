#pragma once
#include "serialization/CommandDeserializer.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <cstdint>

namespace obcsw {

/**
 * @brief Thread-safe, bounded FIFO queue for uplink Commands.
 *
 * Producer: the UDP receive thread pushes parsed Commands.
 * Consumer: the command-execution thread pops via popWait().
 */
class CommandQueue {
public:
    explicit CommandQueue(uint32_t max_depth = 32);

    /// Push a command. Returns false and drops if the queue is full.
    bool push(Command cmd);

    /// Block until a command is available or timeout_ms elapses.
    std::optional<Command> popWait(uint32_t timeout_ms = 1000);

    /// Non-blocking pop; returns nullopt if queue is empty.
    std::optional<Command> tryPop();

    bool     empty() const;
    uint32_t size()  const;

private:
    const uint32_t          m_max_depth;
    std::queue<Command>     m_queue;
    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
};

} // namespace obcsw
