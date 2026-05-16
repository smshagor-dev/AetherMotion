cmake_minimum_required(VERSION 3.24)
project(ARXPlatform VERSION 3.0.0 LANGUAGES CXX)

option(ARX_ENABLE_CUDA "Enable CUDA-accelerated runtime stages" OFF)
option(ARX_ENABLE_QT6 "Build the Qt6 dashboard" ON)
option(ARX_ENABLE_PROFILING "Enable real-time profiling hooks" ON)
option(ARX_ENABLE_HEADLESS "Build without local display dependencies" OFF)
option(ARX_ENABLE_ZMQ "Enable ZeroMQ-based remote services" OFF)
option(ARX_ENABLE_REMOTE "Enable optional remote service integration" ON)
option(ARX_ENABLE_MEDIAPIPE "Enable MediaPipe Tasks C++ hand/face tracking" OFF)

set(ARX_MEDIAPIPE_INCLUDE_DIR "" CACHE PATH "MediaPipe include directory")
set(ARX_MEDIAPIPE_LIBRARIES "" CACHE STRING "Semicolon-separated MediaPipe libraries")
set(ARX_MEDIAPIPE_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models" CACHE PATH "Directory containing MediaPipe .task model assets")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

add_library(arx_engine
    engine/core/application.cpp
    engine/threading/thread_pool.cpp
    engine/profiling/frame_profiler.cpp
    engine/profiling/runtime_metrics.cpp
    engine/config/runtime_config.cpp
    engine/diagnostics/health_monitor.cpp
    engine/telemetry/telemetry_encoder.cpp
    engine/runtime/runtime_context.cpp
    apps/runtime_cli.cpp
    vision/tracking/model_asset_validator.cpp
)
target_include_directories(arx_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(arx_engine PUBLIC cxx_std_20)
target_compile_definitions(arx_engine PUBLIC
    $<$<BOOL:${ARX_ENABLE_PROFILING}>:ARX_ENABLE_PROFILING=1>
    $<$<BOOL:${ARX_ENABLE_CUDA}>:ARX_ENABLE_CUDA=1>
    $<$<BOOL:${ARX_ENABLE_ZMQ}>:ARX_ENABLE_ZMQ=1>
    $<$<BOOL:${ARX_ENABLE_REMOTE}>:ARX_ENABLE_REMOTE=1>
)

add_library(arx_vision
    vision/gesture_engine/gesture_engine.cpp
    vision/smoothing/landmark_smoother.cpp
    vision/smoothing/one_euro_filter.cpp
    vision/gesture_engine/gesture_classifier.cpp
    vision/spatial_analysis/spatial_interaction_engine.cpp
)
target_include_directories(arx_vision PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arx_vision PUBLIC arx_engine)

add_library(arx_ar
    ar/scene/scene_graph.cpp
    ar/interactions/interaction_system.cpp
)
target_include_directories(arx_ar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arx_ar PUBLIC arx_engine arx_vision)

add_library(arx_replay
    replay/playback/session_player.cpp
    replay/recorder/session_recorder.cpp
    replay/exporters/jsonl_exporter.cpp
    replay/exporters/csv_exporter.cpp
)
target_include_directories(arx_replay PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(arx_replay PUBLIC arx_engine arx_vision arx_ar)

add_executable(arx_runtime
    apps/arx_runtime_main.cpp
)
target_link_libraries(arx_runtime PRIVATE arx_engine arx_vision arx_ar arx_replay)

find_package(OpenCV QUIET COMPONENTS core imgproc highgui videoio)
set(ARX_MEDIAPIPE_READY OFF)
set(ARX_HAND_MODEL_PATH "${ARX_MEDIAPIPE_MODELS_DIR}/hand_landmarker.task")
set(ARX_FACE_MODEL_PATH "${ARX_MEDIAPIPE_MODELS_DIR}/face_landmarker.task")
if(ARX_ENABLE_MEDIAPIPE)
    if(NOT ARX_MEDIAPIPE_INCLUDE_DIR)
        message(FATAL_ERROR "ARX_ENABLE_MEDIAPIPE=ON but ARX_MEDIAPIPE_INCLUDE_DIR is not set.")
    endif()
    if(NOT EXISTS "${ARX_MEDIAPIPE_INCLUDE_DIR}")
        message(FATAL_ERROR "ARX_ENABLE_MEDIAPIPE=ON but ARX_MEDIAPIPE_INCLUDE_DIR does not exist: ${ARX_MEDIAPIPE_INCLUDE_DIR}")
    endif()
    if(NOT ARX_MEDIAPIPE_LIBRARIES)
        message(FATAL_ERROR "ARX_ENABLE_MEDIAPIPE=ON but ARX_MEDIAPIPE_LIBRARIES is empty.")
    endif()
    foreach(_arx_mp_lib IN LISTS ARX_MEDIAPIPE_LIBRARIES)
        if(_arx_mp_lib MATCHES "[/\\\\]" AND NOT EXISTS "${_arx_mp_lib}")
            message(FATAL_ERROR "ARX_ENABLE_MEDIAPIPE=ON but a configured MediaPipe library does not exist: ${_arx_mp_lib}")
        endif()
    endforeach()
    set(ARX_MEDIAPIPE_READY ON)
endif()
if(ARX_ENABLE_MEDIAPIPE AND NOT OpenCV_FOUND)
    message(FATAL_ERROR "ARX_ENABLE_MEDIAPIPE=ON requires OpenCV (core,imgproc,highgui,videoio) for the current native tracker pipeline.")
endif()
if(OpenCV_FOUND)
    target_compile_definitions(arx_engine PUBLIC ARX_HAS_OPENCV=1)
    if(ARX_MEDIAPIPE_READY)
        target_compile_definitions(arx_engine PUBLIC ARX_HAS_MEDIAPIPE=1)
    endif()
    add_library(arx_runtime_vision
        vision/camera/camera_manager.cpp
        vision/tracking/landmark_runtime_bridge.cpp
        vision/tracking/mediapipe_hand_tracker.cpp
        vision/tracking/mediapipe_face_tracker.cpp
        ar/renderer/render_engine.cpp
    )
    target_include_directories(arx_runtime_vision PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    if(ARX_MEDIAPIPE_READY)
        target_include_directories(arx_runtime_vision PUBLIC ${ARX_MEDIAPIPE_INCLUDE_DIR})
        target_compile_definitions(arx_runtime_vision PUBLIC ARX_HAS_MEDIAPIPE=1)
        target_link_libraries(arx_runtime_vision PUBLIC ${ARX_MEDIAPIPE_LIBRARIES})
    endif()
    target_link_libraries(arx_runtime_vision PUBLIC arx_engine arx_vision arx_ar ${OpenCV_LIBS})
    target_compile_definitions(arx_runtime_vision PUBLIC ARX_HAS_OPENCV=1)
    target_link_libraries(arx_runtime PRIVATE arx_runtime_vision)
endif()

if(ARX_ENABLE_QT6 AND NOT ARX_ENABLE_HEADLESS)
    find_package(Qt6 COMPONENTS Widgets Charts OpenGLWidgets QUIET)
    if(Qt6_FOUND)
        qt_standard_project_setup()
        add_executable(arx_dashboard_qt6
            dashboard/qt6/main.cpp
            dashboard/qt6/dashboard_window.cpp
        )
        target_include_directories(arx_dashboard_qt6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(arx_dashboard_qt6 PRIVATE
            arx_engine arx_vision arx_ar arx_replay
            Qt6::Widgets
        )
        if(TARGET Qt6::Charts)
            target_link_libraries(arx_dashboard_qt6 PRIVATE Qt6::Charts)
        endif()
        if(TARGET Qt6::OpenGLWidgets)
            target_link_libraries(arx_dashboard_qt6 PRIVATE Qt6::OpenGLWidgets)
        endif()
    else()
        message(WARNING "ARX_ENABLE_QT6=ON but Qt6 was not found; skipping arx_dashboard_qt6 target. See docs/INSTALL_QT6_WINDOWS.md or docs/INSTALL_QT6_LINUX.md.")
    endif()
endif()

add_executable(arx_test_filters tests/test_filters.cpp)
target_link_libraries(arx_test_filters PRIVATE arx_vision)
add_test(NAME arx_test_filters COMMAND arx_test_filters)

add_executable(arx_test_gestures tests/test_gestures.cpp)
target_link_libraries(arx_test_gestures PRIVATE arx_engine arx_vision)
add_test(NAME arx_test_gestures COMMAND arx_test_gestures)

add_executable(arx_test_replay tests/test_replay.cpp)
target_link_libraries(arx_test_replay PRIVATE arx_replay arx_vision)
add_test(NAME arx_test_replay COMMAND arx_test_replay)

add_executable(arx_test_runtime tests/test_runtime.cpp)
target_link_libraries(arx_test_runtime PRIVATE arx_engine)
add_test(NAME arx_test_runtime COMMAND arx_test_runtime)

add_executable(arx_test_transport tests/test_transport.cpp)
target_link_libraries(arx_test_transport PRIVATE arx_engine)
add_test(NAME arx_test_transport COMMAND arx_test_transport)

add_executable(arx_test_cli tests/test_cli.cpp)
target_link_libraries(arx_test_cli PRIVATE arx_engine)
add_test(NAME arx_test_cli COMMAND arx_test_cli)

add_executable(arx_test_tracking_setup tests/test_tracking_setup.cpp)
target_link_libraries(arx_test_tracking_setup PRIVATE arx_engine)
add_test(NAME arx_test_tracking_setup COMMAND arx_test_tracking_setup)

message(STATUS "ARX Platform v${PROJECT_VERSION}")
message(STATUS "  CUDA:       ${ARX_ENABLE_CUDA}")
message(STATUS "  Qt6:        ${ARX_ENABLE_QT6}")
message(STATUS "  Profiling:  ${ARX_ENABLE_PROFILING}")
message(STATUS "  Headless:   ${ARX_ENABLE_HEADLESS}")
message(STATUS "  ZMQ:        ${ARX_ENABLE_ZMQ}")
message(STATUS "  Remote:     ${ARX_ENABLE_REMOTE}")
message(STATUS "  MediaPipe:  ${ARX_MEDIAPIPE_READY}")
message(STATUS "  MP incdir:  ${ARX_MEDIAPIPE_INCLUDE_DIR}")
message(STATUS "  MP libs:    ${ARX_MEDIAPIPE_LIBRARIES}")
message(STATUS "  Models dir: ${ARX_MEDIAPIPE_MODELS_DIR}")
message(STATUS "  Hand model: ${ARX_HAND_MODEL_PATH}")
message(STATUS "  Face model: ${ARX_FACE_MODEL_PATH}")
if(Qt6_FOUND)
    message(STATUS "  Qt6 path:   ${Qt6_DIR}")
else()
    message(STATUS "  Qt6 path:   <not found>")
endif()
