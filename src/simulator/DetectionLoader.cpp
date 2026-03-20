#include "simulator/DetectionLoader.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace adas {

// ── Constructor ────────────────────────────────────────────────
// Opens the JSON file, validates structure, parses metadata,
// loads all frames into memory. We load everything upfront
// because these files are small (~1-5MB) and random access
// during playback would be slower.
DetectionLoader::DetectionLoader(const std::string& json_path)
    : cursor_(0)
{
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("DetectionLoader: cannot open file: " + json_path);
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("DetectionLoader: JSON parse error in " +
                                 json_path + ": " + e.what());
    }

    // ── Parse metadata ─────────────────────────────────────────
    const auto& m = root.at("meta");
    meta_.source_video   = m.at("source_video").get<std::string>();
    meta_.scene          = m.at("scene").get<std::string>();
    meta_.video_fps      = m.at("video_fps").get<double>();
    meta_.target_fps     = m.at("target_fps").get<int>();
    meta_.frame_width    = m.at("frame_width").get<int>();
    meta_.frame_height   = m.at("frame_height").get<int>();
    meta_.model          = m.at("model").get<std::string>();
    meta_.conf_threshold = m.at("conf_threshold").get<double>();
    meta_.extracted_at   = m.at("extracted_at").get<std::string>();

    // ── Load frames array ──────────────────────────────────────
    if (!root.contains("frames") || !root["frames"].is_array()) {
        throw std::runtime_error("DetectionLoader: missing or invalid 'frames' array");
    }
    frames_ = std::move(root["frames"]);

    spdlog::info("DetectionLoader: loaded '{}' | scene={} | frames={} | model={}",
                 meta_.source_video,
                 meta_.scene,
                 frames_.size(),
                 meta_.model);
}

// ── hasNext ────────────────────────────────────────────────────
bool DetectionLoader::hasNext() const {
    return cursor_ < frames_.size();
}

// ── nextFrame ──────────────────────────────────────────────────
// Returns all detections for the current frame and advances cursor.
// Returns empty vector if no detections in this frame (valid case —
// some frames have zero objects detected).
std::vector<Detection> DetectionLoader::nextFrame() {
    if (!hasNext()) {
        return {};
    }

    const auto& frame = frames_[cursor_];
    std::vector<Detection> detections;

    if (frame.contains("detections") && frame["detections"].is_array()) {
        for (const auto& d : frame["detections"]) {
            try {
                detections.push_back(parseDetection(d));
            } catch (const std::exception& e) {
                // Skip malformed detection, log and continue —
                // never crash the pipeline on a single bad detection
                spdlog::warn("DetectionLoader: skipping malformed detection at "
                             "frame {}: {}", cursor_, e.what());
            }
        }
    }

    ++cursor_;
    return detections;
}

// ── peekTimestamp ──────────────────────────────────────────────
int64_t DetectionLoader::peekTimestamp() const {
    if (!hasNext()) return -1;
    return frames_[cursor_].value("timestamp_ms", int64_t{-1});
}

// ── reset ──────────────────────────────────────────────────────
void DetectionLoader::reset() {
    cursor_ = 0;
    spdlog::debug("DetectionLoader: reset to frame 0");
}

// ── totalFrames ────────────────────────────────────────────────
size_t DetectionLoader::totalFrames() const {
    return frames_.size();
}

// ── currentIndex ───────────────────────────────────────────────
size_t DetectionLoader::currentIndex() const {
    return cursor_;
}

// ── parseDetection ─────────────────────────────────────────────
// Converts one JSON detection object into a Detection struct.
// Uses .at() for required fields (throws if missing) and
// .value() for optional fields (returns default if missing).
Detection DetectionLoader::parseDetection(const nlohmann::json& j) const {
    Detection d;

    d.id         = j.at("id").get<uint32_t>();
    d.label      = j.at("label").get<std::string>();
    d.confidence = j.at("confidence").get<float>();
    d.sensor     = sensorFromString(j.value("sensor", "CAMERA_FRONT"));

    // World-space position from extractor's monocular estimate
    // x = lateral offset (m), y = estimated distance (m)
    d.x = j.value("lateral_offset_m", 0.0f);
    d.y = j.value("estimated_distance_m", 0.0f);

    // Velocity is unknown at detection stage — set to 0
    // The Kalman tracker will estimate velocity from successive frames
    d.vx = 0.0f;
    d.vy = 0.0f;

    // Store original pixel bbox for accurate video overlay
    if (j.contains("bbox_px") && j["bbox_px"].is_array() &&
        j["bbox_px"].size() == 4) {
        d.bbox_x1 = j["bbox_px"][0].get<int>();
        d.bbox_y1 = j["bbox_px"][1].get<int>();
        d.bbox_x2 = j["bbox_px"][2].get<int>();
        d.bbox_y2 = j["bbox_px"][3].get<int>();
    }
    d.src_frame_w = meta_.frame_width;
    d.src_frame_h = meta_.frame_height;

    // Physical size — convert pixel dimensions to approximate meters
    // using the same pinhole model: size_m = (size_px / height_px) * distance_m
    float height_px = j.value("height_px", 1.0f);
    float width_px  = j.value("width_px",  1.0f);
    float dist_m    = d.y;

    if (height_px > 0 && dist_m > 0) {
        float focal_px = static_cast<float>(meta_.frame_height) * 1.2f;
        d.length = (height_px * dist_m) / focal_px;
        d.width  = (width_px  * dist_m) / focal_px;
    } else {
        d.width  = 1.0f;
        d.length = 1.0f;
    }

    return d;
}

// ── sensorFromString ───────────────────────────────────────────
SensorType DetectionLoader::sensorFromString(const std::string& s) {
    if (s == "RADAR_FRONT") return SensorType::RADAR;
    if (s == "ULTRASONIC")  return SensorType::ULTRASONIC;
    return SensorType::CAMERA;  // default — CAMERA_FRONT etc.
}

} // namespace adas
