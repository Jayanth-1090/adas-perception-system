#include "bridge/SnapshotSerializer.hpp"

namespace adas {

nlohmann::json SnapshotSerializer::toJson(const SystemSnapshot& snap) {
    nlohmann::json j;

    j["timestamp_ms"]   = snap.timestamp_ms;
    j["overall_threat"] = snap.overall_threat;

    // Raw detections
    nlohmann::json dets = nlohmann::json::array();
    for (const auto& d : snap.raw_detections)
        dets.push_back(serializeDetection(d));
    j["raw_detections"] = dets;

    // Fused tracked objects
    nlohmann::json fused = nlohmann::json::array();
    for (const auto& o : snap.fused_objects)
        fused.push_back(serializeFusedObject(o));
    j["fused_objects"] = fused;

    // Threat assessments
    nlohmann::json threats = nlohmann::json::array();
    for (const auto& t : snap.threats)
        threats.push_back(serializeThreat(t));
    j["threats"] = threats;

    return j;
}

nlohmann::json SnapshotSerializer::serializeDetection(const Detection& d) {
    // Send only what the dashboard overlay needs — keep payload small
    return {
        {"id",          d.id},
        {"label",       d.label},
        {"confidence",  d.confidence},
        {"x",           d.x},
        {"y",           d.y},
        {"bbox_x1",     d.bbox_x1},
        {"bbox_y1",     d.bbox_y1},
        {"bbox_x2",     d.bbox_x2},
        {"bbox_y2",     d.bbox_y2},
        {"src_frame_w", d.src_frame_w},
        {"src_frame_h", d.src_frame_h},
    };
}

nlohmann::json SnapshotSerializer::serializeFusedObject(const FusedObject& o) {
    nlohmann::json sensors = nlohmann::json::array();
    for (const auto& s : o.contributing_sensors)
        sensors.push_back(sensorToString(s));

    return {
        {"track_id",             o.track_id},
        {"label",                o.label},
        {"confidence",           o.confidence},
        {"x",                    o.x},
        {"y",                    o.y},
        {"vx",                   o.vx},
        {"vy",                   o.vy},
        {"width",                o.width},
        {"length",               o.length},
        {"contributing_sensors", sensors},
    };
}

nlohmann::json SnapshotSerializer::serializeThreat(const ThreatAssessment& t) {
    return {
        {"track_id", t.track_id},
        {"level",    threatLevelToString(t.level)},
        {"ttc",      t.ttc},
        {"reason",   t.reason},
    };
}

std::string SnapshotSerializer::sensorToString(SensorType s) {
    switch (s) {
        case SensorType::RADAR:      return "RADAR";
        case SensorType::ULTRASONIC: return "ULTRASONIC";
        default:                     return "CAMERA";
    }
}

std::string SnapshotSerializer::threatLevelToString(ThreatLevel l) {
    switch (l) {
        case ThreatLevel::CRITICAL: return "CRITICAL";
        case ThreatLevel::WARNING:  return "WARNING";
        default:                    return "SAFE";
    }
}

} // namespace adas
