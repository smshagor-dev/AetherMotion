#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QTimer>

#include <string>

#include "engine/events/gesture_events.hpp"
#include "engine/telemetry/telemetry_encoder.hpp"

namespace arx::dashboard {

class DashboardWindow : public QMainWindow {
    Q_OBJECT

public:
    DashboardWindow();

public slots:
    void apply_frame_telemetry(const arx::engine::telemetry::RuntimeTelemetryFrame& frame);
    void apply_gesture_event(const arx::engine::GestureRuntimeEvent& event);

private:
    void refresh_demo_state();
    void append_timeline(const QString& entry);

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
    QProgressBar* cpu_{nullptr};
    QProgressBar* gpu_{nullptr};
    QPlainTextEdit* timeline_{nullptr};
    QTimer timer_;
    int frame_counter_{0};
    std::string last_tracker_error_;
};

}  // namespace arx::dashboard
