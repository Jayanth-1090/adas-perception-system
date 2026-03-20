#include "visualizer/ScenePlayer.hpp"
#include <opencv2/highgui.hpp>
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace adas {

ScenePlayer::ScenePlayer(const SceneConfig& cfg,
                         const ThreatConfig& threat_cfg)
    : cfg_(cfg), threat_cfg_(threat_cfg) {}

void ScenePlayer::run() {
    DetectionLoader  loader(cfg_.detection_path);
    VideoReader      video(cfg_.video_path);
    KalmanTracker    tracker;
    ThreatClassifier classifier(threat_cfg_);
    Visualizer       viz(1600, 900);

    const int   total_frames = static_cast<int>(loader.totalFrames());
    const float dt           = 1.0f / loader.meta().target_fps;

    // Pre-load first valid frame as fallback
    cv::Mat last_good_frame;

    spdlog::info("ScenePlayer: starting '{}' | {} frames | dt={:.3f}s",
                 cfg_.name, total_frames, dt);

    while (!viz.windowClosed()) {

        // ── PAUSED ─────────────────────────────────────────────
        if (state_ == PlaybackState::PAUSED) {
            int key = cv::waitKey(30);
            handleKey(key);
            continue;
        }

        // ── End of scene — loop ────────────────────────────────
        if (!loader.hasNext()) {
            spdlog::info("ScenePlayer: scene complete, looping");
            loader.reset();
            video.reset();
            tracker    = KalmanTracker();
            frame_idx_ = 0;
            last_good_frame = cv::Mat();
            continue;
        }

        // ── Get timestamp BEFORE advancing loader ──────────────
        int64_t ts_ms = loader.peekTimestamp();

        // ── Seek video to this timestamp ───────────────────────
        cv::Mat video_frame;
        bool got_frame = video.getFrameAt(ts_ms, video_frame);

        // Use last good frame if seek failed
        if (!got_frame || video_frame.empty() ||
            video_frame.rows <= 0 || video_frame.cols <= 0) {
            if (!last_good_frame.empty())
                video_frame = last_good_frame.clone();
        } else {
            last_good_frame = video_frame.clone();
        }

        // ── Advance detection loader + run pipeline ────────────
        auto detections = loader.nextFrame();
        auto fused      = tracker.update(detections, dt);

        // ── Build snapshot ─────────────────────────────────────
        SystemSnapshot snapshot;
        snapshot.timestamp_ms   = ts_ms;
        snapshot.raw_detections = detections;
        snapshot.fused_objects  = fused;
        classifier.classify(snapshot);

        // ── Render ─────────────────────────────────────────────
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

    spdlog::info("ScenePlayer: exited");
}

void ScenePlayer::handleKey(int key) {
    if (key == -1) return;
    switch (key & 0xFF) {
        case ' ':
            state_ = (state_ == PlaybackState::PAUSED)
                     ? PlaybackState::PLAYING
                     : PlaybackState::PAUSED;
            spdlog::info("Playback: {}",
                state_ == PlaybackState::PAUSED ? "PAUSED" : "PLAYING");
            break;
        case 's': case 'S':
            state_ = PlaybackState::STEPPING;
            break;
        case 'f': case 'F':
            state_ = (state_ == PlaybackState::FAST)
                     ? PlaybackState::PLAYING : PlaybackState::FAST;
            spdlog::info("Playback: FAST");
            break;
        case 'w': case 'W':
            state_ = (state_ == PlaybackState::SLOW)
                     ? PlaybackState::PLAYING : PlaybackState::SLOW;
            spdlog::info("Playback: SLOW");
            break;
        case 'r': case 'R':
            spdlog::info("Playback: RESET");
            state_ = PlaybackState::PLAYING;
            break;
        case 'q': case 'Q': case 27:
            cv::destroyAllWindows();
            break;
        default: break;
    }
}

int ScenePlayer::frameDelayMs() const {
    switch (state_) {
        case PlaybackState::FAST: return 33 / 3;
        case PlaybackState::SLOW: return 200;
        default:                  return 100;
    }
}

} // namespace adas
