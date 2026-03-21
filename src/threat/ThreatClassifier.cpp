#include "threat/ThreatClassifier.hpp"
#include <cmath>
#include <string>

namespace adas {

static constexpr float EGO_LANE_W  = 0.8f;
static constexpr float WARN_ZONE_W = 1.6f;
static constexpr float MAX_DIST    = 30.0f;
static constexpr float MIN_VY      = 2.0f;

ThreatClassifier::ThreatClassifier(const ThreatConfig& cfg) : cfg_(cfg) {}

void ThreatClassifier::classify(SystemSnapshot& snapshot) {
    snapshot.threats.clear();
    ThreatLevel overall = ThreatLevel::SAFE;
    for (const auto& obj : snapshot.fused_objects) {
        auto a = assessObject(obj);
        snapshot.threats.push_back(a);
        if (a.level == ThreatLevel::CRITICAL)
            overall = ThreatLevel::CRITICAL;
        else if (a.level == ThreatLevel::WARNING &&
                 overall != ThreatLevel::CRITICAL)
            overall = ThreatLevel::WARNING;
    }
    snapshot.overall_threat = levelToString(overall);
}

ThreatAssessment ThreatClassifier::assessObject(
    const FusedObject& obj) const
{
    ThreatAssessment a;
    a.track_id = obj.track_id;
    a.level    = ThreatLevel::SAFE;
    a.ttc      = -1.0f;
    a.reason   = "safe";

    float lat  = std::abs(obj.x);
    float dist = obj.y;

    if (dist <= 0.f || dist > MAX_DIST) return a;
    if (lat > 2.5f) return a;

    bool approaching = obj.vy < -MIN_VY;

    if (lat < EGO_LANE_W && approaching) {
        a.ttc = dist / std::abs(obj.vy);
        if (a.ttc < cfg_.ttc_critical_s) {
            a.level  = ThreatLevel::CRITICAL;
            a.reason = "FCW:CRITICAL TTC=" + std::to_string(a.ttc).substr(0,4) + "s";
        } else if (a.ttc < cfg_.ttc_warning_s) {
            a.level  = ThreatLevel::WARNING;
            a.reason = "FCW:WARNING TTC=" + std::to_string(a.ttc).substr(0,4) + "s";
        }
        return a;
    }

    if (lat < EGO_LANE_W && dist < 4.f) {
        a.level  = ThreatLevel::CRITICAL;
        a.reason = "FCW:imminent";
        return a;
    }

    if (lat < WARN_ZONE_W && approaching && dist < 20.f) {
        a.ttc = dist / std::abs(obj.vy);
        if (a.ttc < cfg_.ttc_warning_s) {
            a.level  = ThreatLevel::WARNING;
            a.reason = "LCW:WARNING";
        }
        return a;
    }

    return a;
}

std::string ThreatClassifier::levelToString(ThreatLevel l) {
    switch (l) {
        case ThreatLevel::CRITICAL: return "CRITICAL";
        case ThreatLevel::WARNING:  return "WARNING";
        default:                    return "SAFE";
    }
}

} // namespace adas
