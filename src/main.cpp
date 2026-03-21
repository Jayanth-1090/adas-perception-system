#include "visualizer/ScenePlayer.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main(int argc, char** argv) {
    auto logger = spdlog::stdout_color_mt("adas");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::info);

    spdlog::info("ADAS Perception Simulator");
    spdlog::info("Controls: SPACE=Pause S=Step F=Fast W=Slow R=Reset Q=Quit");

    int scene_idx = (argc > 1) ? std::atoi(argv[1]) : 0;
    int ws_port   = (argc > 2) ? std::atoi(argv[2]) : 9002;
    bool headless = false;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--headless") headless = true;

    const std::vector<adas::SceneConfig> scenes = {
        {"DAY",   "data/videos/nD_1.mp4", "data/detections/scene_day.json"},
        {"RAIN",  "data/videos/rD_2.mp4", "data/detections/scene_rain.json"},
        {"NIGHT", "data/videos/nN_1.mp4", "data/detections/scene_night.json"},
    };

    scene_idx = std::max(0, std::min(scene_idx,
                         static_cast<int>(scenes.size())-1));

    spdlog::info("Scene: {} | WebSocket port: {}",
                 scenes[scene_idx].name, ws_port);

    adas::ThreatConfig threat_cfg;
    threat_cfg.ttc_warning_s  = 4.0f;
    threat_cfg.ttc_critical_s = 2.0f;
    threat_cfg.lat_warning_m  = 1.5f;
    threat_cfg.lat_critical_m = 0.8f;

    try {
        adas::ScenePlayer player(scenes[scene_idx], threat_cfg, ws_port, headless);
        player.run();
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }
    return 0;
}
