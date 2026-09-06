#include <QApplication>
#include <QCoreApplication>
#include <QStatusBar>

#include "dashboard/qt6/dashboard_window.hpp"
#include "dashboard/qt6/runtime_ipc_client.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    arx::dashboard::DashboardWindow window;
    arx::dashboard::RuntimeIpcClient ipc_client;

    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::frame_telemetry,
        &window,
        &arx::dashboard::DashboardWindow::apply_frame_telemetry);
    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::gesture_event,
        &window,
        &arx::dashboard::DashboardWindow::apply_gesture_event);
    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::connection_state_changed,
        &window,
        [&window](const QString& state, bool connected) {
            window.statusBar()->showMessage(state, connected ? 5000 : 2500);
        });
    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::protocol_ready,
        &window,
        [&window](quint32 version, const QString& transport) {
            window.statusBar()->showMessage(
                QString("Native IPC v%1 ready (%2)").arg(version).arg(transport),
                5000);
        });
    QObject::connect(
        &app,
        &QCoreApplication::aboutToQuit,
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::stop);

    ipc_client.start();
    window.show();
    return app.exec();
}
