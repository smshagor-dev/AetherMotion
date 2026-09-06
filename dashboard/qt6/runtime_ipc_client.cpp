#include "dashboard/qt6/runtime_ipc_client.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::dashboard {

namespace {

std::string json_string(const QJsonObject& object, const char* key) {
    const QByteArray utf8 = object.value(QLatin1String(key)).toString().toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

vision::GestureType gesture_from_name(const QString& name) {
    if (name == "open_hand") return vision::GestureType::kOpenHand;
    if (name == "closed_fist") return vision::GestureType::kClosedFist;
    if (name == "pinch") return vision::GestureType::kPinch;
    if (name == "swipe_left") return vision::GestureType::kSwipeLeft;
    if (name == "swipe_right") return vision::GestureType::kSwipeRight;
    if (name == "two_hand") return vision::GestureType::kTwoHand;
    if (name == "rotate") return vision::GestureType::kRotate;
    if (name == "zoom") return vision::GestureType::kZoom;
    if (name == "point_up") return vision::GestureType::kPointUp;
    if (name == "v_sign") return vision::GestureType::kVSign;
    if (name == "grab") return vision::GestureType::kGrab;
    if (name == "release") return vision::GestureType::kRelease;
    if (name == "hover_select") return vision::GestureType::kHoverSelect;
    return vision::GestureType::kNone;
}

vision::GestureEventKind event_kind_from_name(const QString& name) {
    if (name == "HELD") return vision::GestureEventKind::kHeld;
    if (name == "RELEASED") return vision::GestureEventKind::kReleased;
    if (name == "CHANGED") return vision::GestureEventKind::kChanged;
    return vision::GestureEventKind::kStarted;
}

}  // namespace

RuntimeIpcClient::RuntimeIpcClient(QObject* parent) : QObject(parent) {
    reconnect_timer_.setSingleShot(true);
    heartbeat_timer_.setInterval(2000);
    heartbeat_timer_.setSingleShot(false);

    connect(&reconnect_timer_, &QTimer::timeout, this, &RuntimeIpcClient::connect_socket);
    connect(&heartbeat_timer_, &QTimer::timeout, this, &RuntimeIpcClient::heartbeat_tick);
    connect(&socket_, &QTcpSocket::connected, this, &RuntimeIpcClient::on_connected);
    connect(&socket_, &QTcpSocket::disconnected, this, &RuntimeIpcClient::on_disconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &RuntimeIpcClient::on_ready_read);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &RuntimeIpcClient::on_error);
}

void RuntimeIpcClient::start(quint16 port) {
    if (port == 0) {
        port = engine::ipc::kDefaultPort;
    }

    if (started_ && port_ != port) {
        reconnect_timer_.stop();
        heartbeat_timer_.stop();
        protocol_ready_ = false;
        socket_.abort();
    }

    port_ = port;
    started_ = true;
    reconnect_attempt_ = 0;
    connect_socket();
}

void RuntimeIpcClient::stop() {
    started_ = false;
    protocol_ready_ = false;
    reconnect_attempt_ = 0;
    reconnect_timer_.stop();
    heartbeat_timer_.stop();
    read_buffer_.clear();
    socket_.abort();
    emit connection_state_changed("IPC stopped", false);
}

bool RuntimeIpcClient::connected() const noexcept {
    return protocol_ready_ && socket_.state() == QAbstractSocket::ConnectedState;
}

quint16 RuntimeIpcClient::port() const noexcept {
    return port_;
}

void RuntimeIpcClient::send_ping() {
    send_command(QStringLiteral("ping"));
}

void RuntimeIpcClient::send_status() {
    send_command(QStringLiteral("status"));
}

void RuntimeIpcClient::send_pause() {
    send_command(QStringLiteral("pause"));
}

void RuntimeIpcClient::send_resume() {
    send_command(QStringLiteral("resume"));
}

void RuntimeIpcClient::send_shutdown() {
    send_command(QStringLiteral("shutdown"));
}

void RuntimeIpcClient::reconnect_now() {
    if (!started_) {
        started_ = true;
    }
    reconnect_attempt_ = 0;
    protocol_ready_ = false;
    heartbeat_timer_.stop();
    reconnect_timer_.stop();
    socket_.abort();
    reconnect_timer_.start(0);
}

