#pragma once
#include "common/Types.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>

// Forward declare Crow types without pulling in full headers
// (Crow headers are heavy — only include in .cpp)
// Crow forward declaration removed — included only in .cpp

namespace adas {

struct BridgeConfig {
    int         port        = 9002;
    std::string scene_name  = "unknown";
    int         total_frames = 0;
};

class WebSocketBridge {
public:
    explicit WebSocketBridge(const BridgeConfig& cfg);
    ~WebSocketBridge();

    // Start WebSocket server in background thread
    void start();

    // Stop the server
    void stop();

    // Broadcast snapshot to all connected clients
    // Called by ScenePlayer every frame — thread safe
    void broadcast(const SystemSnapshot& snapshot, int frame_idx);

    bool isRunning() const { return running_; }
    int  clientCount() const;

    // Returns -1 if no pending scene switch, otherwise scene index
    int  pendingSceneSwitch();

    // Send event to all clients
    void sendEvent(const std::string& event_json);

private:
    BridgeConfig        cfg_;
    std::atomic<bool>   running_{false};
    std::thread         server_thread_;

    // Latest snapshot JSON — updated each frame, read by new connections
    std::mutex          snapshot_mutex_;
    std::string         latest_snapshot_;
    std::atomic<int>    frame_idx_{0};
    std::atomic<int>    pending_scene_{-1};

    // Crow app lives on heap to avoid heavy include in header
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace adas
