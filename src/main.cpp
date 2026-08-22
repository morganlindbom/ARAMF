// main.cpp

#include "ui/mainwindow/MainWindow.h"

#include <QApplication>

int main(int argc, char **argv)
{
    // Starts the ARAMF desktop application.

    QApplication app(argc, argv);

    constexpr int screen = 2;
    constexpr int windowWidth = 1000;
    constexpr int windowHeight = 600;

    MainWindow window(
        screen,
        windowWidth,
        windowHeight);

    window.show();

    return app.exec();
}