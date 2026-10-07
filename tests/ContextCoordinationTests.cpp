// ContextCoordinationTests.cpp
#include "core/ContextCoordinationService.h"
#include "core/WorkerContextResolver.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "core/WorkerTaskServices.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

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
    // Exercise canonical context and typed-resource boundaries in isolated projects.

    // New fixtures are retained, and successful hashes are never substituted for
    // missing, unreadable, linked or incorrectly classified resource contents.
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

    QTemporaryDir folderFixture;
    folderFixture.setAutoRemove(false);
    ProjectModel folderModel;
    configureModel(folderModel, folderFixture.path(), "typed-folder-context");
    QDir().mkpath(folderFixture.path() + "/bundle/empty");
    writeFile(folderFixture.path() + "/bundle/a.cpp", "// a\n");
    writeFile(folderFixture.path() + "/bundle/.hidden", "hidden\n");
    ProjectResource folder;
    folder.id = "bundle"; folder.type = "folder"; folder.location = "bundle"; folder.scopes = {"ui"};
    folderModel.setResources({folder});
    const auto firstFolder = WorkerContextResolver::resourceSnapshot(folderModel, "bundle");
    check(firstFolder.value("valid").toBool(), "folder manifest readable");
    check(firstFolder == WorkerContextResolver::resourceSnapshot(folderModel, "bundle"), "folder manifests deterministic");
    check(firstFolder.value("entries").toObject().contains("empty"), "empty directories represented");
    check(firstFolder.value("entries").toObject().contains(".hidden"), "hidden files included");