void RuntimeIpcClient::connect_socket() {
    if (!started_ || socket_.state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    emit connection_state_changed(
        QString("IPC connecting to 127.0.0.1:%1").arg(port_), false);
    socket_.connectToHost(QStringLiteral("127.0.0.1"), port_);
}

void RuntimeIpcClient::on_connected() {
    read_buffer_.clear();
    protocol_ready_ = false;
    reconnect_timer_.stop();
    emit connection_state_changed(
        QString("IPC socket connected to 127.0.0.1:%1; awaiting protocol hello").arg(port_),
        false);

    QTimer::singleShot(3000, this, [this] {
        if (started_ && socket_.state() == QAbstractSocket::ConnectedState && !protocol_ready_) {
            emit connection_state_changed("IPC handshake timeout; reconnecting", false);
            socket_.abort();
        }
    });
}

void RuntimeIpcClient::on_disconnected() {
    const bool was_ready = protocol_ready_;
    read_buffer_.clear();
    protocol_ready_ = false;
    heartbeat_timer_.stop();
    if (started_) {
        emit connection_state_changed(
            was_ready ? "IPC disconnected; reconnect scheduled" : "IPC unavailable; reconnect scheduled",
            false);
        schedule_reconnect();
    }
}

void RuntimeIpcClient::on_ready_read() {
    read_buffer_.append(socket_.readAll());

    while (read_buffer_.size() >= 4) {
        const auto b0 = static_cast<quint8>(read_buffer_.at(0));
        const auto b1 = static_cast<quint8>(read_buffer_.at(1));
        const auto b2 = static_cast<quint8>(read_buffer_.at(2));
        const auto b3 = static_cast<quint8>(read_buffer_.at(3));
        const quint32 length =
            (static_cast<quint32>(b0) << 24U) |
            (static_cast<quint32>(b1) << 16U) |
            (static_cast<quint32>(b2) << 8U) |
            static_cast<quint32>(b3);

        if (length == 0 || length > engine::ipc::kMaxPayloadBytes) {
            emit connection_state_changed("IPC protocol error: invalid frame length", false);
            read_buffer_.clear();
            socket_.abort();
            return;
        }

        const auto required = static_cast<qsizetype>(4 + length);
        if (read_buffer_.size() < required) {
            return;
        }

        const QByteArray payload = read_buffer_.mid(4, static_cast<qsizetype>(length));
        read_buffer_.remove(0, required);
        handle_payload(payload);
    }
}

void RuntimeIpcClient::on_error(QAbstractSocket::SocketError) {
    if (!started_) {
        return;
    }
    emit connection_state_changed(QString("IPC: %1").arg(socket_.errorString()), false);
    if (socket_.state() == QAbstractSocket::UnconnectedState) {
        schedule_reconnect();
    }
}

void RuntimeIpcClient::heartbeat_tick() {
    if (!connected()) {
        return;
    }
    if (health_timer_.isValid() && health_timer_.elapsed() > 6500) {
        emit connection_state_changed("IPC heartbeat stale; reconnecting", false);
        protocol_ready_ = false;
        socket_.abort();
        return;
    }
    send_ping();
}

void RuntimeIpcClient::handle_payload(const QByteArray& payload) {
    QJsonParseError parse_error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        emit connection_state_changed("IPC protocol error: malformed JSON payload", false);
        return;
    }

    const QJsonObject root = document.object();
    const QString type = root.value("type").toString();

    if (type == "protocol") {
        const quint32 version = static_cast<quint32>(root.value("version").toInt());
        const QString transport = root.value("transport").toString();
        if (version != engine::ipc::kProtocolVersion) {
            emit connection_state_changed(
                QString("IPC version mismatch: runtime v%1, desktop v%2")
                    .arg(version)
                    .arg(engine::ipc::kProtocolVersion),
                false);
            started_ = false;
            reconnect_timer_.stop();
            socket_.disconnectFromHost();
            return;
        }

        protocol_ready_ = true;
        reconnect_attempt_ = 0;
        health_timer_.start();
        if (!heartbeat_timer_.isActive()) {
            heartbeat_timer_.start();
        }
        emit protocol_ready(version, transport);
        emit connection_state_changed(
            QString("IPC v%1 ready on 127.0.0.1:%2").arg(version).arg(port_),
            true);
        send_ping();
        send_status();
        return;
    }

    if (type == "command_result") {
        const QString name = root.value("name").toString();
        const QString status = root.value("status").toString();
        const QString detail = root.value("detail").toString();
        const QString request_id = root.value("request_id").toString();

        if (name == "ping" && status == "ok") {
            health_timer_.restart();
            emit connection_state_changed(
                QString("IPC v%1 healthy").arg(engine::ipc::kProtocolVersion), true);
        }

        if (root.value("runtime").isObject()) {
            const QJsonObject runtime = root.value("runtime").toObject();
            emit runtime_status(
                runtime.value("state").toString(),
                runtime.value("paused").toBool(),
                runtime.value("shutdown_requested").toBool(),
                runtime.value("mode").toString());
        }

        emit command_result(name, status, detail, request_id);
        return;
    }

    if (type == "frame_telemetry") {
        engine::telemetry::RuntimeTelemetryFrame frame;
        frame.frame_meta.capture_us = static_cast<std::int64_t>(root.value("timestamp").toDouble() * 1000000.0);
        frame.frame_meta.process_us = frame.frame_meta.capture_us;
        frame.frame_meta.fps = root.value("fps").toDouble();
        frame.num_hands = static_cast<std::size_t>(root.value("num_hands").toInteger());
        frame.gesture = json_string(root, "gesture");
        frame.confidence = static_cast<float>(root.value("confidence").toDouble());
        frame.recording = root.value("recording").toBool();
        frame.replaying = root.value("replaying").toBool();
        frame.dropped_frames = static_cast<std::uint64_t>(root.value("dropped_frames").toInteger());
        frame.camera_fps = root.value("camera_fps").toDouble();
        frame.landmark_confidence = static_cast<float>(root.value("landmark_confidence").toDouble());
        frame.gesture_confidence = static_cast<float>(root.value("gesture_confidence").toDouble());
        frame.provider_health = json_string(root, "provider_health");

        const QJsonObject queue = root.value("queue").toObject();
        frame.queue_depth = static_cast<std::size_t>(queue.value("depth").toInteger());
        frame.queue_capacity = static_cast<std::size_t>(queue.value("capacity").toInteger());

        const QJsonObject tracking = root.value("tracking").toObject();
        frame.tracker_state = json_string(tracking, "state");
        frame.tracker_error = json_string(tracking, "error");
        frame.model_loaded = tracking.value("model_loaded").toBool();
        frame.raw_hand_count = static_cast<std::size_t>(tracking.value("raw_hand_count").toInteger());
        frame.top_hand_confidence = static_cast<float>(tracking.value("top_hand_confidence").toDouble());
        frame.hand_model_path = json_string(tracking, "hand_model_path");
        frame.face_model_path = json_string(tracking, "face_model_path");

        const QJsonObject perf = root.value("perf").toObject();
        frame.latency_ms = perf.value("latency_ms").toDouble();
        frame.camera_latency_ms = perf.value("camera_ms").toDouble();
        frame.inference_latency_ms = perf.value("inference_ms").toDouble();
        frame.smoothing_latency_ms = perf.value("smoothing_ms").toDouble();
        frame.gesture_latency_ms = perf.value("gesture_ms").toDouble();
        frame.render_latency_ms = perf.value("render_ms").toDouble();
        frame.end_to_end_latency_ms = perf.value("end_to_end_latency_ms").toDouble();

        if (root.value("profiler").isObject()) {
            const QByteArray compact = QJsonDocument(root.value("profiler").toObject()).toJson(QJsonDocument::Compact);
            frame.profiler_snapshot.assign(compact.constData(), static_cast<std::size_t>(compact.size()));
        }

        emit frame_telemetry(frame);
        return;
    }

    if (type == "gesture_event") {
        engine::GestureRuntimeEvent event;
        event.kind = event_kind_from_name(root.value("event_kind").toString());
        event.gesture = gesture_from_name(root.value("gesture").toString());
        event.confidence = static_cast<float>(root.value("confidence").toDouble());
        event.duration_seconds = root.value("duration").toDouble();
        event.timestamp_us = static_cast<std::int64_t>(root.value("timestamp").toDouble() * 1000000.0);
        emit gesture_event(event);
    }
}

