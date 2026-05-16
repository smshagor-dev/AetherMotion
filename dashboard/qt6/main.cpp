#include <QApplication>

#include "dashboard/qt6/dashboard_window.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    arx::dashboard::DashboardWindow window;
    window.show();
    return app.exec();
}
