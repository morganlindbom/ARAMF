#include "core/ContextCoordinationService.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "core/WorkerTaskServices.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool writeFile(const QString& path, const QByteArray& value)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(value) == value.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

void configureModel(ProjectModel& model, const QString& path, const QString& id)
{
    model.setProjectPath(path);
    model.setProjectId(id);
    auto ai = model.aiConfiguration();
    ai.permissions = {QStringLiteral("read-project-files"), QStringLiteral("modify-files"), QStringLiteral("create-files")};
    model.setAiConfiguration(ai);
    RuleConfiguration rules;
    rules.projectScopes = {QStringLiteral("ui"), QStringLiteral("pico"), QStringLiteral("thesis"), QStringLiteral("report")};
    for (const auto& scope : rules.projectScopes) {
        const QString file = QStringLiteral("source/%1.cpp").arg(scope);
        writeFile(QDir(path).filePath(file), "// canonical\n");
        rules.scopeMetadata.insert(scope, QJsonObject{{QStringLiteral("files"), QJsonArray{file}},
            {QStringLiteral("tests"), QJsonArray{scope + QStringLiteral("-tests")}},
            {QStringLiteral("affects"), QJsonArray{}}, {QStringLiteral("riskTraits"), QJsonArray{}},
            {QStringLiteral("generatedArtifacts"), QJsonArray{}}});
    }
    model.setRuleConfiguration(rules);
    auto academic = model.academicConfiguration();
    academic.enabled = true;
    academic.projectTypes = {QStringLiteral("thesis"), QStringLiteral("report")};
    academic.thesisDocumentation.enabled = true;
    academic.reportDocumentation.enabled = true;
    model.setAcademicConfiguration(academic);
}
}

