// main.cpp

#include "core/MemoryCommand.h"
#include "core/ProjectRootRebindService.h"
#include "ui/mainwindow/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv)
{
    if (argc > 1 && (QString::fromLocal8Bit(argv[1]) == QStringLiteral("--help") || QString::fromLocal8Bit(argv[1]) == QStringLiteral("memory") || QString::fromLocal8Bit(argv[1]) == QStringLiteral("improvement") || QString::fromLocal8Bit(argv[1]) == QStringLiteral("project")))
    {
        QCoreApplication app(argc, argv);
        QTextStream output(stdout);
        QTextStream error(stderr);
        QStringList arguments;
        for (int index = 1; index < argc; ++index)
            arguments.append(QString::fromLocal8Bit(argv[index]));
        if (arguments.value(0) == QStringLiteral("project"))
            return runProjectRootRebindCommand(arguments, output, error);
        return runMemoryCommand(arguments, output, error);
    }

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
