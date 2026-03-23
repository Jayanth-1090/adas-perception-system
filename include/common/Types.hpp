#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace adas {

enum class SensorType { CAMERA, RADAR, ULTRASONIC };
enum class ThreatLevel { SAFE, WARNING, CRITICAL };
enum class PlaybackState { PLAYING, PAUSED, STEPPING, FAST, SLOW };

struct Detection {
    uint32_t    id;
    SensorType  sensor;
    float       x, y, vx, vy, width, length, confidence;
    std::string label;
    int bbox_x1 = 0, bbox_y1 = 0;
    int bbox_x2 = 0, bbox_y2 = 0;
    int src_frame_w = 1920;
    int src_frame_h = 1080;
};

struct FusedObject {
    uint32_t    track_id;
    float       x, y, vx, vy, width, length, confidence;
    std::string label;
    std::vector<SensorType> contributing_sensors;
};

struct ThreatAssessment {
    uint32_t    track_id;
    ThreatLevel level;
    float       ttc;
    std::string reason;
};

struct SystemSnapshot {
    int64_t                       timestamp_ms;
    std::vector<Detection>        raw_detections;
    std::vector<FusedObject>      fused_objects;
    std::vector<ThreatAssessment> threats;
    std::string                   overall_threat;
};

} // namespace adas
