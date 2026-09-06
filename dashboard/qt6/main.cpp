#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>

#include "dashboard/qt6/dashboard_window.hpp"
#include "dashboard/qt6/runtime_ipc_client.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AetherMotion"));
    QCoreApplication::setApplicationName(QStringLiteral("OperatorControlCenter"));

    arx::dashboard::DashboardWindow window;
    arx::dashboard::RuntimeIpcClient ipc_client;
    window.restore_operator_settings();

    auto* control_bar = window.addToolBar(QStringLiteral("Runtime Control"));
    control_bar->setObjectName(QStringLiteral("runtime-control-toolbar"));
    control_bar->setMovable(false);

    auto* ipc_label = new QLabel(QStringLiteral(" IPC: offline "), control_bar);
    control_bar->addWidget(ipc_label);
    control_bar->addSeparator();

    QAction* status_action = control_bar->addAction(QStringLiteral("Status"));
    QAction* pause_action = control_bar->addAction(QStringLiteral("Pause"));
    QAction* resume_action = control_bar->addAction(QStringLiteral("Resume"));
    QAction* shutdown_action = control_bar->addAction(QStringLiteral("Graceful Shutdown"));
    control_bar->addSeparator();
    QAction* reconnect_action = control_bar->addAction(QStringLiteral("Reconnect IPC"));

    status_action->setEnabled(false);
    pause_action->setEnabled(false);
    resume_action->setEnabled(false);
    shutdown_action->setEnabled(false);

    QObject::connect(status_action, &QAction::triggered, &ipc_client, &arx::dashboard::RuntimeIpcClient::send_status);
    QObject::connect(pause_action, &QAction::triggered, &ipc_client, &arx::dashboard::RuntimeIpcClient::send_pause);
    QObject::connect(resume_action, &QAction::triggered, &ipc_client, &arx::dashboard::RuntimeIpcClient::send_resume);
    QObject::connect(shutdown_action, &QAction::triggered, &ipc_client, &arx::dashboard::RuntimeIpcClient::send_shutdown);
    QObject::connect(reconnect_action, &QAction::triggered, &ipc_client, &arx::dashboard::RuntimeIpcClient::reconnect_now);

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
        &arx::dashboard::RuntimeIpcClient::command_result,
        &window,
        &arx::dashboard::DashboardWindow::apply_runtime_command_result);
    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::runtime_status,
        &window,
        &arx::dashboard::DashboardWindow::apply_runtime_status);

    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::connection_state_changed,
        &window,
        [&window, ipc_label, status_action, pause_action, resume_action, shutdown_action](
            const QString& state,
            bool connected) {
            ipc_label->setText(connected ? QStringLiteral(" IPC: ready ") : QStringLiteral(" IPC: offline "));
            status_action->setEnabled(connected);
            pause_action->setEnabled(connected);
            resume_action->setEnabled(connected);
            shutdown_action->setEnabled(connected);
            window.statusBar()->showMessage(state, connected ? 5000 : 2500);
        });

    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::runtime_status,
        &window,
        [pause_action, resume_action, shutdown_action](
            const QString&,
            bool paused,
            bool shutdown_requested,
            const QString&) {
            pause_action->setEnabled(!paused && !shutdown_requested);
            resume_action->setEnabled(paused && !shutdown_requested);
            shutdown_action->setEnabled(!shutdown_requested);
        });

    QObject::connect(
        &ipc_client,
        &arx::dashboard::RuntimeIpcClient::protocol_ready,
        &window,
        [&window, &ipc_client](quint32 version, const QString& transport) {
            window.statusBar()->showMessage(
                QString("Native IPC v%1 ready (%2)").arg(version).arg(transport),
                5000);
            ipc_client.send_status();
        });

    QObject::connect(
        &app,
        &QCoreApplication::aboutToQuit,
        &window,
        [&window, &ipc_client] {
            window.save_operator_settings();
            ipc_client.stop();
        });

    ipc_client.start();
    window.show();
    return app.exec();
}
