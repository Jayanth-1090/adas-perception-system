#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace adas {

// ── Sensor Types ───────────────────────────────────────────────
enum class SensorType {
    CAMERA,
    RADAR,
    ULTRASONIC
};

// Raw detection from a single sensor
struct Detection {
    uint32_t    id;          // unique detection ID
    SensorType  sensor;      // which sensor produced this
    float       x;           // lateral position (m), 0 = ego center
    float       y;           // longitudinal distance (m), 0 = ego front
    float       vx;          // lateral velocity (m/s)
    float       vy;          // longitudinal velocity (m/s)
    float       width;       // object width (m)
    float       length;      // object length (m)
    float       confidence;  // 0.0 - 1.0
    std::string label;       // "pedestrian", "vehicle", "obstacle"
    // Original pixel bbox from extractor (for accurate overlay)
    int bbox_x1 = 0, bbox_y1 = 0;  // top-left
    int bbox_x2 = 0, bbox_y2 = 0;  // bottom-right
    int src_frame_w = 1920;         // source video dimensions
    int src_frame_h = 1080;
};

// Fused object after sensor fusion
struct FusedObject {
    uint32_t             track_id;
    float                x, y;
    float                vx, vy;
    float                width, length;
    float                confidence;
    std::string          label;
    std::vector<SensorType> contributing_sensors;
};

// Threat level enum
enum class ThreatLevel {
    SAFE,
    WARNING,
    CRITICAL
};

// Per-object threat assessment
struct ThreatAssessment {
    uint32_t    track_id;
    ThreatLevel level;
    float       ttc;         // Time-To-Collision (seconds), -1 if not applicable
    std::string reason;      // human-readable: "TTC < 2.0s", "OSE zone breach"
};

// Full system snapshot — this is what gets sent to the dashboard
struct SystemSnapshot {
    int64_t                      timestamp_ms;
    std::vector<Detection>       raw_detections;
    std::vector<FusedObject>     fused_objects;
    std::vector<ThreatAssessment> threats;
    std::string                  overall_threat; // "SAFE" | "WARNING" | "CRITICAL"
};

} // namespace adas

namespace adas {

// ── Lane Detection Result ──────────────────────────────────────
struct LanePoint {
    float x;  // lateral position (m)
    float y;  // longitudinal position (m)
};

struct LaneResult {
    std::vector<LanePoint> left_lane;
    std::vector<LanePoint> right_lane;
    float left_offset_m;    // ego distance from left lane (m)
    float right_offset_m;   // ego distance from right lane (m)
    float heading_deg;      // ego heading relative to lane center
    bool  detected;         // whether lanes were found this frame
};

// ── Playback State ─────────────────────────────────────────────
enum class PlaybackState {
    PLAYING,
    PAUSED,
    STEPPING,   // advance one frame then pause
    FAST,       // 3x speed
    SLOW,       // 0.5x speed
};

} // namespace adas
