#pragma once

#ifdef ARX_HAS_OPENCV
#include <opencv2/core.hpp>
#endif

#include "vision/landmarks/landmark_types.hpp"

namespace arx::ar::renderer {

#ifdef ARX_HAS_OPENCV
class RenderEngine {
public:
    void init(int width, int height);
    void draw_hand_skeleton(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands);
    void draw_face_mesh(cv::Mat& frame, const std::optional<vision::FaceLandmarks>& face);
    void draw_ar_cube(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands);
    void draw_interaction_circles(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands);
    void draw_hand_points(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands, const cv::Scalar& color, int radius);
    void draw_hand_labels(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands);
    void draw_fps_counter(cv::Mat& frame, double fps);
    void draw_hud_overlay(cv::Mat& frame,
                          const vision::FrameMetadata& meta,
                          const std::string& gesture_label,
                          const std::string& tracker_state = {},
                          double inference_latency_ms = 0.0,
                          std::size_t hand_count = 0,
                          float top_hand_confidence = 0.0f,
                          bool model_loaded = false);

private:
    int width_{1280};
    int height_{720};
};
#endif

}  // namespace arx::ar::renderer
