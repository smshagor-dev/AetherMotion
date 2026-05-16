// ─────────────────────────────────────────────────────────────────────────────
// render_engine.cpp  –  OpenCV-based AR overlay rendering.
//
//  Draws on top of the live camera frame:
//    • Hand skeleton (21-point MediaPipe model)
//    • Face mesh (468-point)
//    • AR cube (perspective-projected, depth-shaded)
//    • Interaction circles at fingertips
//    • HUD: FPS, latency, gesture label, depth illusion rings
// ─────────────────────────────────────────────────────────────────────────────

#include "render_engine.hpp"
#include "arx_types.hpp"

#include <opencv2/imgproc.hpp>
#include <cmath>
#include <vector>
#include <string>

namespace arx {

// ─── MediaPipe hand connections (21 landmarks, 0-indexed) ──────────────────
static const std::vector<std::pair<int,int>> kHandConnections = {
    {0,1},{1,2},{2,3},{3,4},         // thumb
    {0,5},{5,6},{6,7},{7,8},         // index
    {5,9},{9,10},{10,11},{11,12},    // middle
    {9,13},{13,14},{14,15},{15,16},  // ring
    {13,17},{17,18},{18,19},{19,20}, // pinky
    {0,17}                           // palm base
};

// ─── Colour palette (BGR) ─────────────────────────────────────────────────
static constexpr cv::Scalar kCyan       {255, 255,   0};
static constexpr cv::Scalar kMagenta    {255,   0, 255};
static constexpr cv::Scalar kGreen      {  0, 255, 128};
static constexpr cv::Scalar kOrange     {  0, 165, 255};
static constexpr cv::Scalar kWhite      {255, 255, 255};
static constexpr cv::Scalar kDimWhite   {180, 180, 180};
static constexpr cv::Scalar kRed        {  0,   0, 255};
static constexpr cv::Scalar kBlue       {255,  50,  50};

void RenderEngine::init(int w, int h) {
    width_ = w; height_ = h;
}

// ─────────────────────────────────────────────────────────────────────────────
// Hand skeleton
// ─────────────────────────────────────────────────────────────────────────────
void RenderEngine::draw_hand_skeleton(cv::Mat& frame,
                                       const std::vector<HandLandmark>& hands)
{
    for (const auto& hand : hands) {
        const auto& pts = hand.points;
        const bool is_left = hand.is_left;

        auto lm_px = [&](int i) -> cv::Point {
            return {
                static_cast<int>(pts[i].x * frame.cols),
                static_cast<int>(pts[i].y * frame.rows)
            };
        };

        // Bones
        for (const auto& [a, b] : kHandConnections) {
            cv::line(frame, lm_px(a), lm_px(b),
                     is_left ? kCyan : kMagenta, 2, cv::LINE_AA);
        }

        // Joints
        for (int i = 0; i < 21; ++i) {
            const int radius = (i == 0) ? 7 : (i % 4 == 0 ? 5 : 3);
            cv::circle(frame, lm_px(i), radius, kWhite, -1, cv::LINE_AA);
            cv::circle(frame, lm_px(i), radius,
                       is_left ? kCyan : kMagenta, 1, cv::LINE_AA);
        }

        // Fingertip highlight circles
        constexpr int fingertips[] = {4, 8, 12, 16, 20};
        for (int tip : fingertips) {
            cv::circle(frame, lm_px(tip), 9, kGreen, 1, cv::LINE_AA);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Face mesh  (renders a subset of 468 landmarks for performance)
// ─────────────────────────────────────────────────────────────────────────────
void RenderEngine::draw_face_mesh(cv::Mat& frame,
                                   const std::optional<FaceLandmark>& face)
{
    if (!face) return;
    const auto& pts = face->points;
    for (const auto& pt : pts) {
        cv::Point p{
            static_cast<int>(pt.x * frame.cols),
            static_cast<int>(pt.y * frame.rows)
        };
        cv::circle(frame, p, 1, kDimWhite, -1, cv::LINE_AA);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AR Cube  –  perspective-projected 3D box anchored to wrist landmark
// ─────────────────────────────────────────────────────────────────────────────
void RenderEngine::draw_ar_cube(cv::Mat& frame,
                                 const std::vector<HandLandmark>& hands)
{
    if (hands.empty()) return;
    const auto& wrist = hands[0].points[0];

    const float cx = wrist.x * frame.cols;
    const float cy = wrist.y * frame.rows;
    const float depth = 1.f - std::clamp(wrist.z + 0.5f, 0.f, 1.f);
    const float size  = 40.f * depth;
    const float angle = static_cast<float>(cv::getTickCount()) /
                        cv::getTickFrequency() * 0.8f;  // rotation over time

    // 8 cube vertices in model space
    const float h = size;
    std::vector<cv::Point3f> cube_3d = {
        {-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
        {-h,-h, h},{h,-h, h},{h,h, h},{-h,h, h}
    };

    // Simple Y-axis rotation matrix
    const float ca = std::cos(angle), sa = std::sin(angle);
    auto rot = [&](cv::Point3f p) -> cv::Point {
        float rx = p.x * ca - p.z * sa;
        float rz = p.x * sa + p.z * ca;
        // Perspective divide (f=400)
        float f = 400.f / (400.f - rz);
        return {static_cast<int>(cx + rx * f),
                static_cast<int>(cy + p.y * f)};
    };

    std::vector<cv::Point> proj(8);
    std::transform(cube_3d.begin(), cube_3d.end(), proj.begin(), rot);

    // Draw edges (front face, back face, connectors)
    auto edge = [&](int a, int b, cv::Scalar col, int thick) {
        cv::line(frame, proj[a], proj[b], col, thick, cv::LINE_AA);
    };
    // Back face
    for (int i = 0; i < 4; ++i) edge(i, (i+1)%4, kBlue, 1);
    // Front face
    for (int i = 4; i < 8; ++i) edge(i, 4 + (i-4+1)%4, kCyan, 2);
    // Pillars
    for (int i = 0; i < 4; ++i) edge(i, i+4, kMagenta, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Interaction circles at fingertips
// ─────────────────────────────────────────────────────────────────────────────
void RenderEngine::draw_interaction_circles(cv::Mat& frame,
                                             const std::vector<HandLandmark>& hands)
{
    constexpr int kTips[] = {4, 8, 12, 16, 20};
    for (const auto& hand : hands) {
        for (int i : kTips) {
            cv::Point p{
                static_cast<int>(hand.points[i].x * frame.cols),
                static_cast<int>(hand.points[i].y * frame.rows)
            };
            // Depth-modulated pulse radius
            const float depth = 1.f - std::clamp(hand.points[i].z + 0.5f, 0.f, 1.f);
            const int r = static_cast<int>(12.f * depth);
            cv::circle(frame, p, r,     kOrange, 1, cv::LINE_AA);
            cv::circle(frame, p, r + 4, kOrange, 1, cv::LINE_AA);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HUD overlay
// ─────────────────────────────────────────────────────────────────────────────
void RenderEngine::draw_fps_counter(cv::Mat& frame, double fps) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "FPS: %.1f", fps);
    cv::putText(frame, buf,
                {10, 28}, cv::FONT_HERSHEY_DUPLEX, 0.7, kGreen, 1, cv::LINE_AA);
}

void RenderEngine::draw_hud_overlay(cv::Mat& frame, const FrameMeta& meta) {
    // Corner brackets (top-left)
    const int L = 20;
    cv::line(frame, {0,0}, {L,0}, kCyan, 2);
    cv::line(frame, {0,0}, {0,L}, kCyan, 2);

    // Corner brackets (bottom-right)
    const int bx = frame.cols, by = frame.rows;
    cv::line(frame, {bx,by}, {bx-L,by}, kCyan, 2);
    cv::line(frame, {bx,by}, {bx,by-L}, kCyan, 2);

    // Frame ID
    char buf[64];
    std::snprintf(buf, sizeof(buf), "FRAME #%06lld", (long long)meta.frame_id);
    cv::putText(frame, buf,
                {10, frame.rows - 10},
                cv::FONT_HERSHEY_PLAIN, 0.9, kDimWhite, 1, cv::LINE_AA);
}

}  // namespace arx
