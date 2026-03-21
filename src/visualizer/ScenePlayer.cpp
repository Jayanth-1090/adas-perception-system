#include "visualizer/ScenePlayer.hpp"
#include "bridge/WebSocketBridge.hpp"
#include <opencv2/highgui.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace adas {

ScenePlayer::ScenePlayer(const SceneConfig& cfg,
                         const ThreatConfig& threat_cfg,
                         int ws_port)
    : cfg_(cfg), threat_cfg_(threat_cfg), ws_port_(ws_port) {}

void ScenePlayer::run() {
    DetectionLoader  loader(cfg_.detection_path);
    VideoReader      video(cfg_.video_path);
    KalmanTracker    tracker;
    ThreatClassifier classifier(threat_cfg_);
    Visualizer       viz(1600, 900);

    const int   total_frames = static_cast<int>(loader.totalFrames());
    const float dt           = 1.0f / loader.meta().target_fps;

    // ── Start WebSocket bridge ─────────────────────────────────
    BridgeConfig bridge_cfg;
    bridge_cfg.port         = ws_port_;
    bridge_cfg.scene_name   = cfg_.name;
    bridge_cfg.total_frames = total_frames;

    WebSocketBridge bridge(bridge_cfg);
    bridge.start();

    cv::Mat last_good_frame;

    spdlog::info("ScenePlayer: '{}' | {} frames | dt={:.3f}s",
                 cfg_.name, total_frames, dt);
    spdlog::info("Dashboard:   http://localhost:{}/info", ws_port_);
    spdlog::info("WebSocket:   ws://localhost:{}/ws",     ws_port_);

    while (!viz.windowClosed()) {

        if (state_ == PlaybackState::PAUSED) {
            int key = cv::waitKey(30);
            handleKey(key);
            continue;
        }

        if (!loader.hasNext()) {
            spdlog::info("ScenePlayer: looping");
            loader.reset();
            video.reset();
            tracker    = KalmanTracker();
            frame_idx_ = 0;
            last_good_frame = cv::Mat();
            continue;
        }

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

        auto detections = loader.nextFrame();
        auto fused      = tracker.update(detections, dt);

        SystemSnapshot snapshot;
        snapshot.timestamp_ms   = ts_ms;
        snapshot.raw_detections = detections;
        snapshot.fused_objects  = fused;
        classifier.classify(snapshot);

        // ── Broadcast to dashboard ─────────────────────────────
        bridge.broadcast(snapshot, frame_idx_);

        // ── Render OpenCV window ───────────────────────────────
        int key = viz.render(video_frame, snapshot,
                             cfg_.name, frame_idx_,
                             total_frames, state_);
        handleKey(key);

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
        case 's': case 'S': state_ = PlaybackState::STEPPING; break;
        case 'f': case 'F':
            state_ = (state_ == PlaybackState::FAST)
                     ? PlaybackState::PLAYING : PlaybackState::FAST;
            break;
        case 'w': case 'W':
            state_ = (state_ == PlaybackState::SLOW)
                     ? PlaybackState::PLAYING : PlaybackState::SLOW;
            break;
        case 'r': case 'R': state_ = PlaybackState::PLAYING; break;
        case 'q': case 'Q': case 27: cv::destroyAllWindows(); break;
        default: break;
    }
}

int ScenePlayer::frameDelayMs() const {
    switch (state_) {
        case PlaybackState::FAST: return 11;
        case PlaybackState::SLOW: return 200;
        default:                  return 100;
    }
}

} // namespace adas
