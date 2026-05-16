#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// telemetry_encoder.hpp
// ─────────────────────────────────────────────────────────────────────────────

#include "arx_types.hpp"
#include <memory>

#ifdef ARX_ZEROMQ_ENABLED
namespace zmq { class context_t; class socket_t; }
#endif

namespace arx {

class TelemetryEncoder {
public:
    explicit TelemetryEncoder(int port);
    ~TelemetryEncoder();

    void init();
    void encode_and_send(const TelemetryPacket& pkt);
    void shutdown();

private:
    int port_;

#ifdef ARX_ZEROMQ_ENABLED
    std::unique_ptr<zmq::context_t> ctx_;
    std::unique_ptr<zmq::socket_t>  sock_;
#endif
};

}  // namespace arx
