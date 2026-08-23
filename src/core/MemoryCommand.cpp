#include "MemoryCommand.h"

#include "AramfPaths.h"
#include "ProjectMemory.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

namespace {
void printUsage(QTextStream& stream)
{
    stream << "Usage: aramf memory record --project <project-root> --operation <operation> [options]\n"
            << "Operations: task-start, task-complete, build-result, test-result, validation-result\n"
            << "Common options: --task <title> --status PASS|FAIL --summary <text> --detail <text>\n"
            << "Build options: --build-system <name> --configuration <name>\n"
            << "Test options: --suite <name> --passed <count> --failed <count> --total <count>\n"
            << "Other options: --category <name> --issue <reference>\n";
}

bool boundedOption(const QHash<QString, QString>& options,
                   const QString& name,
                   int maximum,
                   QJsonObject& fields,
                   QTextStream& error)
{
    if (!options.contains(name)) return true;
    const QString value = options.value(name);
    if (value.size() > maximum) {
        error << "error=option-too-long:" << name << "\n";
        return false;
    }
    fields.insert(name.mid(2), value);
    return true;
}

bool boundedIntegerOption(const QHash<QString, QString>& options,
                          const QString& name,
                          QJsonObject& fields,
                          QTextStream& error)
{
    if (!options.contains(name)) return true;
    bool ok = false;
    const int value = options.value(name).toInt(&ok);
    if (!ok || value < 0) {
        error << "error=invalid-integer:" << name << "\n";
        return false;
    }
    fields.insert(name.mid(2), value);
    return true;
}
}

int runMemoryCommand(const QStringList& arguments, QTextStream& output, QTextStream& error)
{
    if (arguments.size() == 1 && arguments.first() == QStringLiteral("--help")) {
        printUsage(output);
        return 0;
    }
    if (arguments.size() < 2 || arguments.at(0) != QStringLiteral("memory")
        || arguments.at(1) != QStringLiteral("record")) {
        printUsage(error);
        return 2;
    }

    const QSet<QString> valueOptions{
        QStringLiteral("--project"), QStringLiteral("--operation"), QStringLiteral("--task"),
        QStringLiteral("--status"), QStringLiteral("--summary"), QStringLiteral("--detail"),
        QStringLiteral("--category"), QStringLiteral("--issue"), QStringLiteral("--build-system"),
        QStringLiteral("--configuration"), QStringLiteral("--suite"), QStringLiteral("--passed"),
        QStringLiteral("--failed"), QStringLiteral("--total")};
    QHash<QString, QString> options;
    for (int index = 2; index < arguments.size(); ++index) {
        const QString name = arguments.at(index);
        if (!valueOptions.contains(name) || index + 1 >= arguments.size() || arguments.at(index + 1).startsWith(QStringLiteral("--"))) {
            error << "error=invalid-argument:" << name << "\n";
            return 2;
        }
        options.insert(name, arguments.at(++index));
    }
    if (!options.contains(QStringLiteral("--project")) || !options.contains(QStringLiteral("--operation"))) {
        error << "error=project-and-operation-are-required\n";
        return 2;
    }

    const QString projectRoot = QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath());
    if (!QDir(projectRoot).exists()) {
        error << "error=project-path-not-found\n";
        return 2;
    }
    if (!QDir(QDir(projectRoot).filePath(AramfPaths::ControlDirectory)).exists()) {
        error << "error=ARAMF_WORKER-not-found\n";
        return 2;
    }

    QJsonObject fields;
    if (!boundedOption(options, QStringLiteral("--task"), 512, fields, error)
        || !boundedOption(options, QStringLiteral("--summary"), 1024, fields, error)
        || !boundedOption(options, QStringLiteral("--detail"), 1024, fields, error)
        || !boundedOption(options, QStringLiteral("--category"), 128, fields, error)
        || !boundedOption(options, QStringLiteral("--issue"), 256, fields, error)
        || !boundedOption(options, QStringLiteral("--build-system"), 128, fields, error)
        || !boundedOption(options, QStringLiteral("--configuration"), 128, fields, error)
        || !boundedOption(options, QStringLiteral("--suite"), 256, fields, error)) return 2;
    if (options.contains(QStringLiteral("--status"))) fields.insert(QStringLiteral("status"), options.value(QStringLiteral("--status")));
    if (!boundedIntegerOption(options, QStringLiteral("--passed"), fields, error)
        || !boundedIntegerOption(options, QStringLiteral("--failed"), fields, error)
        || !boundedIntegerOption(options, QStringLiteral("--total"), fields, error)) return 2;

    ProjectMemory memory;
    QJsonObject result;
    QString recordingError;
    if (!memory.recordOperation(projectRoot, options.value(QStringLiteral("--operation")), fields, &result, &recordingError)) {
        error << "error=" << recordingError << "\n";
        return 2;
    }
    output << "recorded operation=" << result.value(QStringLiteral("operation")).toString()
           << " eventType=" << result.value(QStringLiteral("eventType")).toString()
           << " sequence=" << result.value(QStringLiteral("sequenceNumber")).toInt()
           << " eventId=" << result.value(QStringLiteral("eventId")).toString() << "\n";
    return 0;
}
