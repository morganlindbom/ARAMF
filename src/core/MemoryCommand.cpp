#include "MemoryCommand.h"

#include "AramfPaths.h"
#include "FrameworkKnowledge.h"
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
            << "       aramf memory decision record --project <project-root> --id <id> --topic <topic> --summary <summary> [options]\n"
            << "       aramf memory decision supersede --project <project-root> --id <id> --replacement <id>\n"
            << "       aramf memory knowledge promote --project <project-root> --id <id>\n"
            << "       aramf memory checkpoint --project <project-root> --title <title> --summary <summary> [options]\n"
            << "       aramf memory validate --project <project-root>\n"
            << "       aramf memory cold-start --project <project-root>\n"
            << "       aramf memory refresh-contract --project <project-root>\n"
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
    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory")
        && (arguments.at(1) == QStringLiteral("validate") || arguments.at(1) == QStringLiteral("cold-start")
            || arguments.at(1) == QStringLiteral("refresh-contract"))) {
        if (arguments.size() != 4 || arguments.at(2) != QStringLiteral("--project")) {
            error << "error=project-is-required\n";
            return 2;
        }
        const QString projectRoot = QDir::cleanPath(QFileInfo(arguments.at(3)).absoluteFilePath());
        ProjectMemory memory;
        QString validationError;
        if (arguments.at(1) == QStringLiteral("refresh-contract")) {
            if (!memory.refreshMemoryContract(projectRoot, &validationError)
                || !memory.refreshMemoryInstructions(projectRoot, &validationError)) {
                error << "error=" << validationError << "\n";
                return 2;
            }
            output << "refreshed operation=memory-contract status=PASS\n";
            return 0;
        }
        const QJsonObject report = arguments.at(1) == QStringLiteral("validate")
            ? memory.validate(projectRoot, &validationError)
            : memory.validateColdStart(projectRoot, &validationError);
        if (!validationError.isEmpty() || report.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
            error << "error=" << (validationError.isEmpty() ? QStringLiteral("validation failed") : validationError) << "\n";
            return 2;
        }
        output << "validated operation=" << arguments.at(1) << " status=PASS\n";
        return 0;
    }

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("decision") && arguments.at(2) == QStringLiteral("record")) {
        const QSet<QString> valueOptions{QStringLiteral("--project"), QStringLiteral("--id"), QStringLiteral("--topic"),
                                         QStringLiteral("--summary"), QStringLiteral("--status"), QStringLiteral("--superseded-by")};
        QHash<QString, QString> options;
        for (int index = 3; index < arguments.size(); ++index) {
            const QString name = arguments.at(index);
            if (!valueOptions.contains(name) || index + 1 >= arguments.size()) {
                error << "error=invalid-argument:" << name << "\n";
                return 2;
            }
            options.insert(name, arguments.at(++index));
        }
        for (const QString& required : {QStringLiteral("--project"), QStringLiteral("--id"),
                                        QStringLiteral("--topic"), QStringLiteral("--summary")}) {
            if (!options.contains(required)) {
                error << "error=missing-argument:" << required << "\n";
                return 2;
            }
        }
        ProjectMemory memory;
        QString decisionError;
        if (!memory.recordDecision(QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
                                   options.value(QStringLiteral("--id")), options.value(QStringLiteral("--topic")),
                                   options.value(QStringLiteral("--summary")),
                                   options.value(QStringLiteral("--status"), QStringLiteral("current")),
                                   options.value(QStringLiteral("--superseded-by")), &decisionError)) {
            error << "error=" << decisionError << "\n";
            return 2;
        }
        output << "recorded decision=" << options.value(QStringLiteral("--id")) << "\n";
        return 0;
    }

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("decision") && arguments.at(2) == QStringLiteral("supersede")) {
        QHash<QString, QString> options;
        for (int index = 3; index + 1 < arguments.size(); index += 2) options.insert(arguments.at(index), arguments.at(index + 1));
        if (!options.contains(QStringLiteral("--project")) || !options.contains(QStringLiteral("--id"))
            || !options.contains(QStringLiteral("--replacement"))) {
            error << "error=project-id-and-replacement-are-required\n";
            return 2;
        }
        ProjectMemory memory;
        QString decisionError;
        if (!memory.supersedeDecision(QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
                                      options.value(QStringLiteral("--id")), options.value(QStringLiteral("--replacement")), &decisionError)) {
            error << "error=" << decisionError << "\n";
            return 2;
        }
        output << "superseded decision=" << options.value(QStringLiteral("--id")) << "\n";
        return 0;
    }

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("knowledge") && arguments.at(2) == QStringLiteral("promote")) {
        if (arguments.size() != 7 || arguments.at(3) != QStringLiteral("--project") || arguments.at(5) != QStringLiteral("--id")) {
            error << "error=project-and-id-are-required\n";
            return 2;
        }
        FrameworkKnowledgeService service;
        QString promotionError;
        if (!service.promoteToGlobal(QDir::cleanPath(QFileInfo(arguments.at(4)).absoluteFilePath()), arguments.at(6), &promotionError)) {
            error << "error=" << promotionError << "\n";
            return 2;
        }
        output << "promoted knowledge=" << arguments.at(6) << " path=" << service.globalLibraryPath() << "\n";
        return 0;
    }

    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("checkpoint")) {
        const QSet<QString> valueOptions{QStringLiteral("--project"), QStringLiteral("--title"),
                                         QStringLiteral("--summary"), QStringLiteral("--task"),
                                         QStringLiteral("--commit"), QStringLiteral("--verification-status")};
        QHash<QString, QString> options;
        for (int index = 2; index < arguments.size(); ++index) {
            const QString name = arguments.at(index);
            if (!valueOptions.contains(name) || index + 1 >= arguments.size() || arguments.at(index + 1).startsWith(QStringLiteral("--"))) {
                error << "error=invalid-argument:" << name << "\n";
                return 2;
            }
            options.insert(name, arguments.at(++index));
        }
        for (const QString& required : {QStringLiteral("--project"), QStringLiteral("--title"), QStringLiteral("--summary")}) {
            if (!options.contains(required)) {
                error << "error=missing-argument:" << required << "\n";
                return 2;
            }
        }
        ProjectMemory memory;
        QJsonObject result;
        QString checkpointError;
        if (!memory.recordCheckpoint(QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
                                     options.value(QStringLiteral("--title")), options.value(QStringLiteral("--summary")),
                                     options.value(QStringLiteral("--task")), options.value(QStringLiteral("--commit")),
                                     options.value(QStringLiteral("--verification-status")), &result, &checkpointError)) {
            error << "error=" << checkpointError << "\n";
            return 2;
        }
        output << "recorded checkpoint=" << result.value(QStringLiteral("id")).toString()
               << " productionSequence=" << result.value(QStringLiteral("productionSequence")).toInt()
               << " eventId=" << result.value(QStringLiteral("latestEventId")).toString() << "\n";
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
