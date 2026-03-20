#include "threat/ThreatClassifier.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace adas {

ThreatClassifier::ThreatClassifier(const ThreatConfig& cfg) : cfg_(cfg) {}

void ThreatClassifier::classify(SystemSnapshot& snapshot) {
    snapshot.threats.clear();

    ThreatLevel overall = ThreatLevel::SAFE;

    for (const auto& obj : snapshot.fused_objects) {
        auto assessment = assessObject(obj);
        snapshot.threats.push_back(assessment);

        // Overall = worst individual threat
        if (assessment.level == ThreatLevel::CRITICAL)
            overall = ThreatLevel::CRITICAL;
        else if (assessment.level == ThreatLevel::WARNING &&
                 overall != ThreatLevel::CRITICAL)
            overall = ThreatLevel::WARNING;
    }

    snapshot.overall_threat = levelToString(overall);
}

ThreatAssessment ThreatClassifier::assessObject(const FusedObject& obj) const {
    ThreatAssessment a;
    a.track_id = obj.track_id;
    a.level    = ThreatLevel::SAFE;
    a.ttc      = -1.0f;
    a.reason   = "safe";

    // ── TTC (Time-To-Collision) ────────────────────────────────
    // vy < 0 means object is approaching (closing velocity)
    // TTC = distance / closing_speed
    if (obj.vy < -1.5f) {   // approaching faster than 1.5 m/s
        a.ttc = std::abs(obj.y / obj.vy);

        if (a.ttc < cfg_.ttc_critical_s) {
            a.level  = ThreatLevel::CRITICAL;
            a.reason = "TTC=" + std::to_string(a.ttc).substr(0,4) + "s CRITICAL";
        } else if (a.ttc < cfg_.ttc_warning_s) {
            a.level  = ThreatLevel::WARNING;
            a.reason = "TTC=" + std::to_string(a.ttc).substr(0,4) + "s WARNING";
        }
    }

    // ── OSE (Object in Safe Envelope) ─────────────────────────
    // Lateral distance check — regardless of TTC
    float lat = std::abs(obj.x);

    if (lat < cfg_.lat_critical_m && obj.y < 12.0f) {
        // Only critical if also close in distance
        if (a.level != ThreatLevel::CRITICAL) {
            a.level  = ThreatLevel::CRITICAL;
            a.reason = "OSE breach lat=" +
                       std::to_string(lat).substr(0,4) + "m CRITICAL";
        }
    } else if (lat < cfg_.lat_warning_m && obj.y < 20.0f) {
        if (a.level == ThreatLevel::SAFE) {
            a.level  = ThreatLevel::WARNING;
            a.reason = "OSE lat=" +
                       std::to_string(lat).substr(0,4) + "m WARNING";
        }
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
