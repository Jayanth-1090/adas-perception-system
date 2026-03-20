#include "simulator/DetectionLoader.hpp"
#include "fusion/KalmanTracker.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void runScene(const std::string& path, const std::string& name) {
    spdlog::info("========================================");
    spdlog::info("Scene: {}", name);
    spdlog::info("========================================");

    adas::DetectionLoader loader(path);
    adas::KalmanTracker   tracker;

    const float dt = 0.1f;
    int frame = 0;
    int total_confirmed = 0;
    uint32_t max_simultaneous = 0;

    while (loader.hasNext()) {
        auto detections = loader.nextFrame();
        auto fused      = tracker.update(detections, dt);

        total_confirmed += static_cast<int>(fused.size());
        max_simultaneous = std::max(max_simultaneous,
                                    static_cast<uint32_t>(fused.size()));
        ++frame;
    }

    spdlog::info("Frames processed   : {}", frame);
    spdlog::info("Total track outputs : {}", total_confirmed);
    spdlog::info("Max simultaneous    : {}", max_simultaneous);
    spdlog::info("Active tracks end   : {}", tracker.tracks().size());
    spdlog::info("");
}

int main() {
    auto logger = spdlog::stdout_color_mt("adas");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    try {
        runScene("data/detections/scene_day.json",   "DAY   (nD_1.mp4)");
        runScene("data/detections/scene_rain.json",  "RAIN  (rD_2.mp4)");
        runScene("data/detections/scene_night.json", "NIGHT (nN_1.mp4)");
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
    return 0;
}
