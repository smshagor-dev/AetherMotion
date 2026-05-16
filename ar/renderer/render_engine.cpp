#include "ar/renderer/render_engine.hpp"

#ifdef ARX_HAS_OPENCV

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>

namespace arx::ar::renderer {

namespace {
const std::vector<std::pair<int, int>> kHandConnections = {
    {0,1},{1,2},{2,3},{3,4},{0,5},{5,6},{6,7},{7,8},
    {5,9},{9,10},{10,11},{11,12},{9,13},{13,14},{14,15},{15,16},
    {13,17},{17,18},{18,19},{19,20},{0,17}
};
constexpr cv::Scalar kCyan{255, 255, 0};
constexpr cv::Scalar kMagenta{255, 0, 255};
constexpr cv::Scalar kGreen{0, 255, 128};
constexpr cv::Scalar kOrange{0, 165, 255};
constexpr cv::Scalar kWhite{255, 255, 255};
constexpr cv::Scalar kDimWhite{180, 180, 180};
constexpr cv::Scalar kAmber{0, 215, 255};
}

void RenderEngine::init(int width, int height) {
    width_ = width;
    height_ = height;
}

void RenderEngine::draw_hand_skeleton(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands) {
    for (const auto& hand : hands) {
        auto lm_px = [&](int i) {
            return cv::Point{
                static_cast<int>(hand.points[i].x * frame.cols),
                static_cast<int>(hand.points[i].y * frame.rows)
            };
        };
        for (const auto& [a, b] : kHandConnections) {
            cv::line(frame, lm_px(a), lm_px(b), hand.is_left ? kCyan : kMagenta, 2, cv::LINE_AA);
        }
        for (int i = 0; i < 21; ++i) {
            const int radius = (i == 0) ? 7 : (i % 4 == 0 ? 5 : 3);
            cv::circle(frame, lm_px(i), radius, kWhite, -1, cv::LINE_AA);
        }
    }
}

void RenderEngine::draw_face_mesh(cv::Mat& frame, const std::optional<vision::FaceLandmarks>& face) {
    if (!face.has_value()) {
        return;
    }
    for (const auto& point : face->points) {
        cv::circle(frame,
            {static_cast<int>(point.x * frame.cols), static_cast<int>(point.y * frame.rows)},
            1, kDimWhite, -1, cv::LINE_AA);
    }
}

void RenderEngine::draw_ar_cube(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands) {
    if (hands.empty()) {
        return;
    }
    const auto& wrist = hands.front().points[0];
    const float cx = wrist.x * frame.cols;
    const float cy = wrist.y * frame.rows;
    const float depth = 1.f - std::clamp(wrist.z + 0.5f, 0.f, 1.f);
    const float size = 40.f * depth;
    const float angle = static_cast<float>(cv::getTickCount()) / cv::getTickFrequency() * 0.8f;
    const float ca = std::cos(angle);
    const float sa = std::sin(angle);
    const float h = size;
    std::vector<cv::Point3f> cube = {
        {-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
        {-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}
    };
    std::vector<cv::Point> projected;
    projected.reserve(cube.size());
    for (const auto& p : cube) {
        const float rx = p.x * ca - p.z * sa;
        const float rz = p.x * sa + p.z * ca;
        const float f = 400.f / (400.f - rz);
        projected.push_back({static_cast<int>(cx + rx * f), static_cast<int>(cy + p.y * f)});
    }
    auto edge = [&](int a, int b, const cv::Scalar& color, int thickness) {
        cv::line(frame, projected[a], projected[b], color, thickness, cv::LINE_AA);
    };
    for (int i = 0; i < 4; ++i) edge(i, (i + 1) % 4, kDimWhite, 1);
    for (int i = 4; i < 8; ++i) edge(i, 4 + (i - 4 + 1) % 4, kCyan, 2);
    for (int i = 0; i < 4; ++i) edge(i, i + 4, kMagenta, 1);
}

void RenderEngine::draw_interaction_circles(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands) {
    constexpr int kTips[] = {4, 8, 12, 16, 20};
    for (const auto& hand : hands) {
        for (int tip : kTips) {
            const auto& point = hand.points[tip];
            const float depth = 1.f - std::clamp(point.z + 0.5f, 0.f, 1.f);
            const int radius = static_cast<int>(12.f * depth);
            cv::Point p{static_cast<int>(point.x * frame.cols), static_cast<int>(point.y * frame.rows)};
            cv::circle(frame, p, radius, kOrange, 1, cv::LINE_AA);
            cv::circle(frame, p, radius + 4, kOrange, 1, cv::LINE_AA);
        }
    }
}

void RenderEngine::draw_hand_points(cv::Mat& frame,
                                    const std::vector<vision::HandLandmarks>& hands,
                                    const cv::Scalar& color,
                                    int radius) {
    for (const auto& hand : hands) {
        for (const auto& point : hand.points) {
            cv::circle(frame,
                {static_cast<int>(point.x * frame.cols), static_cast<int>(point.y * frame.rows)},
                radius,
                color,
                -1,
                cv::LINE_AA);
        }
    }
}

void RenderEngine::draw_hand_labels(cv::Mat& frame, const std::vector<vision::HandLandmarks>& hands) {
    for (const auto& hand : hands) {
        const auto& wrist = hand.points[0];
        const cv::Point anchor{
            static_cast<int>(wrist.x * frame.cols),
            static_cast<int>(wrist.y * frame.rows) - 12
        };
        char label[64];
        std::snprintf(label, sizeof(label), "%s conf=%.2f", hand.is_left ? "Left" : "Right", hand.confidence);
        cv::putText(frame, label, anchor, cv::FONT_HERSHEY_PLAIN, 1.0, kWhite, 1, cv::LINE_AA);
    }
}

void RenderEngine::draw_fps_counter(cv::Mat& frame, double fps) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "FPS: %.1f", fps);
    cv::putText(frame, buffer, {10, 28}, cv::FONT_HERSHEY_DUPLEX, 0.7, kGreen, 1, cv::LINE_AA);
}

void RenderEngine::draw_hud_overlay(cv::Mat& frame,
                                    const vision::FrameMetadata& meta,
                                    const std::string& gesture_label,
                                    const std::string& tracker_state,
                                    double inference_latency_ms,
                                    std::size_t hand_count,
                                    float top_hand_confidence,
                                    bool model_loaded) {
    cv::putText(frame, "Gesture: " + gesture_label, {10, 55}, cv::FONT_HERSHEY_DUPLEX, 0.7, kWhite, 1, cv::LINE_AA);
    char tracking[160];
    std::snprintf(tracking, sizeof(tracking), "Tracker: %s | model: %s | hands: %zu | conf: %.2f | infer: %.1f ms",
        tracker_state.empty() ? "unknown" : tracker_state.c_str(),
        model_loaded ? "loaded" : "missing",
        hand_count,
        top_hand_confidence,
        inference_latency_ms);
    cv::putText(frame, tracking, {10, 82}, cv::FONT_HERSHEY_PLAIN, 1.0, kAmber, 1, cv::LINE_AA);
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "FRAME #%06llu", static_cast<unsigned long long>(meta.frame_id));
    cv::putText(frame, buffer, {10, frame.rows - 10}, cv::FONT_HERSHEY_PLAIN, 0.9, kDimWhite, 1, cv::LINE_AA);
}

}  // namespace arx::ar::renderer

#endif
