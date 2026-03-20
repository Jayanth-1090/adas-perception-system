#pragma once
#include "common/Types.hpp"
#include "simulator/DetectionLoader.hpp"
#include "simulator/VideoReader.hpp"
#include "fusion/KalmanTracker.hpp"
#include "threat/ThreatClassifier.hpp"
#include "visualizer/Visualizer.hpp"
#include <string>
#include <memory>

namespace adas {

struct SceneConfig {
    std::string name;
    std::string video_path;
    std::string detection_path;
};

class ScenePlayer {
public:
    ScenePlayer(const SceneConfig& cfg,
                const ThreatConfig& threat_cfg = {});

    // Run the full playback loop — blocks until quit
    void run();

private:
    SceneConfig      cfg_;
    ThreatConfig     threat_cfg_;

    PlaybackState    state_ = PlaybackState::PLAYING;
    int              frame_idx_ = 0;

    void handleKey(int key);
    int  frameDelayMs() const;
};

} // namespace adas
