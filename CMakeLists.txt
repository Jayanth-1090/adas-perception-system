cmake_minimum_required(VERSION 3.20)
project(adas_perception_simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(OpenCV REQUIRED)

include_directories(include ${OpenCV_INCLUDE_DIRS})

# ── Simulator ──────────────────────────────────────────────────
add_library(simulator_lib
  src/simulator/DetectionLoader.cpp
  src/simulator/VideoReader.cpp
)
target_link_libraries(simulator_lib
  PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json
          ${OpenCV_LIBS}
)

# ── Fusion ─────────────────────────────────────────────────────
add_library(fusion_lib
  src/fusion/FusionEngine.cpp
  src/fusion/HungarianAssigner.cpp
)
target_link_libraries(fusion_lib
  PRIVATE simulator_lib spdlog::spdlog
)

# ── Threat ─────────────────────────────────────────────────────
add_library(threat_lib
  src/threat/ThreatClassifier.cpp
)
target_link_libraries(threat_lib
  PRIVATE fusion_lib spdlog::spdlog
)

# ── Bridge ─────────────────────────────────────────────────────
add_library(bridge_lib
  src/bridge/WebSocketBridge.cpp
)
target_link_libraries(bridge_lib
  PRIVATE threat_lib Crow::Crow spdlog::spdlog
          nlohmann_json::nlohmann_json
)

# ── Visualizer ─────────────────────────────────────────────────
add_library(visualizer_lib
  src/visualizer/Visualizer.cpp
  src/visualizer/BirdsEyeView.cpp
  src/visualizer/ScenePlayer.cpp
)
target_link_libraries(visualizer_lib
  PRIVATE threat_lib bridge_lib spdlog::spdlog
          nlohmann_json::nlohmann_json ${OpenCV_LIBS}
)

# ── Main ───────────────────────────────────────────────────────
add_executable(adas_simulator src/main.cpp)
target_link_libraries(adas_simulator
  PRIVATE visualizer_lib bridge_lib threat_lib
          fusion_lib simulator_lib
          spdlog::spdlog nlohmann_json::nlohmann_json
          Crow::Crow ${OpenCV_LIBS}
)
