#pragma once
#include "TelemetryFrame.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace obcsw {

struct DispatcherConfig {
    std::string dest_ip           = "127.0.0.1";
    uint16_t    dest_port         = 5005;
    uint32_t    dispatch_interval_ms = 1000;
    uint32_t    max_queue_depth   = 64;
};

/**
 * @brief Background thread that drains a TelemetryFrame queue over UDP.
 *
 * Frames are serialized to JSON by TelemetrySerializer and sent as
 * individual UDP datagrams to the configured ground-station address.
 *
 * Thread-safety: enqueue() is safe to call from any thread while
 * the dispatcher is running.
 */
class TelemetryDispatcher {
public:
    explicit TelemetryDispatcher(const DispatcherConfig& cfg);
    ~TelemetryDispatcher();

    TelemetryDispatcher(const TelemetryDispatcher&)            = delete;
    TelemetryDispatcher& operator=(const TelemetryDispatcher&) = delete;

    /// Enqueue a frame for dispatch. Returns false if the queue is full.
    bool enqueue(TelemetryFrame frame);

    /// Start the background dispatch thread and open the UDP socket.
    void start();

    /// Stop the thread gracefully and close the socket.
    void stop();

    uint64_t framesSent()    const { return m_frames_sent.load();    }
    uint64_t framesDropped() const { return m_frames_dropped.load(); }

private:
    void dispatchLoop();
    bool sendUdp(const std::string& payload);

    DispatcherConfig            m_cfg;
    std::queue<TelemetryFrame>  m_queue;
    std::mutex                  m_queue_mutex;
    std::thread                 m_thread;
    std::atomic<bool>           m_running{false};
    int                         m_sock_fd{-1};
    std::atomic<uint64_t>       m_frames_sent{0};
    std::atomic<uint64_t>       m_frames_dropped{0};
};

} // namespace obcsw
