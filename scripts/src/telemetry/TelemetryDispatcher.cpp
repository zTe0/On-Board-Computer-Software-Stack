#include "telemetry/TelemetryDispatcher.h"
#include "serialization/TelemetrySerializer.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>

// POSIX socket headers (Linux / Embedded Linux)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>  // add any other POSIX socket headers you use
#endif

namespace obcsw {

TelemetryDispatcher::TelemetryDispatcher(const DispatcherConfig& cfg)
    : m_cfg(cfg) {}

TelemetryDispatcher::~TelemetryDispatcher() {
    stop();
}

bool TelemetryDispatcher::enqueue(TelemetryFrame frame) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    if (m_queue.size() >= m_cfg.max_queue_depth) {
        ++m_frames_dropped;
        return false;
    }
    m_queue.push(std::move(frame));
    return true;
}

void TelemetryDispatcher::start() {
    if (m_running.load()) return;

    // Create UDP socket
    m_sock_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sock_fd < 0) {
        throw std::runtime_error("TelemetryDispatcher: failed to create UDP socket");
    }

    m_running = true;
    m_thread  = std::thread(&TelemetryDispatcher::dispatchLoop, this);
}

void TelemetryDispatcher::stop() {
    if (!m_running.load()) return;
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
    if (m_sock_fd >= 0) {
        ::close(m_sock_fd);
        m_sock_fd = -1;
    }
}

void TelemetryDispatcher::dispatchLoop() {
    while (m_running.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(m_cfg.dispatch_interval_ms));

        // Drain entire queue each interval
        while (true) {
            TelemetryFrame frame;
            {
                std::lock_guard<std::mutex> lock(m_queue_mutex);
                if (m_queue.empty()) break;
                frame = m_queue.front();
                m_queue.pop();
            }
            const std::string payload = TelemetrySerializer::toJson(frame);
            if (sendUdp(payload)) {
                ++m_frames_sent;
            } else {
                ++m_frames_dropped;
                std::cerr << "[TelemetryDispatcher] UDP send failed for frame "
                          << frame.frame_id << "\n";
            }
        }
    }
}

bool TelemetryDispatcher::sendUdp(const std::string& payload) {
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(m_cfg.dest_port);
    if (::inet_pton(AF_INET, m_cfg.dest_ip.c_str(), &dest.sin_addr) != 1) {
        std::cerr << "[TelemetryDispatcher] Invalid IP: " << m_cfg.dest_ip << "\n";
        return false;
    }

    const ssize_t sent = ::sendto(
        m_sock_fd,
        payload.data(),
        payload.size(),
        0,
        reinterpret_cast<const sockaddr*>(&dest),
        sizeof(dest)
    );
    return sent == static_cast<ssize_t>(payload.size());
}

} // namespace obcsw
