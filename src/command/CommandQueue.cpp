#include "command/CommandQueue.h"

namespace obcsw {

CommandQueue::CommandQueue(uint32_t max_depth)
    : m_max_depth(max_depth) {}

bool CommandQueue::push(Command cmd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.size() >= m_max_depth) return false;
    m_queue.push(std::move(cmd));
    m_cv.notify_one();
    return true;
}

std::optional<Command> CommandQueue::popWait(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(m_mutex);
    const bool ready = m_cv.wait_for(
        lock,
        std::chrono::milliseconds(timeout_ms),
        [this] { return !m_queue.empty(); }
    );
    if (!ready) return std::nullopt;
    Command cmd = std::move(m_queue.front());
    m_queue.pop();
    return cmd;
}

std::optional<Command> CommandQueue::tryPop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) return std::nullopt;
    Command cmd = std::move(m_queue.front());
    m_queue.pop();
    return cmd;
}

bool CommandQueue::empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
}

uint32_t CommandQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint32_t>(m_queue.size());
}

} // namespace obcsw
