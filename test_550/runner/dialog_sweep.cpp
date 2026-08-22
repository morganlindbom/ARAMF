#include "ui/mainwindow/MainWindow.h"

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QTimer>

namespace {

QPushButton* button(MainWindow& window, const QString& text)
{
    for (auto* candidate : window.findChildren<QPushButton*>()) {
        if (candidate->text() == text) return candidate;
    }
    return nullptr;
}

}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MainWindow window(0, 1000, 600);
    window.setWindowTitle(QStringLiteral("ARAMF — Manual Dialog Validation"));
    window.show();

    auto* timer = new QTimer(&window);
    int stage = 0;
    bool wasModal = false;
    timer->setInterval(250);
    QObject::connect(timer, &QTimer::timeout, &window, [&] {
        auto* modal = QApplication::activeModalWidget();
        if (modal) {
            wasModal = true;
            return;
        }
        if (wasModal) {
            wasModal = false;
            ++stage;
            if (stage >= 1) {
                timer->stop();
                QTimer::singleShot(500, &app, &QCoreApplication::quit);
                return;
            }
        }

        QPushButton* next = nullptr;
        if (stage == 0) next = button(window, QStringLiteral("Save As"));
        if (next) next->click();
    });
    timer->start();
    return app.exec();
}