void RuntimeIpcClient::send_payload(const QByteArray& payload) {
    if (socket_.state() != QAbstractSocket::ConnectedState || payload.isEmpty() ||
        payload.size() > static_cast<qsizetype>(engine::ipc::kMaxPayloadBytes)) {
        return;
    }

    const quint32 length = static_cast<quint32>(payload.size());
    QByteArray framed;
    framed.reserve(static_cast<qsizetype>(4 + payload.size()));
    framed.append(static_cast<char>((length >> 24U) & 0xFFU));
    framed.append(static_cast<char>((length >> 16U) & 0xFFU));
    framed.append(static_cast<char>((length >> 8U) & 0xFFU));
    framed.append(static_cast<char>(length & 0xFFU));
    framed.append(payload);
    socket_.write(framed);
    socket_.flush();
}

void RuntimeIpcClient::send_command(const QString& name) {
    if (!connected()) {
        emit connection_state_changed(
            QString("IPC command '%1' skipped: transport not ready").arg(name), false);
        return;
    }

    const QString request_id = QString("qt-%1").arg(++request_sequence_);
    QJsonObject command;
    command.insert(QStringLiteral("type"), QStringLiteral("command"));
    command.insert(QStringLiteral("version"), static_cast<int>(engine::ipc::kProtocolVersion));
    command.insert(QStringLiteral("name"), name);
    command.insert(QStringLiteral("request_id"), request_id);
    send_payload(QJsonDocument(command).toJson(QJsonDocument::Compact));
}

void RuntimeIpcClient::schedule_reconnect() {
    if (!started_ || reconnect_timer_.isActive()) {
        return;
    }

    const int exponent = std::min(reconnect_attempt_, 4);
    const int delay_ms = std::min(5000, 250 * (1 << exponent));
    ++reconnect_attempt_;
    reconnect_timer_.start(delay_ms);
}

}  // namespace arx::dashboard
