cmake_minimum_required(VERSION 3.20)
project(ARXVisionEngine VERSION 2.0.0 LANGUAGES CXX)

# ─────────────────────────────────────────────────────────────────────────────
# Compiler Standards & Global Flags
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Release-mode optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -flto")
set(CMAKE_CXX_FLAGS_DEBUG   "-g3 -fsanitize=address,undefined")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Optional: CUDA Acceleration
# ─────────────────────────────────────────────────────────────────────────────
option(ENABLE_CUDA "Enable CUDA GPU acceleration" OFF)
option(ENABLE_ZEROMQ "Enable ZeroMQ inter-process messaging" ON)

if(ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDA REQUIRED)
    add_compile_definitions(ARX_CUDA_ENABLED)
    message(STATUS "CUDA acceleration: ENABLED")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Dependencies
# ─────────────────────────────────────────────────────────────────────────────
find_package(OpenCV 4.8 REQUIRED COMPONENTS core highgui imgproc videoio)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)

if(ENABLE_ZEROMQ)
    find_package(ZeroMQ REQUIRED)
    find_package(cppzmq REQUIRED)
    add_compile_definitions(ARX_ZEROMQ_ENABLED)
    message(STATUS "ZeroMQ IPC: ENABLED")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Source Files
# ─────────────────────────────────────────────────────────────────────────────
set(ARX_SOURCES
    src/main.cpp
    src/camera_manager.cpp
    src/frame_pipeline.cpp
    src/render_engine.cpp
    src/shared_memory_bridge.cpp
    src/telemetry_encoder.cpp
)

add_executable(arx_vision_engine ${ARX_SOURCES})

target_include_directories(arx_vision_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(arx_vision_engine PRIVATE
    Threads::Threads
    ${OpenCV_LIBS}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

if(ENABLE_ZEROMQ)
    target_link_libraries(arx_vision_engine PRIVATE
        libzmq
        cppzmq
    )
endif()

if(ENABLE_CUDA)
    target_link_libraries(arx_vision_engine PRIVATE ${CUDA_LIBRARIES})
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Compiler Warnings
# ─────────────────────────────────────────────────────────────────────────────
target_compile_options(arx_vision_engine PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -ffast-math
)

# ─────────────────────────────────────────────────────────────────────────────
# Install Targets
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS arx_vision_engine DESTINATION bin)

message(STATUS "ARX Vision Engine ${PROJECT_VERSION} configured")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenCV:      ${OpenCV_VERSION}")
