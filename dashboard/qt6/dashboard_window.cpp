#include "dashboard/qt6/dashboard_window.hpp"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

#include "vision/gesture_engine/gesture_types.hpp"

namespace arx::dashboard {

namespace {

QString discover_workspace() {
    QDir current(QDir::currentPath());
    if (current.exists("CMakeLists.txt") && current.exists("configs")) {
        return current.absolutePath();
    }

    QDir candidate(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 6; ++depth) {
        if (candidate.exists("CMakeLists.txt") && candidate.exists("configs")) {
            return candidate.absolutePath();
        }
        if (!candidate.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

QString process_state_text(QProcess::ProcessState state) {
    switch (state) {
    case QProcess::Starting:
        return "starting";
    case QProcess::Running:
        return "running";
    case QProcess::NotRunning:
    default:
        return "stopped";
    }
}

}  // namespace

DashboardWindow::DashboardWindow() {
    setWindowTitle("AetherMotion | ARX Operator Control Center");
    resize(1560, 940);
    build_ui();
    wire_processes();

    connect(&timer_, &QTimer::timeout, this, &DashboardWindow::refresh_demo_state);
    timer_.start(500);
}

DashboardWindow::~DashboardWindow() {
    stop_native_runtime();
    stop_legacy_services();
}

void DashboardWindow::build_ui() {
    auto* root = new QWidget(this);
    auto* root_layout = new QVBoxLayout(root);
    root_layout->setContentsMargins(16, 16, 16, 16);
    root_layout->setSpacing(12);

    root->setStyleSheet(
        "QWidget { background: #07111f; color: #dce9ff; font-size: 13px; }"
        "QFrame, QGroupBox { background: #0c1628; border: 1px solid #173153; border-radius: 10px; }"
        "QGroupBox { margin-top: 10px; padding-top: 12px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #8ecbff; }"
        "QPlainTextEdit, QLineEdit, QComboBox, QSpinBox { background: #08111d; border: 1px solid #173153; border-radius: 6px; padding: 6px; }"
        "QPushButton { background: #11345a; border: 1px solid #1d5b94; border-radius: 6px; padding: 8px 12px; font-weight: 600; }"
        "QPushButton:hover { background: #164474; }"
        "QPushButton:disabled { background: #172233; color: #61738c; border-color: #26384f; }"
        "QTabWidget::pane { border: 1px solid #173153; border-radius: 8px; }"
        "QTabBar::tab { background: #0c1628; padding: 9px 14px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #11345a; color: #8ee8ff; }");

    auto* header = new QFrame();
    auto* header_layout = new QHBoxLayout(header);
    auto* branding = new QVBoxLayout();
    title_ = new QLabel("AetherMotion");
    title_->setStyleSheet("font-size: 30px; font-weight: 800; color: #00e5ff;");
    subtitle_ = new QLabel("ARX real-time gesture intelligence, spatial interaction and runtime operations");
    subtitle_->setStyleSheet("font-size: 14px; color: #8aa3c7;");
    branding->addWidget(title_);
    branding->addWidget(subtitle_);
    header_layout->addLayout(branding, 1);

    native_status_ = new QLabel("Native: stopped");
    legacy_status_ = new QLabel("Services: stopped");
    native_status_->setStyleSheet("color: #ffcc66; font-weight: 700;");
    legacy_status_->setStyleSheet("color: #ffcc66; font-weight: 700;");
    header_layout->addWidget(native_status_);
    header_layout->addWidget(legacy_status_);
    root_layout->addWidget(header);

    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* controls = new QWidget();
    controls->setMinimumWidth(380);
    controls->setMaximumWidth(470);
    auto* control_layout = new QVBoxLayout(controls);
    control_layout->setContentsMargins(0, 0, 6, 0);

    auto* workspace_group = new QGroupBox("Workspace");
    auto* workspace_layout = new QVBoxLayout(workspace_group);
    auto* workspace_row = new QHBoxLayout();
    workspace_edit_ = new QLineEdit(discover_workspace());
    auto* workspace_browse = new QPushButton("Browse");
    workspace_row->addWidget(workspace_edit_, 1);
    workspace_row->addWidget(workspace_browse);
    workspace_layout->addLayout(workspace_row);
    workspace_layout->addWidget(new QLabel("Repository root containing CMakeLists.txt, configs/ and run.py"));
    control_layout->addWidget(workspace_group);

    auto* native_group = new QGroupBox("Native Runtime");
    auto* native_layout = new QFormLayout(native_group);
    mode_combo_ = new QComboBox();
    mode_combo_->addItems({
        "live",
        "record",
        "graphical-fusion",
        "replay",
        "fusion-replay",
        "tracker-smoke",
        "validate-replay"
    });
    camera_spin_ = new QSpinBox();
    camera_spin_->setRange(0, 32);
    session_edit_ = new QLineEdit();
    session_edit_->setPlaceholderText("sessions/session.jsonl (required for replay modes)");
    auto* session_row = new QWidget();
    auto* session_layout = new QHBoxLayout(session_row);
    session_layout->setContentsMargins(0, 0, 0, 0);
    auto* session_browse = new QPushButton("...");
    session_browse->setMaximumWidth(42);
    session_layout->addWidget(session_edit_, 1);
    session_layout->addWidget(session_browse);
    native_layout->addRow("Mode", mode_combo_);
    native_layout->addRow("Camera", camera_spin_);
    native_layout->addRow("Session", session_row);

    auto* native_buttons = new QWidget();
    auto* native_button_layout = new QGridLayout(native_buttons);
    native_button_layout->setContentsMargins(0, 4, 0, 0);
    start_native_button_ = new QPushButton("Start Runtime");
    stop_native_button_ = new QPushButton("Stop Runtime");
    check_models_button_ = new QPushButton("Check Models");
    native_button_layout->addWidget(start_native_button_, 0, 0);
    native_button_layout->addWidget(stop_native_button_, 0, 1);
    native_button_layout->addWidget(check_models_button_, 1, 0, 1, 2);
    native_layout->addRow(native_buttons);
    control_layout->addWidget(native_group);

    auto* legacy_group = new QGroupBox("Legacy / Remote Services");
    auto* legacy_layout = new QVBoxLayout(legacy_group);
    legacy_go_ = new QCheckBox("Go control plane / WebSocket API");
    legacy_ai_ = new QCheckBox("Python MediaPipe AI layer");
    legacy_go_->setChecked(true);
    legacy_ai_->setChecked(true);
    legacy_layout->addWidget(legacy_go_);
    legacy_layout->addWidget(legacy_ai_);
    auto* legacy_buttons = new QHBoxLayout();
    start_legacy_button_ = new QPushButton("Start Services");
    stop_legacy_button_ = new QPushButton("Stop Services");
    legacy_buttons->addWidget(start_legacy_button_);
    legacy_buttons->addWidget(stop_legacy_button_);
    legacy_layout->addLayout(legacy_buttons);
    legacy_layout->addWidget(new QLabel("Uses run.py in headless service mode; this Qt window remains the operator console."));
    control_layout->addWidget(legacy_group);
    control_layout->addStretch(1);

    splitter->addWidget(controls);

    tabs_ = new QTabWidget();

    auto* overview = new QWidget();
    auto* overview_layout = new QVBoxLayout(overview);
    auto* metrics = new QFrame();
    auto* metrics_layout = new QGridLayout(metrics);
    fps_ = new QLabel("FPS: --");
    latency_ = new QLabel("Latency: --");
    gesture_ = new QLabel("Gesture: none");
    session_ = new QLabel("Session: idle");
    replay_ = new QLabel("Replay: off");
    dropped_ = new QLabel("Dropped: 0");
    health_ = new QLabel("Health: waiting");
    tracker_ = new QLabel("Tracker: offline");
    confidence_ = new QLabel("Confidence: --");
    hands_ = new QLabel("Hands: 0");
    cpu_ = new QProgressBar();
    gpu_ = new QProgressBar();
    cpu_->setRange(0, 100);
    gpu_->setRange(0, 100);
    cpu_->setFormat("Load proxy %p%");
    gpu_->setFormat("Tracking load %p%");

    const QList<QLabel*> metric_labels = {
        fps_, latency_, gesture_, session_, replay_, dropped_, health_, tracker_, confidence_, hands_
    };
    for (int i = 0; i < metric_labels.size(); ++i) {
        metrics_layout->addWidget(metric_labels[i], i / 2, i % 2);
    }
    metrics_layout->addWidget(cpu_, 5, 0);
    metrics_layout->addWidget(gpu_, 5, 1);
    overview_layout->addWidget(metrics);

    auto* architecture = new QFrame();
    auto* architecture_layout = new QVBoxLayout(architecture);
    auto* architecture_title = new QLabel("Operational topology");
    architecture_title->setStyleSheet("font-size: 17px; font-weight: 700; color: #8ee8ff;");
    architecture_layout->addWidget(architecture_title);
    architecture_layout->addWidget(new QLabel(
        "Native: Camera → Tracking → Smoothing → Gesture Engine → Spatial Interaction → AR/Fusion → Telemetry/Replay\n"
        "Services: Python AI (legacy) → ZeroMQ → Go Control Plane → WebSocket/API\n"
        "Operator: this console controls both paths and centralizes logs, modes, model checks and replay selection."));
    overview_layout->addWidget(architecture);
    overview_layout->addStretch(1);

    runtime_log_ = new QPlainTextEdit();
    runtime_log_->setReadOnly(true);
    runtime_log_->setMaximumBlockCount(5000);
    runtime_log_->setPlainText("[operator] Control center ready.\n");

    timeline_ = new QPlainTextEdit();
    timeline_->setReadOnly(true);
    timeline_->setMaximumBlockCount(3000);
    timeline_->setPlainText("Event timeline\n- operator console online");

    tabs_->addTab(overview, "Overview");
    tabs_->addTab(runtime_log_, "Runtime Logs");
    tabs_->addTab(timeline_, "Events");
    splitter->addWidget(tabs_);
    splitter->setStretchFactor(1, 1);
    root_layout->addWidget(splitter, 1);
    setCentralWidget(root);

    set_native_running(false);
    set_legacy_running(false);

    connect(workspace_browse, &QPushButton::clicked, this, &DashboardWindow::browse_workspace);
    connect(session_browse, &QPushButton::clicked, this, &DashboardWindow::browse_session);
    connect(start_native_button_, &QPushButton::clicked, this, &DashboardWindow::start_native_runtime);
    connect(stop_native_button_, &QPushButton::clicked, this, &DashboardWindow::stop_native_runtime);
    connect(check_models_button_, &QPushButton::clicked, this, &DashboardWindow::check_models);
    connect(start_legacy_button_, &QPushButton::clicked, this, &DashboardWindow::start_legacy_services);
    connect(stop_legacy_button_, &QPushButton::clicked, this, &DashboardWindow::stop_legacy_services);
}

void DashboardWindow::wire_processes() {
    native_process_.setProcessChannelMode(QProcess::MergedChannels);
    legacy_process_.setProcessChannelMode(QProcess::MergedChannels);

    connect(&native_process_, &QProcess::readyReadStandardOutput, this, &DashboardWindow::drain_native_output);
    connect(&legacy_process_, &QProcess::readyReadStandardOutput, this, &DashboardWindow::drain_legacy_output);
    connect(&native_process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &DashboardWindow::native_process_finished);
    connect(&legacy_process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &DashboardWindow::legacy_process_finished);
    connect(&native_process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        runtime_log_->appendPlainText(QString("[native][error] %1").arg(native_process_.errorString()));
    });
    connect(&legacy_process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        runtime_log_->appendPlainText(QString("[services][error] %1").arg(legacy_process_.errorString()));
    });
}

void DashboardWindow::start_native_runtime() {
    if (native_process_.state() != QProcess::NotRunning) {
        runtime_log_->appendPlainText("[operator] Native runtime is already active.");
        return;
    }
    if (!ensure_workspace_valid()) {
        return;
    }

    const QString mode = mode_combo_->currentText();
    if ((mode == "replay" || mode == "fusion-replay" || mode == "validate-replay") &&
        session_edit_->text().trimmed().isEmpty()) {
        runtime_log_->appendPlainText("[operator][error] Selected replay mode requires a session JSONL path.");
        tabs_->setCurrentWidget(runtime_log_);
        return;
    }

    const QString executable = resolve_native_runtime();
    if (executable.isEmpty()) {
        runtime_log_->appendPlainText(
            "[operator][error] arx_runtime was not found. Build the desktop/headless preset first, then retry.");
        tabs_->setCurrentWidget(runtime_log_);
        return;
    }

    native_process_.setWorkingDirectory(workspace_root());
    const QStringList args = native_arguments(false);
    runtime_log_->appendPlainText(QString("[operator] Launching native: %1 %2").arg(executable, args.join(' ')));
    native_process_.start(executable, args);
    if (!native_process_.waitForStarted(2500)) {
        runtime_log_->appendPlainText(QString("[native][error] %1").arg(native_process_.errorString()));
        set_native_running(false);
        return;
    }
    set_native_running(true);
    tabs_->setCurrentWidget(runtime_log_);
}

void DashboardWindow::stop_native_runtime() {
    if (native_process_.state() == QProcess::NotRunning) {
        set_native_running(false);
        return;
    }
    runtime_log_->appendPlainText("[operator] Stopping native runtime...");
    native_process_.terminate();
    if (!native_process_.waitForFinished(2000)) {
        native_process_.kill();
        native_process_.waitForFinished(1500);
    }
    set_native_running(false);
}

void DashboardWindow::check_models() {
    if (native_process_.state() != QProcess::NotRunning) {
        runtime_log_->appendPlainText("[operator][error] Stop the native runtime before running a model check.");
        tabs_->setCurrentWidget(runtime_log_);
        return;
    }
    if (!ensure_workspace_valid()) {
        return;
    }
    const QString executable = resolve_native_runtime();
    if (executable.isEmpty()) {
        runtime_log_->appendPlainText("[operator][error] arx_runtime was not found for model validation.");
        tabs_->setCurrentWidget(runtime_log_);
        return;
    }
    native_process_.setWorkingDirectory(workspace_root());
    const QStringList args = native_arguments(true);
    runtime_log_->appendPlainText(QString("[operator] Validating model assets: %1 %2").arg(executable, args.join(' ')));
    native_process_.start(executable, args);
    if (native_process_.waitForStarted(2500)) {
        set_native_running(true);
        tabs_->setCurrentWidget(runtime_log_);
    } else {
        runtime_log_->appendPlainText(QString("[native][error] %1").arg(native_process_.errorString()));
    }
}

void DashboardWindow::start_legacy_services() {
    if (legacy_process_.state() != QProcess::NotRunning) {
        runtime_log_->appendPlainText("[operator] Legacy/remote services are already active.");
        return;
    }
    if (!legacy_go_->isChecked() && !legacy_ai_->isChecked()) {
        runtime_log_->appendPlainText("[operator][error] Select at least one service to start.");
        return;
    }
    if (!ensure_workspace_valid()) {
        return;
    }

    const QString run_script = QDir(workspace_root()).filePath("run.py");
    if (!QFileInfo::exists(run_script)) {
        runtime_log_->appendPlainText("[operator][error] run.py was not found in the selected workspace.");
        tabs_->setCurrentWidget(runtime_log_);
        return;
    }
    const QString python = resolve_python();
    if (python.isEmpty()) {
        runtime_log_->appendPlainText(
            "[operator][error] Python was not found. Set ARX_PYTHON or add python/python3 to PATH.");
        tabs_->setCurrentWidget(runtime_log_);
        return;
    }

    QStringList args{run_script, "--skip-dashboard", "--camera", QString::number(camera_spin_->value())};
    if (!legacy_go_->isChecked()) {
        args << "--skip-go";
    }
    if (!legacy_ai_->isChecked()) {
        args << "--skip-ai";
    }

    legacy_process_.setWorkingDirectory(workspace_root());
    runtime_log_->appendPlainText(QString("[operator] Launching services: %1 %2").arg(python, args.join(' ')));
    legacy_process_.start(python, args);
    if (!legacy_process_.waitForStarted(2500)) {
        runtime_log_->appendPlainText(QString("[services][error] %1").arg(legacy_process_.errorString()));
        set_legacy_running(false);
        return;
    }
    set_legacy_running(true);
    tabs_->setCurrentWidget(runtime_log_);
}

void DashboardWindow::stop_legacy_services() {
    if (legacy_process_.state() == QProcess::NotRunning) {
        set_legacy_running(false);
        return;
    }
    runtime_log_->appendPlainText("[operator] Stopping legacy/remote services...");
    legacy_process_.terminate();
    if (!legacy_process_.waitForFinished(2500)) {
        legacy_process_.kill();
        legacy_process_.waitForFinished(1500);
    }
    set_legacy_running(false);
}

void DashboardWindow::browse_workspace() {
    const QString selected = QFileDialog::getExistingDirectory(this, "Select AetherMotion workspace", workspace_root());
    if (!selected.isEmpty()) {
        workspace_edit_->setText(QDir::cleanPath(selected));
    }
}

void DashboardWindow::browse_session() {
    const QString selected = QFileDialog::getOpenFileName(
        this,
        "Select replay session",
        QDir(workspace_root()).filePath("sessions"),
        "ARX session (*.jsonl);;All files (*.*)");
    if (!selected.isEmpty()) {
        session_edit_->setText(QDir::toNativeSeparators(selected));
    }
}

void DashboardWindow::drain_native_output() {
    append_runtime_log("native", native_process_.readAllStandardOutput());
}

void DashboardWindow::drain_legacy_output() {
    append_runtime_log("services", legacy_process_.readAllStandardOutput());
}

void DashboardWindow::native_process_finished(int exit_code, QProcess::ExitStatus status) {
    drain_native_output();
    runtime_log_->appendPlainText(QString("[native] finished exit=%1 status=%2")
        .arg(exit_code)
        .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
    set_native_running(false);
}

void DashboardWindow::legacy_process_finished(int exit_code, QProcess::ExitStatus status) {
    drain_legacy_output();
    runtime_log_->appendPlainText(QString("[services] finished exit=%1 status=%2")
        .arg(exit_code)
        .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
    set_legacy_running(false);
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
    gpu_->setValue(static_cast<int>(std::min<std::size_t>(5, frame.num_hands) * 20));
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
    native_status_->setText(QString("Native: %1").arg(process_state_text(native_process_.state())));
    legacy_status_->setText(QString("Services: %1").arg(process_state_text(legacy_process_.state())));
}

void DashboardWindow::append_timeline(const QString& entry) {
    timeline_->appendPlainText(entry);
}

void DashboardWindow::append_runtime_log(const QString& source, const QByteArray& bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    const QString text = QString::fromLocal8Bit(bytes).trimmed();
    if (text.isEmpty()) {
        return;
    }
    const auto lines = text.split('\n');
    for (const QString& line : lines) {
        runtime_log_->appendPlainText(QString("[%1] %2").arg(source, line.trimmed()));
    }
}

void DashboardWindow::set_native_running(bool running) {
    start_native_button_->setEnabled(!running);
    check_models_button_->setEnabled(!running);
    stop_native_button_->setEnabled(running);
    native_status_->setStyleSheet(running
        ? "color: #55ff99; font-weight: 700;"
        : "color: #ffcc66; font-weight: 700;");
    native_status_->setText(QString("Native: %1").arg(running ? "running" : "stopped"));
}

void DashboardWindow::set_legacy_running(bool running) {
    start_legacy_button_->setEnabled(!running);
    stop_legacy_button_->setEnabled(running);
    legacy_status_->setStyleSheet(running
        ? "color: #55ff99; font-weight: 700;"
        : "color: #ffcc66; font-weight: 700;");
    legacy_status_->setText(QString("Services: %1").arg(running ? "running" : "stopped"));
}

QString DashboardWindow::workspace_root() const {
    return QDir::cleanPath(workspace_edit_->text().trimmed());
}

QString DashboardWindow::resolve_native_runtime() const {
#ifdef _WIN32
    const QString binary_name = "arx_runtime.exe";
#else
    const QString binary_name = "arx_runtime";
#endif
    const QDir root(workspace_root());
    const QStringList candidates = {
        root.filePath(binary_name),
        root.filePath("build/desktop-dev/" + binary_name),
        root.filePath("build/headless-dev/" + binary_name),
        root.filePath("build/release-headless/" + binary_name),
        root.filePath("build_qt/Release/" + binary_name),
        root.filePath("build_qt/Debug/" + binary_name),
        QDir(QCoreApplication::applicationDirPath()).filePath(binary_name)
    };
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return {};
}

QString DashboardWindow::resolve_python() const {
    const QString configured = qEnvironmentVariable("ARX_PYTHON").trimmed();
    if (!configured.isEmpty() && QFileInfo::exists(configured)) {
        return configured;
    }
#ifdef _WIN32
    return QStandardPaths::findExecutable("python");
#else
    QString python = QStandardPaths::findExecutable("python3");
    if (python.isEmpty()) {
        python = QStandardPaths::findExecutable("python");
    }
    return python;
#endif
}

QStringList DashboardWindow::native_arguments(bool model_check_only) const {
    QStringList args;
    if (model_check_only) {
        args << "--check-models";
    } else {
        args << "--mode" << mode_combo_->currentText();
    }
    args << "--camera" << QString::number(camera_spin_->value());
    if (!session_edit_->text().trimmed().isEmpty() && !model_check_only) {
        args << "--session" << QDir::fromNativeSeparators(session_edit_->text().trimmed());
    }
    return args;
}

bool DashboardWindow::ensure_workspace_valid() {
    const QDir root(workspace_root());
    const bool valid = root.exists("CMakeLists.txt") && root.exists("configs");
    if (!valid) {
        runtime_log_->appendPlainText(
            "[operator][error] Workspace is invalid. Select the repository root containing CMakeLists.txt and configs/.");
        tabs_->setCurrentWidget(runtime_log_);
    }
    return valid;
}

}  // namespace arx::dashboard
