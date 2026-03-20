#pragma once

#include "common/Types.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

namespace adas {

struct SceneMeta {
    std::string source_video;
    std::string scene;          // "day" | "rain" | "night"
    double      video_fps;
    int         target_fps;
    int         frame_width;
    int         frame_height;
    std::string model;
    double      conf_threshold;
    std::string extracted_at;
};

class DetectionLoader {
public:
    // Loads and parses the entire JSON file into memory
    explicit DetectionLoader(const std::string& json_path);

    // Returns true if more frames are available
    bool hasNext() const;

    // Returns detections for the next frame and advances the cursor
    std::vector<Detection> nextFrame();

    // Peek at timestamp of next frame without advancing
    int64_t peekTimestamp() const;

    // Reset playback to beginning
    void reset();

    // Total number of sampled frames in this scene
    size_t totalFrames() const;

    // Current frame index (0-based)
    size_t currentIndex() const;

    const SceneMeta& meta() const { return meta_; }

private:
    SceneMeta               meta_;
    nlohmann::json          frames_;   // raw JSON array of frames
    size_t                  cursor_;   // current position in frames_

    // Converts a single JSON detection object → Detection struct
    Detection parseDetection(const nlohmann::json& j) const;

    // Converts scene string → SensorType
    static SensorType sensorFromString(const std::string& s);
};

} // namespace adas
