// Isolate Crow from rest of codebase
// Crow defines its own logging functions (info, debug, warn etc.)
// that conflict with spdlog when both are in the same translation unit.
// Solution: define CROW_LOG_LEVEL to suppress Crow's internal logging
// and use a raw printf for any bridge-level logs.
#define CROW_LOG_LEVEL 5  // suppress all Crow logs
#include <crow.h>

#include "bridge/WebSocketBridge.hpp"
#include "bridge/SnapshotSerializer.hpp"
#include <nlohmann/json.hpp>
#include <set>
#include <mutex>
#include <cstdio>

namespace adas {

// ── Impl ───────────────────────────────────────────────────────
struct WebSocketBridge::Impl {
    crow::SimpleApp                        app;
    std::set<crow::websocket::connection*> clients;
    std::mutex                             clients_mutex;
};

// ── Constructor / Destructor ───────────────────────────────────
WebSocketBridge::WebSocketBridge(const BridgeConfig& cfg)
    : cfg_(cfg), impl_(std::make_unique<Impl>()) {}

WebSocketBridge::~WebSocketBridge() { stop(); }

// ── start ──────────────────────────────────────────────────────
void WebSocketBridge::start() {
    if (running_) return;
    running_ = true;

    CROW_WEBSOCKET_ROUTE(impl_->app, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            {
                std::lock_guard<std::mutex> lk(impl_->clients_mutex);
                impl_->clients.insert(&conn);
            }
            std::lock_guard<std::mutex> lk(snapshot_mutex_);
            if (!latest_snapshot_.empty())
                conn.send_text(latest_snapshot_);
        })
                .onclose([this](crow::websocket::connection& conn,
                        const std::string&, uint16_t) {
            std::lock_guard<std::mutex> lk(impl_->clients_mutex);
            impl_->clients.erase(&conn);
        })
        .onmessage([this](crow::websocket::connection&,
                       const std::string& msg, bool) {
            try {
                auto j = nlohmann::json::parse(msg);
                if (j.value("cmd","") == "switch_scene") {
                    int idx = j.value("scene", -1);
                    if (idx >= 0) pending_scene_.store(idx);
                }
            } catch (...) {}
        });

    CROW_ROUTE(impl_->app, "/info")
    ([this]() {
        nlohmann::json info = {
            {"scene",        cfg_.scene_name},
            {"total_frames", cfg_.total_frames},
            {"port",         cfg_.port},
            {"status",       "running"},
        };
        crow::response res(info.dump());
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        return res;
    });

    CROW_ROUTE(impl_->app, "/snapshot")
    ([this]() {
        std::lock_guard<std::mutex> lk(snapshot_mutex_);
        crow::response res(latest_snapshot_.empty()
                           ? "{}" : latest_snapshot_);
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        return res;
    });

    CROW_ROUTE(impl_->app, "/<path>")
        .methods(crow::HTTPMethod::Options)
    ([](const crow::request&, std::string) {
        crow::response res(204);
        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    int port = cfg_.port;
    server_thread_ = std::thread([this, port]() {
        printf("[bridge] WebSocket listening on ws://localhost:%d/ws\n",
               port);
        impl_->app.port(port).multithreaded().run();
    });
}

// ── stop ───────────────────────────────────────────────────────
void WebSocketBridge::stop() {
    if (!running_) return;
    running_ = false;
    impl_->app.stop();
    if (server_thread_.joinable())
        server_thread_.join();
}

// ── broadcast ──────────────────────────────────────────────────
void WebSocketBridge::broadcast(const SystemSnapshot& snapshot,
                                 int frame_idx)
{
    auto j = SnapshotSerializer::toJson(snapshot);
    j["frame_idx"]    = frame_idx;
    j["total_frames"] = cfg_.total_frames;
    j["scene"]        = cfg_.scene_name;
    std::string msg   = j.dump();

    {
        std::lock_guard<std::mutex> lk(snapshot_mutex_);
        latest_snapshot_ = msg;
        frame_idx_       = frame_idx;
    }

    std::lock_guard<std::mutex> lk(impl_->clients_mutex);
    for (auto* conn : impl_->clients) {
        try { conn->send_text(msg); }
        catch (...) {}
    }
}

// ── clientCount ────────────────────────────────────────────────
int WebSocketBridge::clientCount() const {
    std::lock_guard<std::mutex> lk(
        const_cast<std::mutex&>(impl_->clients_mutex));
    return static_cast<int>(impl_->clients.size());
}

int WebSocketBridge::pendingSceneSwitch() {
    return pending_scene_.exchange(-1);
}

void WebSocketBridge::sendEvent(const std::string& event_json) {
    std::lock_guard<std::mutex> lk(impl_->clients_mutex);
    for (auto* conn : impl_->clients) {
        try { conn->send_text(event_json); }
        catch (...) {}
    }
}

} // namespace adas
