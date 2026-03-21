#pragma once
#include "common/Types.hpp"
#include <nlohmann/json.hpp>

namespace adas {

// Converts SystemSnapshot → nlohmann::json
// This is the wire format sent to the Next.js dashboard
class SnapshotSerializer {
public:
    static nlohmann::json toJson(const SystemSnapshot& snapshot);

private:
    static nlohmann::json serializeDetection(const Detection& d);
    static nlohmann::json serializeFusedObject(const FusedObject& o);
    static nlohmann::json serializeThreat(const ThreatAssessment& t);
    static std::string    sensorToString(SensorType s);
    static std::string    threatLevelToString(ThreatLevel l);
};

} // namespace adas
