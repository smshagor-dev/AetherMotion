#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QTimer>

#include <string>

#include "engine/events/gesture_events.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"

class QCheckBox;
class QComboBox;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace arx::dashboard {

class DashboardWindow final : public QMainWindow {
    Q_OBJECT

public:
    DashboardWindow();
    ~DashboardWindow() override;

public slots:
    void apply_frame_telemetry(const arx::engine::telemetry::RuntimeTelemetryFrame& frame);
    void apply_gesture_event(const arx::engine::GestureRuntimeEvent& event);

private slots:
    void start_native_runtime();
    void stop_native_runtime();
    void check_models();
    void start_legacy_services();
    void stop_legacy_services();
    void browse_workspace();
    void browse_session();
    void drain_native_output();
    void drain_legacy_output();
    void native_process_finished(int exit_code, QProcess::ExitStatus status);
    void legacy_process_finished(int exit_code, QProcess::ExitStatus status);

private:
    void build_ui();
    void wire_processes();
    void refresh_demo_state();
    void append_timeline(const QString& entry);
    void append_runtime_log(const QString& source, const QByteArray& bytes);
    void set_native_running(bool running);
    void set_legacy_running(bool running);
    [[nodiscard]] QString workspace_root() const;
    [[nodiscard]] QString resolve_native_runtime() const;
    [[nodiscard]] QString resolve_python() const;
    [[nodiscard]] QStringList native_arguments(bool model_check_only = false) const;
    bool ensure_workspace_valid();

    QLabel* title_{nullptr};
    QLabel* subtitle_{nullptr};
    QLabel* fps_{nullptr};
    QLabel* latency_{nullptr};
    QLabel* gesture_{nullptr};
    QLabel* session_{nullptr};
    QLabel* replay_{nullptr};
    QLabel* dropped_{nullptr};
    QLabel* health_{nullptr};
    QLabel* tracker_{nullptr};
    QLabel* confidence_{nullptr};
    QLabel* hands_{nullptr};
    QLabel* native_status_{nullptr};
    QLabel* legacy_status_{nullptr};
    QProgressBar* cpu_{nullptr};
    QProgressBar* gpu_{nullptr};
    QPlainTextEdit* timeline_{nullptr};
    QPlainTextEdit* runtime_log_{nullptr};
    QLineEdit* workspace_edit_{nullptr};
    QLineEdit* session_edit_{nullptr};
    QComboBox* mode_combo_{nullptr};
    QSpinBox* camera_spin_{nullptr};
    QCheckBox* legacy_go_{nullptr};
    QCheckBox* legacy_ai_{nullptr};
    QPushButton* start_native_button_{nullptr};
    QPushButton* stop_native_button_{nullptr};
    QPushButton* check_models_button_{nullptr};
    QPushButton* start_legacy_button_{nullptr};
    QPushButton* stop_legacy_button_{nullptr};
    QTabWidget* tabs_{nullptr};
    QProcess native_process_;
    QProcess legacy_process_;
    QTimer timer_;
    int frame_counter_{0};
    std::string last_tracker_error_;
};

}  // namespace arx::dashboard
