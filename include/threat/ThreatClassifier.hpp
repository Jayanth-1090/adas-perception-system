#pragma once
#include "common/Types.hpp"
#include <vector>

namespace adas {

struct ThreatConfig {
    float ttc_warning_s  = 4.0f;
    float ttc_critical_s = 2.0f;
    float lat_warning_m  = 1.5f;
    float lat_critical_m = 0.8f;
};

class ThreatClassifier {
public:
    explicit ThreatClassifier(const ThreatConfig& cfg = {});

    // Classify each fused object — returns per-object assessments
    // and sets overall_threat on the snapshot
    void classify(SystemSnapshot& snapshot);

private:
    ThreatConfig cfg_;

    ThreatAssessment assessObject(const FusedObject& obj) const;
    static std::string levelToString(ThreatLevel l);
};

} // namespace adas
