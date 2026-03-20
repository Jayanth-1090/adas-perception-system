#include "simulator/DetectionLoader.hpp"
#include <spdlog/spdlog.h>
#include <iostream>

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::info("ADAS Perception Simulator starting...");

    try {
        // Load the day scene
        adas::DetectionLoader loader("data/detections/scene_day.json");

        spdlog::info("Scene    : {}", loader.meta().scene);
        spdlog::info("Source   : {}", loader.meta().source_video);
        spdlog::info("Frames   : {}", loader.totalFrames());
        spdlog::info("Model    : {}", loader.meta().model);

        // Walk through first 5 frames
        int frame_count = 0;
        while (loader.hasNext() && frame_count < 5) {
            int64_t ts = loader.peekTimestamp();
            auto detections = loader.nextFrame();

            spdlog::info("Frame {:3d} | t={}ms | detections={}",
                         frame_count, ts, detections.size());

            for (const auto& d : detections) {
                spdlog::info("  [{:>12s}] id={:4d} | dist={:5.1f}m | lat={:+.2f}m | conf={:.2f}",
                             d.label, d.id, d.y, d.x, d.confidence);
            }
            ++frame_count;
        }

        spdlog::info("DetectionLoader OK");

    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