#ifdef Q_OS_WIN
    const QString lockedPath = folderFixture.path() + "/bundle/a.cpp";
    HANDLE lockedResource = CreateFileW(reinterpret_cast<LPCWSTR>(lockedPath.utf16()), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(lockedResource != INVALID_HANDLE_VALUE, "exclusive unreadable-resource fixture opened");
    if (lockedResource != INVALID_HANDLE_VALUE) {
        check(!WorkerContextResolver::resourceSnapshot(folderModel, "bundle").value("valid").toBool(), "locked child cannot be hashed as empty content");
        CloseHandle(lockedResource);
    }
#endif
    check(GenerationServices().generate(folderModel, folderModel.generationOptions()).success, "folder context generation");
    check(ContextCoordinationService::validate(folderModel).value("valid").toBool(), "folder context current");
    writeFile(folderFixture.path() + "/bundle/a.cpp", "// changed\n");
    check(!ContextCoordinationService::validate(folderModel).value("valid").toBool(), "nested content invalidates context");
    check(ContextCoordinationService::generate(folderModel).value("success").toBool(), "canonical folder reindex");
    check(ContextCoordinationService::validate(folderModel).value("valid").toBool(), "reindexed folder is current");
    QDir().mkpath(folderFixture.path() + "/bundle/new/deep");
    check(!ContextCoordinationService::validate(folderModel).value("valid").toBool(), "nested addition invalidates context");
    check(QFile::rename(folderFixture.path() + "/bundle/a.cpp", folderFixture.path() + "/bundle/renamed.cpp"), "rename preserved fixture file");
    const auto renamedFolder = WorkerContextResolver::resourceSnapshot(folderModel, "bundle");
    check(!renamedFolder.value("entries").toObject().contains("a.cpp") && renamedFolder.value("entries").toObject().contains("renamed.cpp"), "rename changes manifest keys");
    check(QFile::rename(folderFixture.path() + "/bundle/renamed.cpp", folderFixture.path() + "/preserved.cpp"), "move fixture file without deletion");
    check(!WorkerContextResolver::resourceSnapshot(folderModel, "bundle").value("entries").toObject().contains("renamed.cpp"), "removal from resource detected");
    folder.type = "file"; folderModel.setResources({folder});
    check(!WorkerContextResolver::resourceSnapshot(folderModel, "bundle").value("valid").toBool(), "directory cannot become a file hash");
    folder.type = "folder"; folderModel.setResources({folder});
    QJsonObject firstOperation;
    {
        WorkerResourceObservation operation;
        firstOperation = WorkerContextResolver::resourceSnapshot(folderModel, "bundle");
        check(firstOperation == WorkerContextResolver::resourceSnapshot(folderModel, "bundle"), "one operation reuses a coherent resource observation");
    }
    writeFile(folderFixture.path() + "/bundle/next-operation.txt", "changed between operations\n");
    {
        WorkerResourceObservation operation;
        check(firstOperation.value("fingerprint") != WorkerContextResolver::resourceSnapshot(folderModel, "bundle").value("fingerprint"), "next operation never reuses stale observations");
    }
    folder.type = "folder"; folder.location = "absent"; folderModel.setResources({folder});
    check(!WorkerContextResolver::resourceSnapshot(folderModel, "bundle").value("valid").toBool(), "missing folder invalid");
    folder.location = "bundle"; folderModel.setResources({folder});
    auto policyRules = folderModel.ruleConfiguration();
    auto policyMetadata = policyRules.scopeMetadata.value("ui").toObject();
    policyMetadata.insert("folderResourcePolicies", QJsonObject{{"bundle", QJsonObject{{"schemaVersion", 1}, {"generatedPaths", QJsonArray{".hidden"}}}}});
    policyRules.scopeMetadata.insert("ui", policyMetadata); folderModel.setRuleConfiguration(policyRules);
    check(!WorkerContextResolver::resourceSnapshot(folderModel, "bundle").value("valid").toBool(), "arbitrary generated exclusion rejected");

    QTemporaryDir selfFixture; selfFixture.setAutoRemove(false);
    ProjectModel selfModel; configureModel(selfModel, selfFixture.path(), "self-referencing-worker");
    check(GenerationServices().generate(selfModel, selfModel.generationOptions()).success, "self-reference fixture generated");
    ProjectResource selfResource; selfResource.id = "worker"; selfResource.type = "folder";
    selfResource.location = "ARAMF_WORKER"; selfResource.scopes = {"ui"}; selfModel.setResources({selfResource});
    auto selfRules = selfModel.ruleConfiguration(); auto selfMetadata = selfRules.scopeMetadata.value("ui").toObject();
    selfMetadata.insert("folderResourcePolicies", QJsonObject{{"worker", QJsonObject{{"schemaVersion", 1},
        {"generatedPaths", QJsonArray{"context/context-index.json", "context/compressed-context.json", "context/freshness.json",
            "context/agent-adapters.json", "verification/latest-validation.json", "verification/verification-result.json",
            "memory/cold-start-validation.json", "memory/memory-consistency-validation.json"}}}}});
    selfRules.scopeMetadata.insert("ui", selfMetadata); selfModel.setRuleConfiguration(selfRules);
    check(GenerationServices().generate(selfModel, selfModel.generationOptions()).success, "self-reference policy generated canonically");
    check(!ContextCoordinationService::validate(selfModel).value("valid").toBool(), "undeclared generation-state change is not silently excluded");
    auto selfPolicies = selfMetadata.value("folderResourcePolicies").toObject();
    auto selfPolicy = selfPolicies.value("worker").toObject();
    auto selfGenerated = selfPolicy.value("generatedPaths").toArray();
    selfGenerated.append("verification/generation-state.json");
    selfPolicy.insert("generatedPaths", selfGenerated); selfPolicies.insert("worker", selfPolicy);
    selfMetadata.insert("folderResourcePolicies", selfPolicies); selfRules.scopeMetadata.insert("ui", selfMetadata);
    selfModel.setRuleConfiguration(selfRules);
    check(GenerationServices().regenerateConfiguration(selfModel, selfModel.generationOptions()).success, "self-host configuration regenerated through production writer");
    check(ContextCoordinationService::validate(selfModel).value("valid").toBool(), "generation-state producer marker prevents late-write self-staleness");
    bool matchingReadback = true;
    const auto expectedOutputs = GenerationServices::derivedTaskArtifacts(selfModel, selfModel.generationOptions());
    for (auto it = expectedOutputs.begin(); it != expectedOutputs.end(); ++it) {
        auto actual = QJsonDocument::fromJson(readFile(selfFixture.path() + '/' + it.key())).object();
        actual.remove("_file");
        auto expected = it.value().toObject(); expected.remove("_file");
        matchingReadback &= actual == expected;
    }
    check(matchingReadback, "configuration update readback agrees without a second context generation");
    check(VerificationServices().verify(selfModel, selfModel.generationOptions()).overallStatus == VerificationStatus::Pass, "self-host configuration update verification PASS");
    // Index the first verification products after their canonical creation.

    // Generated paths remain explicit manifest entries: creating a previously
    // absent producer output legitimately changes the folder's entry set.
    ContextCoordinationService::generate(selfModel);
    const QString statePath = selfFixture.path() + "/ARAMF_WORKER/verification/generation-state.json";
    const auto intactState = readFile(statePath);
    writeFile(statePath, "{}\n");
    check(VerificationServices().verify(selfModel, selfModel.generationOptions(), false).overallStatus != VerificationStatus::Pass, "generation-state policy cannot hide corrupt producer evidence");
    writeFile(statePath, intactState);
    const auto restoredVerification = VerificationServices().verify(selfModel, selfModel.generationOptions());
    check(restoredVerification.overallStatus == VerificationStatus::Pass, "restored producer evidence revalidates");
    ContextCoordinationService::generate(selfModel);
    const auto selfSnapshot = WorkerContextResolver::resourceSnapshot(selfModel, "worker");
    ContextCoordinationService::generate(selfModel);
    check(selfSnapshot == WorkerContextResolver::resourceSnapshot(selfModel, "worker"), "explicit producer markers prevent recursive self-staleness");
    check(ContextCoordinationService::validate(selfModel).value("valid").toBool(), "self-referencing context remains fresh");
    writeFile(selfFixture.path() + "/ARAMF_WORKER/context/context-index.json", "{}\n");
    check(!ContextCoordinationService::validate(selfModel).value("valid").toBool(), "generated exclusions do not authorize corrupt indexes");
    ContextCoordinationService::generate(selfModel);
    writeFile(selfFixture.path() + "/ARAMF_WORKER/custom/unmapped.txt", "unmapped\n");
    check(selfSnapshot.value("fingerprint") != WorkerContextResolver::resourceSnapshot(selfModel, "worker").value("fingerprint"), "user-owned children are never excluded");
    const QString linkPath = selfFixture.path() + "/ARAMF_WORKER/unsafe.lnk";
    if (QFile::link(selfFixture.path() + "/source/ui.cpp", linkPath) && QFileInfo(linkPath).isSymLink())
        check(!WorkerContextResolver::resourceSnapshot(selfModel, "worker").value("valid").toBool(), "unsafe linked child rejected");
    else std::cout << "NOT_RUN: platform cannot create the unsafe-link fixture\n";
    std::cout << "P1-CONTEXT checks=" << checks << " failures=" << failures << '\n';
    return failures == 0;
}
