#include "ui/mainwindow/MainWindow.h"
#include "ui/workflow/WorkflowWidget.h"

#include <QApplication>
#include <QListWidget>
#include <QScrollArea>
#include <QTest>
#include <QTimer>

namespace {
const QList<int> rows{1,2,3,4,5,6,7,8,10,11,12,13,15,16,17,19,20,22,23,25,26,27,28};
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    MainWindow window(0, 1000, 600);
    window.setWindowTitle(QStringLiteral("ARAMF — Manual Visual Validation Sweep"));
    window.show();
    auto* list = window.findChild<QListWidget*>();
    if (!list) return 2;

    auto* timer = new QTimer(&window);
    int phase = 0;
    int index = 0;
    QObject::connect(timer, &QTimer::timeout, &window, [&]() {
        if (phase == 0) {
            if (index < rows.size()) {
                list->scrollToItem(list->item(rows.at(index)), QAbstractItemView::PositionAtCenter);
                list->setCurrentRow(rows.at(index++));
                window.setWindowTitle(QStringLiteral("ARAMF — Visual sweep page %1 / 23").arg(index));
                return;
            }
            phase = 1;
            index = 0;
            list->setCurrentRow(19);
            for (int i = 0; i < 7; ++i) QTest::keyClick(&window, Qt::Key_Minus, Qt::ControlModifier);
            window.setWindowTitle(QStringLiteral("ARAMF — Visual sweep 30% zoom"));
            return;
        }
        if (phase == 1) {
            phase = 2;
            for (int i = 0; i < 12; ++i) QTest::keyClick(&window, Qt::Key_Plus, Qt::ControlModifier);
            window.setWindowTitle(QStringLiteral("ARAMF — Visual sweep 150% zoom"));
            return;
        }
        if (phase == 2) {
            phase = 3;
            window.resize(760, 480);
            window.setWindowTitle(QStringLiteral("ARAMF — Visual sweep narrow window"));
            return;
        }
        if (phase == 3) {
            phase = 4;
            window.resize(1200, 800);
            QTest::keyClick(&window, Qt::Key_0, Qt::ControlModifier);
            window.setWindowTitle(QStringLiteral("ARAMF — Visual sweep restored 100%"));
            return;
        }
        timer->stop();
        app.quit();
    });
    timer->start(1200);
    return app.exec();
}
