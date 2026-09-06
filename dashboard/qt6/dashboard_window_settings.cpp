#include "dashboard/qt6/dashboard_window.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QSettings>
#include <QSpinBox>

namespace arx::dashboard {

namespace {

constexpr int kOperatorSettingsSchema = 1;
constexpr auto kOrganization = "AetherMotion";
constexpr auto kApplication = "OperatorControlCenter";

bool valid_workspace(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return false;
    }
    const QDir root(path);
    return root.exists("CMakeLists.txt") && root.exists("configs");
}

}  // namespace

void DashboardWindow::restore_operator_settings() {
    QSettings settings(kOrganization, kApplication);
    const int schema = settings.value("operator/schema_version", 0).toInt();

    const QByteArray geometry = settings.value("window/geometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    if (schema != kOperatorSettingsSchema) {
        return;
    }

    const QString workspace = settings.value("operator/workspace").toString();
    if (valid_workspace(workspace)) {
        workspace_edit_->setText(QDir(workspace).absolutePath());
    }

    const QString mode = settings.value("operator/mode").toString();
    const int mode_index = mode_combo_->findText(mode);
    if (mode_index >= 0) {
        mode_combo_->setCurrentIndex(mode_index);
    }

    camera_spin_->setValue(settings.value("operator/camera", camera_spin_->value()).toInt());
    session_edit_->setText(settings.value("operator/session", session_edit_->text()).toString());
    legacy_go_->setChecked(settings.value("operator/legacy_go", legacy_go_->isChecked()).toBool());
    legacy_ai_->setChecked(settings.value("operator/legacy_ai", legacy_ai_->isChecked()).toBool());

    append_timeline("operator settings restored (schema v1)");
}

void DashboardWindow::save_operator_settings() const {
    QSettings settings(kOrganization, kApplication);
    settings.setValue("operator/schema_version", kOperatorSettingsSchema);
    settings.setValue("operator/workspace", workspace_edit_->text().trimmed());
    settings.setValue("operator/mode", mode_combo_->currentText());
    settings.setValue("operator/camera", camera_spin_->value());
    settings.setValue("operator/session", session_edit_->text().trimmed());
    settings.setValue("operator/legacy_go", legacy_go_->isChecked());
    settings.setValue("operator/legacy_ai", legacy_ai_->isChecked());
    settings.setValue("window/geometry", saveGeometry());
    settings.sync();
}

void DashboardWindow::apply_runtime_command_result(
    const QString& name,
    const QString& status,
    const QString& detail,
    const QString& request_id) {
    if (name == "ping") {
        return;
    }

    QString entry = QString("control %1 -> %2").arg(name, status);
    if (!detail.isEmpty()) {
        entry += QString(" (%1)").arg(detail);
    }
    if (!request_id.isEmpty()) {
        entry += QString(" [%1]").arg(request_id);
    }

    append_timeline(entry);
    runtime_log_->appendPlainText(QString("[control] %1").arg(entry));
}

void DashboardWindow::apply_runtime_status(
    const QString& state,
    bool paused,
    bool shutdown_requested,
    const QString& mode) {
    QString effective_state = state;
    if (shutdown_requested) {
        effective_state = "stopping";
    } else if (paused) {
        effective_state = "paused";
    } else if (effective_state.isEmpty()) {
        effective_state = "running";
    }

    native_status_->setText(
        QString("Native: %1%2")
            .arg(effective_state)
            .arg(mode.isEmpty() ? QString() : QString(" (%1)").arg(mode)));

    if (shutdown_requested) {
        native_status_->setStyleSheet("color: #ff9f68; font-weight: 700;");
    } else if (paused) {
        native_status_->setStyleSheet("color: #ffd166; font-weight: 700;");
    } else {
        native_status_->setStyleSheet("color: #72f1b8; font-weight: 700;");
    }
}

}  // namespace arx::dashboard
