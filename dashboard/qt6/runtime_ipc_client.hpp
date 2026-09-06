#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#include "engine/events/gesture_events.hpp"
#include "engine/ipc/local_ipc_protocol.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"

namespace arx::dashboard {

class RuntimeIpcClient final : public QObject {
    Q_OBJECT

public:
    explicit RuntimeIpcClient(QObject* parent = nullptr);

    void start(quint16 port = engine::ipc::kDefaultPort);
    void stop();
    [[nodiscard]] bool connected() const noexcept;
    [[nodiscard]] quint16 port() const noexcept;

public slots:
    void send_ping();
    void send_status();
    void send_pause();
    void send_resume();
    void send_shutdown();
    void reconnect_now();

signals:
    void frame_telemetry(const arx::engine::telemetry::RuntimeTelemetryFrame& frame);
    void gesture_event(const arx::engine::GestureRuntimeEvent& event);
    void protocol_ready(quint32 version, const QString& transport);
    void connection_state_changed(const QString& state, bool connected);
    void command_result(
        const QString& name,
        const QString& status,
        const QString& detail,
        const QString& request_id);
    void runtime_status(
        const QString& state,
        bool paused,
        bool shutdown_requested,
        const QString& mode);

private slots:
    void connect_socket();
    void on_connected();
    void on_disconnected();
    void on_ready_read();
    void on_error(QAbstractSocket::SocketError error);
    void heartbeat_tick();

private:
    void handle_payload(const QByteArray& payload);
    void send_payload(const QByteArray& payload);
    void send_command(const QString& name);
    void schedule_reconnect();

    QTcpSocket socket_;
    QTimer reconnect_timer_;
    QTimer heartbeat_timer_;
    QElapsedTimer health_timer_;
    QByteArray read_buffer_;
    quint16 port_{engine::ipc::kDefaultPort};
    quint64 request_sequence_{0};
    int reconnect_attempt_{0};
    bool started_{false};
    bool protocol_ready_{false};
};

}  // namespace arx::dashboard
