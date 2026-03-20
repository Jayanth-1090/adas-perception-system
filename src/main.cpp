#include "visualizer/ScenePlayer.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char** argv) {
    auto logger = spdlog::stdout_color_mt("adas");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("ADAS Perception Simulator");
    spdlog::info("Controls: SPACE=Pause  S=Step  F=Fast  W=Slow  R=Reset  Q=Quit");

    // Default to day scene — pass arg to select
    // 0=day, 1=rain, 2=night
    int scene_idx = 0;
    if (argc > 1) scene_idx = std::atoi(argv[1]);

    const std::vector<adas::SceneConfig> scenes = {
        { "DAY",   "data/videos/nD_1.mp4", "data/detections/scene_day.json"   },
        { "RAIN",  "data/videos/rD_2.mp4", "data/detections/scene_rain.json"  },
        { "NIGHT", "data/videos/nN_1.mp4", "data/detections/scene_night.json" },
    };

    scene_idx = std::max(0, std::min(scene_idx,
                         static_cast<int>(scenes.size()) - 1));

    spdlog::info("Loading scene: {}", scenes[scene_idx].name);

    adas::ThreatConfig threat_cfg;
    threat_cfg.ttc_warning_s  = 4.0f;
    threat_cfg.ttc_critical_s = 2.0f;
    threat_cfg.lat_warning_m  = 1.5f;
    threat_cfg.lat_critical_m = 0.8f;

    try {
        adas::ScenePlayer player(scenes[scene_idx], threat_cfg);
        player.run();
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
