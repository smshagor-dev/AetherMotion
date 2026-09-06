#pragma once

#include <QByteArray>
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

public slots:
    void send_ping();

signals:
    void frame_telemetry(const arx::engine::telemetry::RuntimeTelemetryFrame& frame);
    void gesture_event(const arx::engine::GestureRuntimeEvent& event);
    void protocol_ready(quint32 version, const QString& transport);
    void connection_state_changed(const QString& state, bool connected);

private slots:
    void connect_socket();
    void on_connected();
    void on_disconnected();
    void on_ready_read();
    void on_error(QAbstractSocket::SocketError error);

private:
    void handle_payload(const QByteArray& payload);
    void send_payload(const QByteArray& payload);

    QTcpSocket socket_;
    QTimer reconnect_timer_;
    QByteArray read_buffer_;
    quint16 port_{engine::ipc::kDefaultPort};
    bool started_{false};
};

}  // namespace arx::dashboard
