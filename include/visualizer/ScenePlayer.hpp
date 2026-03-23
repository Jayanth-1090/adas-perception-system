#pragma once
#include "common/Types.hpp"
#include "simulator/DetectionLoader.hpp"
#include "simulator/VideoReader.hpp"
#include "fusion/KalmanTracker.hpp"
#include "threat/ThreatClassifier.hpp"
#include "visualizer/Visualizer.hpp"
#include "bridge/WebSocketBridge.hpp"
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
    ScenePlayer(const std::vector<SceneConfig>& scenes,
                const ThreatConfig& threat_cfg = {},
                int ws_port = 9002,
                bool headless = false);

    void run();

private:
    std::vector<SceneConfig> scenes_;
    int               current_scene_idx_ = 0;
    ThreatConfig     threat_cfg_;
    int              ws_port_;
    bool             headless_;
    PlaybackState    state_ = PlaybackState::PLAYING;
    int              frame_idx_ = 0;

    void handleKey(int key);
    int  frameDelayMs() const;
};

} // namespace adas
// headless mode added below
