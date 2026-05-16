// ─────────────────────────────────────────────────────────────────────────────
// telemetry_encoder.cpp  –  Serialises TelemetryPackets to JSON and publishes
//                            them via ZeroMQ PUB socket to the Go control plane.
// ─────────────────────────────────────────────────────────────────────────────

#include "telemetry_encoder.hpp"
#include "arx_types.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#ifdef ARX_ZEROMQ_ENABLED
#  include <zmq.hpp>
#endif

namespace arx {

using json = nlohmann::json;

static const char* gesture_name(GestureType g) {
    switch (g) {
        case GestureType::kOpenHand:   return "open_hand";
        case GestureType::kClosedFist: return "closed_fist";
        case GestureType::kPinch:      return "pinch";
        case GestureType::kSwipeLeft:  return "swipe_left";
        case GestureType::kSwipeRight: return "swipe_right";
        case GestureType::kTwoHand:    return "two_hand";
        case GestureType::kRotate:     return "rotate";
        case GestureType::kZoom:       return "zoom";
        case GestureType::kPointUp:    return "point_up";
        case GestureType::kVSign:      return "v_sign";
        default:                       return "none";
    }
}

TelemetryEncoder::TelemetryEncoder(int port) : port_(port) {}
TelemetryEncoder::~TelemetryEncoder() { shutdown(); }

void TelemetryEncoder::init() {
#ifdef ARX_ZEROMQ_ENABLED
    ctx_  = std::make_unique<zmq::context_t>(1);
    sock_ = std::make_unique<zmq::socket_t>(*ctx_, zmq::socket_type::pub);
    sock_->set(zmq::sockopt::sndhwm, 10);  // drop old messages, never block
    const std::string ep = "tcp://*:" + std::to_string(port_);
    sock_->bind(ep);
    spdlog::info("[Telemetry] ZeroMQ PUB bound on {}", ep);
#else
    spdlog::warn("[Telemetry] ZeroMQ not compiled in; telemetry disabled");
#endif
}

void TelemetryEncoder::encode_and_send(const TelemetryPacket& pkt) {
    json j;

    // Frame metadata
    j["frame"]["id"]           = pkt.frame_meta.frame_id;
    j["frame"]["capture_us"]   = pkt.frame_meta.capture_us;
    j["frame"]["process_us"]   = pkt.frame_meta.process_us;
    j["frame"]["width"]        = pkt.frame_meta.width;
    j["frame"]["height"]       = pkt.frame_meta.height;
    j["frame"]["fps"]          = pkt.frame_meta.fps;
    j["frame"]["dropped"]      = pkt.frame_meta.dropped_frames;

    // Gesture
    j["gesture"]["type"]       = gesture_name(pkt.gesture.type);
    j["gesture"]["confidence"] = pkt.gesture.confidence;
    j["gesture"]["origin"]     = {pkt.gesture.origin.x, pkt.gesture.origin.y};

    // Hands (summary: only wrist + fingertip positions)
    j["hands"] = json::array();
    for (const auto& h : pkt.hands) {
        json hj;
        hj["is_left"]    = h.is_left;
        hj["confidence"] = h.confidence;
        hj["wrist"]      = {h.points[0].x, h.points[0].y, h.points[0].z};
        hj["fingertips"] = json::array();
        for (int tip : {4, 8, 12, 16, 20}) {
            hj["fingertips"].push_back({
                h.points[tip].x,
                h.points[tip].y,
                h.points[tip].z
            });
        }
        j["hands"].push_back(hj);
    }

    // Performance
    j["perf"]["latency_ms"] = pkt.pipeline_latency_ms;
    j["perf"]["cpu_pct"]    = pkt.cpu_usage_pct;
    j["perf"]["gpu_pct"]    = pkt.gpu_usage_pct;
    j["source"]             = "cpp_vision_engine";
    j["ts"]                 = now_us();

    const std::string payload = j.dump();

#ifdef ARX_ZEROMQ_ENABLED
    if (sock_) {
        zmq::message_t msg(payload.begin(), payload.end());
        sock_->send(msg, zmq::send_flags::dontwait);
    }
#endif
}

void TelemetryEncoder::shutdown() {
#ifdef ARX_ZEROMQ_ENABLED
    if (sock_) sock_->close();
    if (ctx_)  ctx_->close();
#endif
}

}  // namespace arx
