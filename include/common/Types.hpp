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
