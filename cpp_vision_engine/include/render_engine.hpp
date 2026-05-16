#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// render_engine.hpp
// ─────────────────────────────────────────────────────────────────────────────

#include "arx_types.hpp"
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

namespace arx {

class RenderEngine {
public:
    void init(int width, int height);

    void draw_hand_skeleton(cv::Mat& frame,
                             const std::vector<HandLandmark>& hands);

    void draw_face_mesh(cv::Mat& frame,
                         const std::optional<FaceLandmark>& face);

    void draw_ar_cube(cv::Mat& frame,
                       const std::vector<HandLandmark>& hands);

    void draw_interaction_circles(cv::Mat& frame,
                                   const std::vector<HandLandmark>& hands);

    void draw_fps_counter(cv::Mat& frame, double fps);

    void draw_hud_overlay(cv::Mat& frame, const FrameMeta& meta);

private:
    int width_  = 1280;
    int height_ = 720;
};

}  // namespace arx
