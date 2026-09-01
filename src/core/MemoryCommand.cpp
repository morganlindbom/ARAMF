#include "MemoryCommand.h"

#include "AramfPaths.h"
#include "FrameworkKnowledge.h"
#include "ImprovementBacklog.h"
#include "ProjectMemory.h"
#include "ProjectMemoryCompaction.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTextStream>

namespace {
void printUsage(QTextStream& stream)
{
    stream << "Usage: aramf memory record --project <project-root> --operation <operation> [options]\n"
            << "       aramf memory events --project <project-root> [--format json]\n"
            << "       aramf memory event --project <project-root> --id <event-id> [--format json]\n"
            << "       aramf memory decisions --project <project-root> [--format json]\n"
            << "       aramf memory decision --project <project-root> --id <decision-id> [--format json]\n"
            << "       aramf memory checkpoints --project <project-root> [--format json]\n"
            << "       aramf memory checkpoint get --project <project-root> --id <checkpoint-id> [--format json]\n"
            << "       aramf memory decision record --project <project-root> --id <id> --topic <topic> --summary <summary> [options]\n"
            << "       aramf memory decision supersede --project <project-root> --id <id> --replacement <id>\n"
       << "       aramf memory knowledge promote --project <project-root> --id <id>\n"
            << "       aramf memory knowledge propose --project <project-root> --title <title> --lesson <lesson> --scopes <csv> --evidence <text> [--portable true|false]\n"
            << "       aramf memory knowledge approve --project <project-root> --id <id> --source <source>\n"
            << "       aramf memory override --project <project-root> --instruction <text> --rule <rule> --reason <reason> --scope <scope> --action <action> [--affected-files <csv>] [--affected-systems <csv>] [--persistent-policy true|false]\n"
            << "       aramf memory override knowledge --project <project-root> --instruction <text> --title <title> --lesson <lesson> --scopes <csv> --evidence <text> --scope project [--portable true|false]\n"
            << "       aramf memory checkpoint --project <project-root> --title <title> --summary <summary> [options]\n"
            << "       aramf memory validate --project <project-root>\n"
            << "       aramf memory cold-start --project <project-root>\n"
            << "       aramf memory compact --project <project-root> [--dry-run] [--approve]\n"
            << "       aramf improvement report --project <project-root> --title <title> --observation <observation> [options]\n"
            << "       aramf improvement normalize --project <project-root> --project-id <id> --project-name <name>\n"
            << "       aramf improvement list [--stage <stage>] [--status <status>] [--project <project>]\n"
            << "       aramf improvement triage --id <gap-id> --action <action> [options]\n"
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

QHash<QString, QString> parseImprovementOptions(const QStringList& arguments, int start, QTextStream& error)
{
    const QSet<QString> allowed{QStringLiteral("--project"), QStringLiteral("--title"), QStringLiteral("--observation"),
                                QStringLiteral("--expected"), QStringLiteral("--area"), QStringLiteral("--evidence"),
                                QStringLiteral("--task"), QStringLiteral("--agent"), QStringLiteral("--project-id"), QStringLiteral("--project-name"), QStringLiteral("--stage"),
                                QStringLiteral("--status"), QStringLiteral("--id"), QStringLiteral("--action"),
                                QStringLiteral("--duplicate-of"), QStringLiteral("--priority")};
    QHash<QString, QString> options;
    for (int index = start; index < arguments.size(); ++index) {
        const auto name = arguments.at(index);
        if (!allowed.contains(name) || index + 1 >= arguments.size() || arguments.at(index + 1).startsWith(QStringLiteral("--"))) {
            error << "error=invalid-argument:" << name << "\n";
            return {};
        }
        options.insert(name, arguments.at(++index));
    }
    return options;
}

void printImprovementItem(QTextStream& output, const QJsonObject& item)
{
    output << item.value(QStringLiteral("id")).toString() << " "
           << item.value(QStringLiteral("todoId")).toString() << " "
           << item.value(QStringLiteral("stage")).toString() << " "
           << item.value(QStringLiteral("status")).toString() << " "
           << item.value(QStringLiteral("title")).toString() << "\n";
}

QHash<QString, QString> parseReadOptions(const QStringList& arguments, int start, QTextStream& error)
{
    const QSet<QString> allowed{QStringLiteral("--project"), QStringLiteral("--id"),
                                QStringLiteral("--task"), QStringLiteral("--event-type"),
                                QStringLiteral("--topic"), QStringLiteral("--format"),
                                QStringLiteral("--current")};
    QHash<QString, QString> options;
    for (int index = start; index < arguments.size(); ++index) {
        const QString name = arguments.at(index);
        if (!allowed.contains(name)) {
            error << "error=invalid-argument:" << name << "\n";
            return {};
        }
        if (name == QStringLiteral("--current")) {
            options.insert(name, QStringLiteral("true"));
            continue;
        }
        if (index + 1 >= arguments.size() || arguments.at(index + 1).startsWith(QStringLiteral("--"))) {
            error << "error=missing-value:" << name << "\n";
            return {};
        }
        options.insert(name, arguments.at(++index));
    }
    return options;
}

void printObjects(QTextStream& output, const QList<QJsonObject>& values, bool json)
{
    if (json) {
        QJsonArray array;
        for (const auto& value : values) array.append(value);
        output << QJsonDocument(array).toJson(QJsonDocument::Compact) << "\n";
        return;
    }
    for (const auto& value : values) output << QJsonDocument(value).toJson(QJsonDocument::Compact) << "\n";
}

void printObject(QTextStream& output, const QJsonObject& value, bool json)
{
    output << QJsonDocument(value).toJson(json ? QJsonDocument::Compact : QJsonDocument::Indented) << "\n";
}
}

int runMemoryCommand(const QStringList& arguments, QTextStream& output, QTextStream& error)
{
    if (arguments.size() == 1 && arguments.first() == QStringLiteral("--help")) {
        printUsage(output);
        return 0;
    }
    if (!arguments.isEmpty() && arguments.first() == QStringLiteral("improvement")) {
        if (arguments.size() == 2 && arguments.at(1) == QStringLiteral("--help")) {
            output << "Usage: aramf improvement report --project <root> --title <title> --observation <observation> [options]\n"
                    << "       aramf improvement normalize --project <root> --project-id <id> --project-name <name>\n"
                    << "       aramf improvement list [--stage <stage>] [--status <status>] [--project <project>]\n"
                    << "       aramf improvement triage --id <gap-id> --action promote|project-specific|duplicate|needs-evidence|already-resolved|reject [options]\n";
            return 0;
        }
        if (arguments.size() < 2) { printUsage(error); return 2; }
        ImprovementBacklogService service;
        const auto operation = arguments.at(1);
        if (operation == QStringLiteral("report")) {
            const auto options = parseImprovementOptions(arguments, 2, error);
            for (const auto& required : {QStringLiteral("--project"), QStringLiteral("--title"), QStringLiteral("--observation")})
                if (!options.contains(required)) { error << "error=missing-argument:" << required << "\n"; return 2; }
            QStringList evidence;
            if (options.contains(QStringLiteral("--evidence"))) evidence.append(options.value(QStringLiteral("--evidence")));
            QJsonObject result;
            QString operationError;
            auto projectIdentity = service.projectIdentity(options.value(QStringLiteral("--project")));
            if (options.contains(QStringLiteral("--project-id"))) projectIdentity.projectId = options.value(QStringLiteral("--project-id")).trimmed();
            if (options.contains(QStringLiteral("--project-name"))) projectIdentity.projectName = options.value(QStringLiteral("--project-name")).trimmed();
            if (!service.reportWithIdentity(projectIdentity, options.value(QStringLiteral("--title")), options.value(QStringLiteral("--observation")),
                                             options.value(QStringLiteral("--expected")), options.value(QStringLiteral("--area")), evidence,
                                             options.value(QStringLiteral("--task")), options.value(QStringLiteral("--agent")), &result, &operationError)) {
                error << "error=" << operationError << "\n"; return 2;
            }
            output << "reported improvement=" << result.value(QStringLiteral("id")).toString()
                   << " outcome=" << result.value(QStringLiteral("outcome")).toString() << " path=" << service.backlogPath() << "\n";
            return 0;
        }
        if (operation == QStringLiteral("normalize")) {
            const auto options = parseImprovementOptions(arguments, 2, error);
            for (const auto& required : {QStringLiteral("--project"), QStringLiteral("--project-id"), QStringLiteral("--project-name")})
                if (!options.contains(required)) { error << "error=missing-argument:" << required << "\n"; return 2; }
            const ImprovementBacklogProjectIdentity projectIdentity{options.value(QStringLiteral("--project-id")).trimmed(), options.value(QStringLiteral("--project-name")).trimmed(), options.value(QStringLiteral("--project"))};
            QString operationError;
            if (!service.normalizeProjectIdentity(projectIdentity, &operationError)) { error << "error=" << operationError << "\n"; return 2; }
            output << "normalized project=" << projectIdentity.projectId << " path=" << service.backlogPath() << "\n";
            return 0;
        }
        if (operation == QStringLiteral("list")) {
            const auto options = parseImprovementOptions(arguments, 2, error);
            QString operationError;
            const auto allItems = service.items(&operationError);
            if (!operationError.isEmpty()) { error << "error=" << operationError << "\n"; return 2; }
            for (const auto& item : allItems) {
                if (options.contains(QStringLiteral("--stage")) && item.value(QStringLiteral("stage")).toString() != options.value(QStringLiteral("--stage"))) continue;
                if (options.contains(QStringLiteral("--status")) && item.value(QStringLiteral("status")).toString() != options.value(QStringLiteral("--status"))) continue;
                if (options.contains(QStringLiteral("--project"))) {
                    bool found = false;
                    for (const auto& project : item.value(QStringLiteral("originProjects")).toArray())
                        if (project.toObject().value(QStringLiteral("projectId")).toString() == options.value(QStringLiteral("--project"))
                            || project.toObject().value(QStringLiteral("projectName")).toString() == options.value(QStringLiteral("--project"))) found = true;
                    if (!found) continue;
                }
                printImprovementItem(output, item);
            }
            return 0;
        }
        if (operation == QStringLiteral("triage")) {
            const auto options = parseImprovementOptions(arguments, 2, error);
            if (!options.contains(QStringLiteral("--id")) || !options.contains(QStringLiteral("--action"))) { error << "error=id-and-action-are-required\n"; return 2; }
            QString operationError;
            if (!service.triage(options.value(QStringLiteral("--id")), options.value(QStringLiteral("--action")), options.value(QStringLiteral("--duplicate-of")), options.value(QStringLiteral("--priority")), &operationError)) { error << "error=" << operationError << "\n"; return 2; }
            output << "triaged improvement=" << options.value(QStringLiteral("--id")) << " action=" << options.value(QStringLiteral("--action")) << "\n";
            return 0;
        }
        error << "error=unknown-improvement-operation:" << operation << "\n";
        return 2;
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
    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory") && arguments.at(1) == QStringLiteral("compact")) {
        QString projectRoot; bool dryRun = false; bool approve = false;
        for (int i = 2; i < arguments.size(); ++i) {
            if (arguments.at(i) == QStringLiteral("--dry-run")) { dryRun = true; continue; }
            if (arguments.at(i) == QStringLiteral("--approve")) { approve = true; continue; }
            if (arguments.at(i) == QStringLiteral("--project") && i + 1 < arguments.size()) { projectRoot = QDir::cleanPath(QFileInfo(arguments.at(++i)).absoluteFilePath()); continue; }
            error << "error=invalid-argument:" << arguments.at(i) << "\n"; return 2;
        }
        if (projectRoot.isEmpty()) { error << "error=project-is-required\n"; return 2; }
        ProjectMemoryCompaction compaction; QString operationError; QJsonObject result;
        if (dryRun) result = compaction.dryRun(projectRoot, &operationError);
        else if (!compaction.compact(projectRoot, approve, &result, &operationError)) { error << "error=" << operationError << "\n"; return 2; }
        if (!operationError.isEmpty()) { error << "error=" << operationError << "\n"; return 2; }
        output << QJsonDocument(result).toJson(QJsonDocument::Indented) << "\n"; return 0;
    }

    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory")
        && (arguments.at(1) == QStringLiteral("events") || arguments.at(1) == QStringLiteral("event"))) {
        const auto options = parseReadOptions(arguments, 2, error);
        if (!options.contains(QStringLiteral("--project"))) {
            error << "error=project-is-required\n";
            return 2;
        }
        const QString format = options.value(QStringLiteral("--format"), QStringLiteral("text")).toLower();
        if (format != QStringLiteral("text") && format != QStringLiteral("json")) {
            error << "error=unsupported-format:" << format << "\n";
            return 2;
        }
        const bool json = format == QStringLiteral("json");
        const QString projectRoot = QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath());
        ProjectMemory memory;
        QString readError;
        if (arguments.at(1) == QStringLiteral("events")) {
            QList<QJsonObject> values;
            if (options.contains(QStringLiteral("--task"))) {
                values = memory.eventsForTask(projectRoot, options.value(QStringLiteral("--task")), options.value(QStringLiteral("--event-type")), &readError);
            } else {
                values = memory.events(projectRoot, &readError);
                if (readError.isEmpty() && options.contains(QStringLiteral("--event-type"))) {
                    QList<QJsonObject> filtered;
                    for (const auto& value : values)
                        if (value.value(QStringLiteral("eventType")).toString() == options.value(QStringLiteral("--event-type"))) filtered.append(value);
                    values = filtered;
                }
            }
            if (!readError.isEmpty()) { error << "error=" << readError << "\n"; return 2; }
            printObjects(output, values, json);
            return 0;
        }
        if (!options.contains(QStringLiteral("--id"))) {
            error << "error=id-is-required\n";
            return 2;
        }
        QJsonObject value;
        if (!memory.eventById(projectRoot, options.value(QStringLiteral("--id")), &value, &readError)) {
            error << "error=" << readError << "\n";
            return 2;
        }
        printObject(output, value, json);
        return 0;
    }

    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory")
        && (arguments.at(1) == QStringLiteral("decisions") || arguments.at(1) == QStringLiteral("decision"))) {
        const bool singular = arguments.at(1) == QStringLiteral("decision");
        if (singular && arguments.size() >= 3
            && (arguments.at(2) == QStringLiteral("record") || arguments.at(2) == QStringLiteral("supersede"))) {
            // The write subcommands are handled below.
        } else {
            const auto options = parseReadOptions(arguments, 2, error);
            if (!options.contains(QStringLiteral("--project"))) {
                error << "error=project-is-required\n";
                return 2;
            }
            const QString format = options.value(QStringLiteral("--format"), QStringLiteral("text")).toLower();
            if (format != QStringLiteral("text") && format != QStringLiteral("json")) {
                error << "error=unsupported-format:" << format << "\n";
                return 2;
            }
            const bool json = format == QStringLiteral("json");
            const QString projectRoot = QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath());
            ProjectMemory memory;
            QString readError;
            if (singular) {
                if (!options.contains(QStringLiteral("--id"))) {
                    error << "error=id-is-required\n";
                    return 2;
                }
                QJsonObject value;
                if (!memory.decisionById(projectRoot, options.value(QStringLiteral("--id")), &value, &readError)) {
                    error << "error=" << readError << "\n";
                    return 2;
                }
                printObject(output, value, json);
            } else {
                QList<QJsonObject> values;
                if (options.contains(QStringLiteral("--topic")))
                    values = memory.decisionsByTopic(projectRoot, options.value(QStringLiteral("--topic")), !options.contains(QStringLiteral("--current")), &readError);
                else if (options.contains(QStringLiteral("--current")))
                    values = memory.currentDecisions(projectRoot, &readError);
                else values = memory.decisions(projectRoot, true, &readError);
                if (!readError.isEmpty()) { error << "error=" << readError << "\n"; return 2; }
                printObjects(output, values, json);
            }
            return 0;
        }
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

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("knowledge") && arguments.at(2) == QStringLiteral("propose")) {
        const QSet<QString> valueOptions{QStringLiteral("--project"), QStringLiteral("--title"),
                                         QStringLiteral("--lesson"), QStringLiteral("--scopes"),
                                         QStringLiteral("--evidence"), QStringLiteral("--portable")};
        QHash<QString, QString> options;
        for (int index = 3; index < arguments.size(); ++index) {
            const QString name = arguments.at(index);
            if (!valueOptions.contains(name) || index + 1 >= arguments.size()) {
                error << "error=invalid-argument:" << name << "\n";
                return 2;
            }
            options.insert(name, arguments.at(++index));
        }
        for (const QString& required : {QStringLiteral("--project"), QStringLiteral("--title"),
                                        QStringLiteral("--lesson"), QStringLiteral("--scopes"),
                                        QStringLiteral("--evidence")}) {
            if (!options.contains(required)) {
                error << "error=missing-argument:" << required << "\n";
                return 2;
            }
        }
        FrameworkKnowledgeService service;
        QString knowledgeError;
        const QString id = service.propose(
            QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
            options.value(QStringLiteral("--title")), options.value(QStringLiteral("--lesson")),
            options.value(QStringLiteral("--scopes")).split(',', Qt::SkipEmptyParts),
            QStringList{options.value(QStringLiteral("--evidence"))},
            options.value(QStringLiteral("--portable"), QStringLiteral("true")).compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0,
            &knowledgeError);
        if (id.isEmpty()) {
            error << "error=" << knowledgeError << "\n";
            return 2;
        }
        output << "proposed knowledge=" << id << "\n";
        return 0;
    }

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("override") && arguments.at(2) == QStringLiteral("knowledge")) {
        const QSet<QString> valueOptions{QStringLiteral("--project"), QStringLiteral("--instruction"),
                                         QStringLiteral("--title"), QStringLiteral("--lesson"),
                                         QStringLiteral("--scopes"), QStringLiteral("--evidence"),
                                         QStringLiteral("--scope"),
                                         QStringLiteral("--portable")};
        QHash<QString, QString> options;
        for (int index = 3; index < arguments.size(); ++index) {
            const QString name = arguments.at(index);
            if (!valueOptions.contains(name) || index + 1 >= arguments.size()) {
                error << "error=invalid-argument:" << name << "\n";
                return 2;
            }
            options.insert(name, arguments.at(++index));
        }
        for (const QString& required : {QStringLiteral("--project"), QStringLiteral("--instruction"),
                                        QStringLiteral("--title"), QStringLiteral("--lesson"),
                                        QStringLiteral("--scopes"), QStringLiteral("--evidence"),
                                        QStringLiteral("--scope")}) {
            if (!options.contains(required)) {
                error << "error=missing-argument:" << required << "\n";
                return 2;
            }
        }
        ProjectMemory memory;
        const QString projectRoot = QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath());
        if (!memory.isVerifiedAdministrativeOverride(options.value(QStringLiteral("--instruction")))) {
            error << "error=administrative-identity-and-override-intent-required\n";
            return 2;
        }
        QJsonObject audit;
        QString operationError;
        const QString requestedScope = options.value(QStringLiteral("--scope")).trimmed().toLower();
        if (requestedScope != QStringLiteral("project")) {
            error << "error=admin-override-knowledge-is-project-local-only\n";
            return 2;
        }
        FrameworkKnowledgeService service;
        const bool portable = options.value(QStringLiteral("--portable"), QStringLiteral("true"))
            .compare(QStringLiteral("false"), Qt::CaseInsensitive) != 0;
        const QString id = service.proposeApprovedByAdministrator(
            projectRoot, options.value(QStringLiteral("--title")), options.value(QStringLiteral("--lesson")),
            options.value(QStringLiteral("--scopes")).split(',', Qt::SkipEmptyParts),
            QStringList{options.value(QStringLiteral("--evidence"))}, QStringLiteral("Admin Morgan Lindbom"), portable, &operationError);
        if (id.isEmpty()) {
            error << "error=" << operationError << "\n";
            return 2;
        }
        if (!memory.recordAdministrativeOverride(
                projectRoot, options.value(QStringLiteral("--instruction")),
                QStringLiteral("Framework Knowledge requires explicit approval before activation."),
                QStringLiteral("Verified administrator explicitly authorized adding and approving this reusable knowledge."),
                QStringLiteral("Create and approve one project Framework Knowledge entry: %1").arg(options.value(QStringLiteral("--title"))),
                QStringLiteral("Add and approve Framework Knowledge"), {}, {}, false,
                QJsonObject{{QStringLiteral("knowledgeId"), id},
                            {QStringLiteral("resultingStatus"), QStringLiteral("approved")},
                            {QStringLiteral("scope"), QStringLiteral("project")},
                            {QStringLiteral("projectId"), QFileInfo(projectRoot).fileName()},
                            {QStringLiteral("projectName"), QFileInfo(projectRoot).fileName()},
                            {QStringLiteral("projectPath"), QDir::cleanPath(projectRoot)}},
                &audit, &operationError)) {
            error << "error=" << operationError << "\n";
            return 2;
        }
        output << "accepted ADMIN_OVERRIDE knowledge=" << id << " status=approved scope=project"
               << " administrator=Admin-Morgan-Lindbom validation=PASS\n";
        return 0;
    }

    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("override")) {
        const QSet<QString> valueOptions{QStringLiteral("--project"), QStringLiteral("--instruction"),
                                         QStringLiteral("--rule"), QStringLiteral("--reason"),
                                         QStringLiteral("--scope"), QStringLiteral("--action"),
                                         QStringLiteral("--affected-files"), QStringLiteral("--affected-systems"),
                                         QStringLiteral("--persistent-policy")};
        QHash<QString, QString> options;
        for (int index = 2; index < arguments.size(); ++index) {
            const QString name = arguments.at(index);
            if (!valueOptions.contains(name) || index + 1 >= arguments.size()) {
                error << "error=invalid-argument:" << name << "\n";
                return 2;
            }
            options.insert(name, arguments.at(++index));
        }
        for (const QString& required : {QStringLiteral("--project"), QStringLiteral("--instruction"),
                                        QStringLiteral("--rule"), QStringLiteral("--reason"),
                                        QStringLiteral("--scope"), QStringLiteral("--action")}) {
            if (!options.contains(required)) {
                error << "error=missing-argument:" << required << "\n";
                return 2;
            }
        }
        ProjectMemory memory;
        QJsonObject result;
        QString overrideError;
        const bool persistent = options.value(QStringLiteral("--persistent-policy"), QStringLiteral("false"))
            .compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        if (!memory.recordAdministrativeOverride(
                QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
                options.value(QStringLiteral("--instruction")), options.value(QStringLiteral("--rule")),
                options.value(QStringLiteral("--reason")), options.value(QStringLiteral("--scope")),
                options.value(QStringLiteral("--action")),
                options.value(QStringLiteral("--affected-files")).split(',', Qt::SkipEmptyParts),
                options.value(QStringLiteral("--affected-systems")).split(',', Qt::SkipEmptyParts),
                persistent, {}, &result, &overrideError)) {
            error << "error=" << overrideError << "\n";
            return 2;
        }
        output << "accepted ADMIN_OVERRIDE administrator=Morgan Lindbom scope=" << options.value(QStringLiteral("--scope")) << " validation=PASS\n";
        return 0;
    }

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("knowledge") && arguments.at(2) == QStringLiteral("approve")) {
        const QSet<QString> valueOptions{QStringLiteral("--project"), QStringLiteral("--id"), QStringLiteral("--source")};
        QHash<QString, QString> options;
        for (int index = 3; index < arguments.size(); ++index) {
            const QString name = arguments.at(index);
            if (!valueOptions.contains(name) || index + 1 >= arguments.size()) {
                error << "error=invalid-argument:" << name << "\n";
                return 2;
            }
            options.insert(name, arguments.at(++index));
        }
        for (const QString& required : {QStringLiteral("--project"), QStringLiteral("--id"), QStringLiteral("--source")}) {
            if (!options.contains(required)) {
                error << "error=missing-argument:" << required << "\n";
                return 2;
            }
        }
        FrameworkKnowledgeService service;
        QString knowledgeError;
        if (!service.approve(QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
                             options.value(QStringLiteral("--id")), options.value(QStringLiteral("--source")), &knowledgeError)) {
            error << "error=" << knowledgeError << "\n";
            return 2;
        }
        output << "approved knowledge=" << options.value(QStringLiteral("--id")) << "\n";
        return 0;
    }

    if (arguments.size() >= 2 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("checkpoints")) {
        const auto options = parseReadOptions(arguments, 2, error);
        if (!options.contains(QStringLiteral("--project"))) {
            error << "error=project-is-required\n";
            return 2;
        }
        const QString format = options.value(QStringLiteral("--format"), QStringLiteral("text")).toLower();
        if (format != QStringLiteral("text") && format != QStringLiteral("json")) {
            error << "error=unsupported-format:" << format << "\n";
            return 2;
        }
        ProjectMemory memory;
        QString readError;
        const auto values = memory.checkpoints(
            QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()), &readError);
        if (!readError.isEmpty()) {
            error << "error=" << readError << "\n";
            return 2;
        }
        printObjects(output, values, format == QStringLiteral("json"));
        return 0;
    }

    if (arguments.size() >= 3 && arguments.at(0) == QStringLiteral("memory")
        && arguments.at(1) == QStringLiteral("checkpoint")
        && arguments.at(2) == QStringLiteral("get")) {
        const auto options = parseReadOptions(arguments, 3, error);
        if (!options.contains(QStringLiteral("--project")) || !options.contains(QStringLiteral("--id"))) {
            error << "error=project-and-id-are-required\n";
            return 2;
        }
        const QString format = options.value(QStringLiteral("--format"), QStringLiteral("text")).toLower();
        if (format != QStringLiteral("text") && format != QStringLiteral("json")) {
            error << "error=unsupported-format:" << format << "\n";
            return 2;
        }
        ProjectMemory memory;
        QJsonObject value;
        QString readError;
        if (!memory.checkpointById(
                QDir::cleanPath(QFileInfo(options.value(QStringLiteral("--project"))).absoluteFilePath()),
                options.value(QStringLiteral("--id")), &value, &readError)) {
            error << "error=" << readError << "\n";
            return 2;
        }
        printObject(output, value, format == QStringLiteral("json"));
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
