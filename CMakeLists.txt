cmake_minimum_required(VERSION 3.20)
project(adas_perception_simulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg toolchain handles find_package
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Crow CONFIG REQUIRED)

# Common include path
include_directories(include)

# ── Simulator library ──────────────────────────────────────────
add_library(simulator_lib
  src/simulator/DetectionLoader.cpp
)
target_link_libraries(simulator_lib
  PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json
)

# ── Fusion library ─────────────────────────────────────────────
add_library(fusion_lib
  src/fusion/FusionEngine.cpp
)
target_link_libraries(fusion_lib
  PRIVATE simulator_lib spdlog::spdlog
)

# ── Threat library ─────────────────────────────────────────────
add_library(threat_lib
  src/threat/ThreatClassifier.cpp
)
target_link_libraries(threat_lib
  PRIVATE fusion_lib spdlog::spdlog
)

# ── WebSocket bridge ───────────────────────────────────────────
add_library(bridge_lib
  src/bridge/WebSocketBridge.cpp
)
target_link_libraries(bridge_lib
  PRIVATE threat_lib Crow::Crow spdlog::spdlog nlohmann_json::nlohmann_json
)

# ── Main executable ────────────────────────────────────────────
add_executable(adas_simulator src/main.cpp)
target_link_libraries(adas_simulator
  PRIVATE bridge_lib threat_lib fusion_lib simulator_lib
  spdlog::spdlog nlohmann_json::nlohmann_json Crow::Crow
)