bool runContextCoordinationTests()
{
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool condition, const QString& name) {
        ++checks;
        if (!condition) { ++failures; std::cerr << "P1 FAIL: " << name.toStdString() << '\n'; }
    };

    QTemporaryDir project;
    check(project.isValid(), QStringLiteral("temporary P1 project is valid"));
    ProjectModel model;
    configureModel(model, project.path(), QStringLiteral("p1-project-a"));
    const auto generated = GenerationServices().generate(model, model.generationOptions());
    check(generated.success, QStringLiteral("P1 Worker generation succeeds: %1").arg(generated.error));

    const auto index = ContextCoordinationService::buildIndex(model);
    check(index.value(QStringLiteral("schemaVersion")).toInt() == 1, QStringLiteral("context index schema v1"));
    check(index.value(QStringLiteral("authority")).toString() == QStringLiteral("DERIVED"), QStringLiteral("context index is derived"));
    check(index.value(QStringLiteral("projectId")).toString() == model.projectId(), QStringLiteral("context index is project-bound"));
    check(!index.value(QStringLiteral("entries")).toArray().isEmpty(), QStringLiteral("context index has governed entries"));

    const auto thesis = ContextCoordinationService::route(model, {QStringLiteral("thesis")}, QStringLiteral("instructions"));
    const auto pico = ContextCoordinationService::route(model, {QStringLiteral("pico")});
    check(thesis.value(QStringLiteral("valid")).toBool(), QStringLiteral("thesis section route is valid"));
    check(pico.value(QStringLiteral("valid")).toBool(), QStringLiteral("pico route is valid"));
    check(thesis.value(QStringLiteral("entries")).toArray().size() == 1, QStringLiteral("thesis route is scoped"));
    check(pico.value(QStringLiteral("entries")).toArray().size() < index.value(QStringLiteral("entries")).toArray().size(), QStringLiteral("pico route does not load all context"));
    check(!ContextCoordinationService::route(model, {QStringLiteral("not-a-scope")}).value(QStringLiteral("valid")).toBool(), QStringLiteral("unknown scope fails closed"));
    check(ContextCoordinationService::retrieveDecisions(model, {QStringLiteral("thesis")}).value(QStringLiteral("historyPolicy")).toString() == QStringLiteral("event-log excluded"), QStringLiteral("decision retrieval excludes history by default"));

    const auto compressed = ContextCoordinationService::compress(model);
    check(compressed.value(QStringLiteral("authority")).toString() == QStringLiteral("DERIVED"), QStringLiteral("compressed context is derived"));
    check(!compressed.value(QStringLiteral("entries")).toArray().first().toObject().value(QStringLiteral("provenance")).toArray().isEmpty(), QStringLiteral("compression preserves provenance"));
    const QByteArray indexBeforeRepeat = readFile(QDir(project.path()).filePath(QStringLiteral("ARAMF_WORKER/context/context-index.json")));
    const auto generatedAgain = ContextCoordinationService::generate(model);
    check(generatedAgain.value(QStringLiteral("success")).toBool(), QStringLiteral("repeat P1 generation succeeds"));
    check(indexBeforeRepeat == readFile(QDir(project.path()).filePath(QStringLiteral("ARAMF_WORKER/context/context-index.json"))), QStringLiteral("context output is byte stable"));
    check(ContextCoordinationService::validate(model).value(QStringLiteral("valid")).toBool(), QStringLiteral("fresh context validates"));

    const QString statusPath = QDir(project.path()).filePath(QStringLiteral("ARAMF_WORKER/PROJECT_STATUS.md"));
    writeFile(statusPath, readFile(statusPath) + "\nP1 source change\n");
    check(ContextCoordinationService::freshness(model).value(QStringLiteral("staleCount")).toInt() > 0, QStringLiteral("relevant source change becomes stale"));
    check(!ContextCoordinationService::validate(model).value(QStringLiteral("valid")).toBool(), QStringLiteral("stale context is not trusted"));
    check(ContextCoordinationService::generate(model).value(QStringLiteral("success")).toBool(), QStringLiteral("stale derived context regenerates"));
    check(ContextCoordinationService::validate(model).value(QStringLiteral("valid")).toBool(), QStringLiteral("regenerated context is current"));

    const auto dag = ContextCoordinationService::saveTaskDag(model, QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("a")}, {QStringLiteral("state"), QStringLiteral("COMPLETED")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("b")}, {QStringLiteral("dependencies"), QJsonArray{"a"}}, {QStringLiteral("state"), QStringLiteral("PENDING")}}}, QStringLiteral("contract-1"));
    check(dag.value(QStringLiteral("valid")).toBool(), QStringLiteral("linear DAG persists"));
    check(dag.value(QStringLiteral("readyNodes")).toArray() == QJsonArray{"b"}, QStringLiteral("DAG dependency gating is deterministic"));
    check(ContextCoordinationService::saveTaskDag(model, QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("a")}, {QStringLiteral("dependencies"), QJsonArray{"b"}}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("b")}, {QStringLiteral("dependencies"), QJsonArray{"a"}}}}, QStringLiteral("contract-1")).value(QStringLiteral("errorCode")).toString() == QStringLiteral("TASK_DAG_CYCLE"), QStringLiteral("DAG cycles are rejected"));

    WorkerTaskRequest request;
    request.goal = QStringLiteral("Change UI");
    request.type = QStringLiteral("coding");
    request.scopes = {QStringLiteral("ui")};
    request.files = {QStringLiteral("source/ui.cpp")};
    request.definitionOfDone = {QStringLiteral("focused test passes")};
    const auto contract = WorkerTaskServices::prepare(model, request);
    const auto handoff = ContextCoordinationService::createHandoff(model, contract, QStringLiteral("codex"), QStringLiteral("gemini"), {QStringLiteral("ui")}, false);
    check(handoff.value(QStringLiteral("valid")).toBool(), QStringLiteral("valid handoff preserves contract"));
    check(ContextCoordinationService::createHandoff(model, contract, QStringLiteral("codex"), QStringLiteral("gemini"), {QStringLiteral("pico")}, false).value(QStringLiteral("errorCode")).toString() == QStringLiteral("HANDOFF_SCOPE_ESCALATION"), QStringLiteral("handoff cannot expand scope"));
    const auto codex = ContextCoordinationService::adaptContract(contract, QStringLiteral("openai-codex"));
    const auto gemini = ContextCoordinationService::adaptContract(contract, QStringLiteral("gemini"));
    check(codex.value(QStringLiteral("governance")) == gemini.value(QStringLiteral("governance")), QStringLiteral("adapters preserve equivalent governance"));
    check(!ContextCoordinationService::adapterDescriptors().value(QStringLiteral("adapters")).toArray().isEmpty(), QStringLiteral("agent adapters are discoverable"));

    QTemporaryDir secondProject;
    ProjectModel second;
    configureModel(second, secondProject.path(), QStringLiteral("p1-project-b"));
    check(GenerationServices().generate(second, second.generationOptions()).success, QStringLiteral("second P1 project generates"));
    check(ContextCoordinationService::buildIndex(second).value(QStringLiteral("projectId")).toString() != index.value(QStringLiteral("projectId")).toString(), QStringLiteral("P1 index isolates projects"));
    ProjectPersistence persistence;
    const QString saved = QDir(project.path()).filePath(QStringLiteral("project.aramf.json"));
    QString error;
    check(persistence.save(model, saved, &error), QStringLiteral("P1 project persistence saves"));
    ProjectModel reloaded;
    check(persistence.load(&reloaded, saved, &error) && reloaded.projectId() == model.projectId(), QStringLiteral("P1 project reload preserves identity"));
    check(ContextCoordinationService::validate(reloaded).value(QStringLiteral("valid")).toBool(), QStringLiteral("reloaded P0 project can use P1 derived state"));

    std::cout << "P1-CONTEXT checks=" << checks << " failures=" << failures << '\n';
    return failures == 0;
}
