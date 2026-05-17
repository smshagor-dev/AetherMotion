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
const cv::Scalar kCyan{255, 255, 0};
const cv::Scalar kMagenta{255, 0, 255};
const cv::Scalar kGreen{0, 255, 128};
const cv::Scalar kOrange{0, 165, 255};
const cv::Scalar kWhite{255, 255, 255};
const cv::Scalar kDimWhite{180, 180, 180};
const cv::Scalar kAmber{0, 215, 255};
const cv::Scalar kHudBlue{255, 210, 70};
const cv::Scalar kHudGreen{110, 255, 170};
const cv::Scalar kMatrixGreen{80, 220, 120};
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

void RenderEngine::draw_fusion_overlay(cv::Mat& frame, const ar::fusion::ARFusionFrame& fusion) {
    const auto alpha = std::clamp(fusion.telemetry.tracking_fade, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
        return;
    }

    if (fusion.face_binary.active) {
        cv::Mat layer = frame.clone();
        const int x0 = std::clamp(static_cast<int>(fusion.face_binary.min_x * frame.cols), 0, frame.cols - 1);
        const int y0 = std::clamp(static_cast<int>(fusion.face_binary.min_y * frame.rows), 0, frame.rows - 1);
        const int x1 = std::clamp(static_cast<int>(fusion.face_binary.max_x * frame.cols), x0 + 1, frame.cols);
        const int y1 = std::clamp(static_cast<int>(fusion.face_binary.max_y * frame.rows), y0 + 1, frame.rows);
        const int columns = std::max(4, static_cast<int>(fusion.face_binary.density * 18.0f));
        const double t = static_cast<double>(cv::getTickCount()) / cv::getTickFrequency();
        for (int c = 0; c < columns; ++c) {
            const int x = x0 + (c * std::max(1, (x1 - x0) / columns));
            const int offset = static_cast<int>(std::fmod((t * 90.0 * fusion.face_binary.speed) + c * 17.0, std::max(1, y1 - y0)));
            for (int y = y0; y < y1; y += 18) {
                const char digit = ((y + offset) / 18 + c) % 2 == 0 ? '1' : '0';
                cv::putText(layer, std::string(1, digit), {x, y0 + ((y - y0 + offset) % std::max(1, y1 - y0))},
                    cv::FONT_HERSHEY_PLAIN, 0.8, kMatrixGreen, 1, cv::LINE_AA);
            }
        }
        cv::addWeighted(layer, std::clamp(fusion.face_binary.opacity, 0.0f, 1.0f), frame,
            1.0 - std::clamp(fusion.face_binary.opacity, 0.0f, 1.0f), 0.0, frame);
    }

    for (const auto& hand : fusion.smoothed_hands) {
        const auto color = hand.is_left ? kCyan : kMagenta;
        auto lm_px = [&](int i) {
            return cv::Point{
                static_cast<int>(hand.points[i].x * frame.cols),
                static_cast<int>(hand.points[i].y * frame.rows)
            };
        };
        if (fusion.hud.pinch_ring_visible) {
            const auto thumb = lm_px(4);
            const auto index = lm_px(8);
            cv::line(frame, thumb, index, kHudBlue, 1, cv::LINE_AA);
            const auto mid = cv::Point{(thumb.x + index.x) / 2, (thumb.y + index.y) / 2};
            const int ring_radius = std::max(8, static_cast<int>(fusion.hud.pinch_distance * frame.cols * 0.15f));
            cv::circle(frame, mid, ring_radius, kHudBlue, 2, cv::LINE_AA);
        }
        if (fusion.hud.triangular_frame_visible) {
            const auto a = lm_px(4);
            const auto b = lm_px(8);
            const auto c = lm_px(12);
            cv::line(frame, a, b, color, 2, cv::LINE_AA);
            cv::line(frame, b, c, color, 2, cv::LINE_AA);
            cv::line(frame, c, a, color, 2, cv::LINE_AA);
        }
    }

    if (fusion.hud.spatial_cursor_visible) {
        const cv::Point cursor{
            static_cast<int>(fusion.hud.cursor.x * frame.cols),
            static_cast<int>(fusion.hud.cursor.y * frame.rows)
        };
        cv::circle(frame, cursor, 12, kHudGreen, 1, cv::LINE_AA);
        cv::circle(frame, cursor, 20, kHudGreen, 1, cv::LINE_AA);
        cv::line(frame, {cursor.x - 16, cursor.y}, {cursor.x + 16, cursor.y}, kHudGreen, 1, cv::LINE_AA);
        cv::line(frame, {cursor.x, cursor.y - 16}, {cursor.x, cursor.y + 16}, kHudGreen, 1, cv::LINE_AA);
    }

    if (fusion.hud.zoom_guide_visible && fusion.smoothed_hands.size() > 1) {
        const auto& left = fusion.smoothed_hands.front().points[0];
        const auto& right = fusion.smoothed_hands.back().points[0];
        cv::Point a{static_cast<int>(left.x * frame.cols), static_cast<int>(left.y * frame.rows)};
        cv::Point b{static_cast<int>(right.x * frame.cols), static_cast<int>(right.y * frame.rows)};
        cv::line(frame, a, b, kOrange, 2, cv::LINE_AA);
        cv::circle(frame, a, 10, kOrange, 1, cv::LINE_AA);
        cv::circle(frame, b, 10, kOrange, 1, cv::LINE_AA);
    }

    char gesture[160];
    std::snprintf(gesture, sizeof(gesture), "%s conf=%.2f fade=%.2f",
        fusion.gesture_label.c_str(), fusion.gesture_confidence, fusion.telemetry.tracking_fade);
    cv::putText(frame, gesture, {10, 108}, cv::FONT_HERSHEY_DUPLEX, 0.7, kHudBlue, 1, cv::LINE_AA);
}

}  // namespace arx::ar::renderer

#endif
