#include "visualizer/ScenePlayer.hpp"
#include "bridge/WebSocketBridge.hpp"
#include <opencv2/highgui.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace adas {

ScenePlayer::ScenePlayer(const std::vector<SceneConfig>& scenes,
                         const ThreatConfig& threat_cfg,
                         int ws_port,
                         bool headless)
    : scenes_(scenes), threat_cfg_(threat_cfg),
      ws_port_(ws_port), headless_(headless) {}

void ScenePlayer::run() {
    // ── Init fixed modules ─────────────────────────────────────
    ThreatClassifier classifier(threat_cfg_);

    std::unique_ptr<Visualizer> viz;
    if (!headless_)
        viz = std::make_unique<Visualizer>(1600, 900);

    BridgeConfig bridge_cfg;
    bridge_cfg.port = ws_port_;

    WebSocketBridge bridge(bridge_cfg);
    bridge.start();

    // ── Load initial scene ─────────────────────────────────────
    auto loadScene = [&](int idx) {
        const auto& cfg = scenes_[idx];
        bridge_cfg.scene_name   = cfg.name;
        bridge_cfg.total_frames = 0; // updated after load
        current_scene_idx_      = idx;
        spdlog::info("ScenePlayer: loading scene '{}'", cfg.name);
    };

    loadScene(0);

    auto& cfg = scenes_[current_scene_idx_];
    DetectionLoader  loader(cfg.detection_path);
    VideoReader      video(cfg.video_path);
    KalmanTracker    tracker;

    int   total_frames = static_cast<int>(loader.totalFrames());
    float dt           = 1.0f / loader.meta().target_fps;

    cv::Mat last_good_frame;

    // Notify dashboard of initial scene
    nlohmann::json init_evt;
    init_evt["event"]        = "scene_loaded";
    init_evt["scene"]        = cfg.name;
    init_evt["total_frames"] = total_frames;
    init_evt["scenes"]       = nlohmann::json::array();
    for (const auto& s : scenes_)
        init_evt["scenes"].push_back(s.name);
    bridge.sendEvent(init_evt.dump());

    spdlog::info("ScenePlayer: '{}' | {} frames", cfg.name, total_frames);
    spdlog::info("Dashboard:   http://localhost:{}/info", ws_port_);
    spdlog::info("WebSocket:   ws://localhost:{}/ws",     ws_port_);

    while (true) {
        // ── Check window closed ────────────────────────────────
        if (!headless_ && viz && viz->windowClosed()) break;

        // ── Check for scene switch command ─────────────────────
        int switch_to = bridge.pendingSceneSwitch();
        if (switch_to >= 0 &&
            switch_to < static_cast<int>(scenes_.size()) &&
            switch_to != current_scene_idx_) {

            spdlog::info("ScenePlayer: switching to scene {}",
                         scenes_[switch_to].name);

            // Hot-swap — reload data sources, reset tracker
            const auto& new_cfg = scenes_[switch_to];
            loader      = DetectionLoader(new_cfg.detection_path);
            video       = VideoReader(new_cfg.video_path);
            tracker     = KalmanTracker();
            frame_idx_  = 0;
            total_frames = static_cast<int>(loader.totalFrames());
            dt           = 1.0f / loader.meta().target_fps;
            last_good_frame = cv::Mat();
            current_scene_idx_ = switch_to;

            // Notify dashboard
            nlohmann::json evt;
            evt["event"]        = "scene_changed";
            evt["scene"]        = new_cfg.name;
            evt["total_frames"] = total_frames;
            bridge.sendEvent(evt.dump());

            spdlog::info("ScenePlayer: scene '{}' loaded | {} frames",
                         new_cfg.name, total_frames);
            continue;
        }

        // ── Pause ──────────────────────────────────────────────
        if (state_ == PlaybackState::PAUSED) {
            if (!headless_ && viz) {
                int key = cv::waitKey(30);
                handleKey(key);
            } else {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(30));
            }
            continue;
        }

        // ── Loop scene ─────────────────────────────────────────
        if (!loader.hasNext()) {
            loader.reset();
            video.reset();
            tracker    = KalmanTracker();
            frame_idx_ = 0;
            last_good_frame = cv::Mat();
            continue;
        }

        // ── Get frame ──────────────────────────────────────────
        int64_t ts_ms = loader.peekTimestamp();

        cv::Mat video_frame;
        bool got = video.getFrameAt(ts_ms, video_frame);
        if (!got || video_frame.empty() ||
            video_frame.rows <= 0 || video_frame.cols <= 0) {
            if (!last_good_frame.empty())
                video_frame = last_good_frame.clone();
        } else {
            last_good_frame = video_frame.clone();
        }

        // ── Pipeline ───────────────────────────────────────────
        auto detections = loader.nextFrame();
        auto fused      = tracker.update(detections, dt);

        SystemSnapshot snapshot;
        snapshot.timestamp_ms   = ts_ms;
        snapshot.raw_detections = detections;
        snapshot.fused_objects  = fused;
        classifier.classify(snapshot);

        // Inject scene name into snapshot
        snapshot.overall_threat = snapshot.overall_threat;

        // ── Broadcast ──────────────────────────────────────────
        bridge.broadcast(snapshot, frame_idx_);

        // ── Render ─────────────────────────────────────────────
        if (!headless_ && viz) {
            int key = viz->render(video_frame, snapshot,
                                  scenes_[current_scene_idx_].name,
                                  frame_idx_, total_frames, state_);
            handleKey(key);
        }

        if (state_ == PlaybackState::STEPPING)
            state_ = PlaybackState::PAUSED;

        ++frame_idx_;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(frameDelayMs()));
    }

    bridge.stop();
    spdlog::info("ScenePlayer: exited");
}

void ScenePlayer::handleKey(int key) {
    if (key == -1) return;
    switch (key & 0xFF) {
        case ' ':
            state_ = (state_ == PlaybackState::PAUSED)
                     ? PlaybackState::PLAYING : PlaybackState::PAUSED;
            break;
        case 's': case 'S': state_ = PlaybackState::STEPPING;  break;
        case 'f': case 'F':
            state_ = (state_ == PlaybackState::FAST)
                     ? PlaybackState::PLAYING : PlaybackState::FAST;
            break;
        case 'w': case 'W':
            state_ = (state_ == PlaybackState::SLOW)
                     ? PlaybackState::PLAYING : PlaybackState::SLOW;
            break;
        case 'r': case 'R': state_ = PlaybackState::PLAYING;   break;
        case 'q': case 'Q': case 27:
            if (!headless_) cv::destroyAllWindows();
            ::exit(0);
            break;
        default: break;
    }
}

int ScenePlayer::frameDelayMs() const {
    switch (state_) {
        case PlaybackState::FAST: return 33;
        case PlaybackState::SLOW: return 200;
        default:                  return 100;
    }
}

} // namespace adas
