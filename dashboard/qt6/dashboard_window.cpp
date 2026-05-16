#include "dashboard/qt6/dashboard_window.hpp"

#include <algorithm>

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::dashboard {

DashboardWindow::DashboardWindow() {
    setWindowTitle("ARX Platform v3.0 Advanced");
    resize(1440, 900);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    root->setStyleSheet(
        "QWidget { background: #07111f; color: #dce9ff; }"
        "QFrame { background: #0c1628; border: 1px solid #173153; border-radius: 8px; }"
        "QPlainTextEdit { background: #08111d; border: 1px solid #173153; }");

    title_ = new QLabel("ARX Platform v3.0");
    title_->setStyleSheet("font-size: 28px; font-weight: 700; color: #00e5ff;");
    subtitle_ = new QLabel("C++-First Real-Time Gesture Intelligence and Spatial AR Engine");
    subtitle_->setStyleSheet("font-size: 14px; color: #8aa3c7;");

    auto* top = new QFrame();
    auto* top_layout = new QVBoxLayout(top);
    top_layout->addWidget(title_);
    top_layout->addWidget(subtitle_);

    auto* metrics = new QFrame();
    auto* metrics_layout = new QHBoxLayout(metrics);
    fps_ = new QLabel("FPS: --");
    latency_ = new QLabel("Latency: --");
    gesture_ = new QLabel("Gesture: none");
    session_ = new QLabel("Session: idle");
    replay_ = new QLabel("Replay: off");
    dropped_ = new QLabel("Dropped: 0");
    health_ = new QLabel("Health: ok");
    tracker_ = new QLabel("Tracker: offline");
    confidence_ = new QLabel("Confidence: --");
    hands_ = new QLabel("Hands: 0");
    cpu_ = new QProgressBar();
    gpu_ = new QProgressBar();
    cpu_->setRange(0, 100);
    gpu_->setRange(0, 100);
    metrics_layout->addWidget(fps_);
    metrics_layout->addWidget(latency_);
    metrics_layout->addWidget(gesture_);
    metrics_layout->addWidget(session_);
    metrics_layout->addWidget(replay_);
    metrics_layout->addWidget(dropped_);
    metrics_layout->addWidget(health_);
    metrics_layout->addWidget(tracker_);
    metrics_layout->addWidget(confidence_);
    metrics_layout->addWidget(hands_);
    metrics_layout->addWidget(cpu_);
    metrics_layout->addWidget(gpu_);

    timeline_ = new QPlainTextEdit();
    timeline_->setReadOnly(true);
    timeline_->setPlainText("Event timeline\n- dashboard online");

    layout->addWidget(top);
    layout->addWidget(metrics);
    layout->addWidget(timeline_, 1);
    setCentralWidget(root);

    connect(&timer_, &QTimer::timeout, this, &DashboardWindow::refresh_demo_state);
    timer_.start(250);
}

void DashboardWindow::apply_frame_telemetry(const arx::engine::telemetry::RuntimeTelemetryFrame& frame) {
    fps_->setText(QString("FPS: %1").arg(frame.frame_meta.fps, 0, 'f', 1));
    latency_->setText(QString("Latency: %1 ms").arg(frame.latency_ms, 0, 'f', 1));
    gesture_->setText(QString("Gesture: %1").arg(QString::fromStdString(frame.gesture)));
    session_->setText(QString("Session: %1").arg(frame.recording ? "recording" : "idle"));
    replay_->setText(QString("Replay: %1").arg(frame.replaying ? "on" : "off"));
    dropped_->setText(QString("Dropped: %1").arg(frame.dropped_frames));
    tracker_->setText(QString("Tracker: %1 | model:%2")
        .arg(QString::fromStdString(frame.tracker_state.empty() ? "unknown" : frame.tracker_state))
        .arg(frame.model_loaded ? "loaded" : "missing"));
    confidence_->setText(QString("Confidence: %1").arg(frame.top_hand_confidence, 0, 'f', 2));
    hands_->setText(QString("Hands: %1").arg(frame.raw_hand_count));
    health_->setText(frame.tracker_error.empty()
        ? QString("Health: ok")
        : QString("Health: %1").arg(QString::fromStdString(frame.tracker_error)));
    cpu_->setValue(static_cast<int>(std::min(100.0, frame.latency_ms * 3.0)));
    gpu_->setValue(static_cast<int>(frame.num_hands * 20));
    if (!frame.tracker_error.empty() && frame.tracker_error != last_tracker_error_) {
        append_timeline(QString("[tracker] %1").arg(QString::fromStdString(frame.tracker_error)));
        last_tracker_error_ = frame.tracker_error;
    } else if (frame.tracker_error.empty()) {
        last_tracker_error_.clear();
    }
}

void DashboardWindow::apply_gesture_event(const arx::engine::GestureRuntimeEvent& event) {
    gesture_->setText(QString("Gesture: %1").arg(arx::vision::gesture_name(event.gesture)));
    append_timeline(QString("[%1] %2 %3 conf=%4")
        .arg(event.timestamp_us)
        .arg(arx::vision::gesture_event_name(event.kind))
        .arg(arx::vision::gesture_name(event.gesture))
        .arg(event.confidence, 0, 'f', 2));
}

void DashboardWindow::refresh_demo_state() {
    ++frame_counter_;
    if (frame_counter_ % 20 == 0) {
        append_timeline(QString("- frame %1 | runtime heartbeat").arg(frame_counter_));
    }
}

void DashboardWindow::append_timeline(const QString& entry) {
    timeline_->appendPlainText(entry);
}

}  // namespace arx::dashboard
