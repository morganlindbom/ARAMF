// WorkerTaskTests.cpp
#include "core/WorkerTaskServices.h"
#include "core/ContextCoordinationService.h"
#include "core/WorkerContextResolver.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "core/CertificationService.h"
#include "core/ProjectMemory.h"
#include "core/AramfPaths.h"
#include "core/RuntimeOwnershipService.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QMap>
#include <QProcess>
#include <QTemporaryDir>
#include <QDateTime>
#include <atomic>
#include <iostream>
#include <thread>

namespace {
bool write(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray bytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QMap<QString, QByteArray> treeBytes(const QString& root)
{
    QMap<QString, QByteArray> result;
    QDirIterator iterator(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        result.insert(QDir(root).relativeFilePath(path), bytes(path));
    }
    return result;
}
bool hasCode(const QJsonObject& value, const QString& code)
{
    auto errors = value.value("errors").toArray();
    if (value.contains("preflight")) errors = value.value("preflight").toObject().value("errors").toArray();
    for (const auto& item : errors) if (item.toObject().value("code").toString() == code) return true;
    return false;
}
bool ready(const QJsonObject& contract) { return contract.value("preflight").toObject().value("status").toString().startsWith("READY"); }
}

bool runWorkerTaskTests()
{
    // Verify task permission and evidence boundaries using isolated fixtures.

    // Grant PASS records below are synthetic test evidence only; they cannot
    // certify the live repository or authorize its protected CMake file.
    int checks = 0, failed = 0;
    const auto check = [&](bool pass, const QString& name) {
        ++checks;
        if (!pass) { ++failed; std::cerr << "TASK FAIL: " << name.toStdString() << '\n'; }
    };
    const auto generate = [&](ProjectModel& model) {
        const auto result = GenerationServices().generate(model, model.generationOptions());
        check(result.success, "fixture generation: " + result.error);
        const auto verified = VerificationServices().verify(model, model.generationOptions());
        check(verified.overallStatus == VerificationStatus::Pass, "fixture verification");
        const auto context = ContextCoordinationService::generate(model);
        check(context.value(QStringLiteral("success")).toBool(), "fixture context generation");
    };
    QTemporaryDir project;
    ProjectModel model;
    model.setProjectPath(project.path());
    model.setProjectId("task-test-A");
    auto ai = model.aiConfiguration();
    ai.permissions = {"read-project-files", "modify-files", "create-files"};
    model.setAiConfiguration(ai);
    auto memoryOptions = model.memoryConfiguration();
    memoryOptions.maintenanceOptions = {"record-test-results", "update-current-state"};
    model.setMemoryConfiguration(memoryOptions);
    auto certification = model.certificationConfiguration(); certification.enabled = true;
    model.setCertificationConfiguration(certification);
    auto rules = model.ruleConfiguration();
    rules.projectScopes = {"generic", "ui", "android", "communication", "pico", "machine-learning", "thesis", "report", "governance", "persistence"};
    for (const auto& scope : rules.projectScopes) {
        const QString file = "source/" + scope + ".cpp";
        check(write(QDir(project.path()).filePath(file), "before\n"), "write canonical source");
        rules.scopeMetadata.insert(scope, QJsonObject{{"files", QJsonArray{file}}, {"tests", QJsonArray{scope + "-tests"}},
            {"riskTraits", QJsonArray{}}, {"affects", QJsonArray{}}, {"generatedArtifacts", QJsonArray{}}});
    }
    const auto annotate = [&](const QString& scope, const QJsonArray& traits, const QJsonArray& affects) {
        auto metadata = rules.scopeMetadata.value(scope).toObject();
        metadata.insert("riskTraits", traits); metadata.insert("affects", affects); rules.scopeMetadata.insert(scope, metadata);
    };
    annotate("ui", {"ui"}, {});
    annotate("pico", {"hardware"}, {});
    annotate("governance", {"schema", "governance"}, {});
    annotate("persistence", {"persistence", "migration"}, {});
    annotate("communication", {"runtime"}, {"android", "pico"});
    model.setRuleConfiguration(rules);
    rules = model.ruleConfiguration();
    generate(model);
    const auto requestFor = [](const QStringList& scopes) {
        WorkerTaskRequest request;
        request.goal = "Implement the requested behavior";
        request.type = "coding";
        request.scopes = scopes;
        for (const auto& scope : scopes) request.files << "source/" + scope + ".cpp";
        request.definitionOfDone = {"Requested behavior is covered by the mapped focused tests."};
        return request;
    };
    const QList<QStringList> matrix{{"generic"}, {"ui", "android"}, {"communication"}, {"pico"}, {"machine-learning"},
        {"thesis"}, {"report"}, {"thesis", "report"}, {"android", "pico"}, {"governance"}, {"persistence"}};

    // PREPARE is a strictly read-only gate. Protect the complete fixture and
    // explicitly name every context artifact whose canonical producer is P1.
    const QStringList protectedContextArtifacts{
        QStringLiteral("ARAMF_WORKER/context/context-index.json"),
        QStringLiteral("ARAMF_WORKER/context/compressed-context.json"),
        QStringLiteral("ARAMF_WORKER/context/freshness.json"),
        QStringLiteral("ARAMF_WORKER/context/agent-adapters.json")};
    const auto freshTreeBefore = treeBytes(project.path());
    QMap<QString, QByteArray> freshContextBefore;
    for (const auto& path : protectedContextArtifacts)
        freshContextBefore.insert(path, bytes(QDir(project.path()).filePath(path)));
    const auto freshPrepare = WorkerTaskServices::prepare(model, requestFor({"ui"}));
    check(ready(freshPrepare), "fresh PREPARE is ready");
    check(treeBytes(project.path()) == freshTreeBefore, "fresh PREPARE changes no repository file");
    for (const auto& path : protectedContextArtifacts)
        check(bytes(QDir(project.path()).filePath(path)) == freshContextBefore.value(path),
              "fresh PREPARE preserves " + path);

    const QString staleSourcePath = QDir(project.path()).filePath(
        QStringLiteral("ARAMF_WORKER/memory/current-state.md"));
    const QByteArray currentSource = bytes(staleSourcePath);
    check(write(staleSourcePath, currentSource + "stale context input\n"), "make context stale");
    const auto staleTreeBefore = treeBytes(project.path());
    QMap<QString, QByteArray> staleContextBefore;
    for (const auto& path : protectedContextArtifacts)
        staleContextBefore.insert(path, bytes(QDir(project.path()).filePath(path)));
    const auto stalePrepare = WorkerTaskServices::prepare(model, requestFor({"ui"}));
    check(!ready(stalePrepare) && hasCode(stalePrepare, "STALE_DERIVED_ARTIFACT"),
          "stale PREPARE blocks without regeneration");
    check(treeBytes(project.path()) == staleTreeBefore, "stale PREPARE changes no repository file");
    for (const auto& path : protectedContextArtifacts)
        check(bytes(QDir(project.path()).filePath(path)) == staleContextBefore.value(path),
              "stale PREPARE preserves " + path);
    check(write(staleSourcePath, currentSource), "restore stale input fixture");
    generate(model);

    for (const auto& scopes : matrix) {
        const auto request = requestFor(scopes);
        const auto contract = WorkerTaskServices::prepare(model, request);
        if (!ready(contract)) std::cerr << QJsonDocument(contract.value("preflight").toObject()).toJson().toStdString();
        check(ready(contract), "matrix preflight " + scopes.join(','));
        check(contract == WorkerTaskServices::prepare(model, request), "deterministic contract " + scopes.join(','));
        check(contract.value("permittedFiles").toArray().size() == request.files.size(), "exact allowed set");
        check(!contract.value("protectedFiles").toArray().isEmpty() && !contract.value("negativeConstraints").toArray().isEmpty(), "negative ownership constraints");
        check(!contract.value("requiredEvidence").toArray().isEmpty(), "required evidence mapped");
        const auto impact = contract.value("impact").toObject();
        if (scopes.contains("thesis") && !scopes.contains("report")) check(impact.value("instructions").toArray() == QJsonArray{"aramf-thesis-instruction"}, "Thesis excludes Report instructions");
        if (scopes.contains("communication")) check(impact.value("affectedScopes").toArray() == QJsonArray{"android", "communication", "pico"}, "communication traverses only declared dependencies");
    }
    auto ui = requestFor({"ui"});
    auto uiContract = WorkerTaskServices::prepare(model, ui);
    check(uiContract.value("risk").toObject().value("level").toString() == "LOW", "isolated UI stays LOW");
    check(!uiContract.value("requiredEvidence").toArray().contains("ctest") && !uiContract.value("requiredEvidence").toArray().contains("memory-consistency"), "LOW avoids unrelated regression");
    check(uiContract.value("impact").toObject().value("unrelatedScopes").toArray().contains("governance"), "UI does not imply governance");
    const auto medium = WorkerTaskServices::prepare(model, requestFor({"thesis", "report"}));
    check(medium.value("risk").toObject().value("level").toString() == "MEDIUM" && medium.value("requiredEvidence").toArray().contains("affected-workflow-tests"), "two independent scopes select MEDIUM workflow coverage");
    const auto critical = WorkerTaskServices::prepare(model, requestFor({"governance"}));
    check(critical.value("risk").toObject().value("level").toString() == "CRITICAL" && critical.value("requiredEvidence").toArray().contains("ctest"), "schema has broad required evidence");
    const auto hardware = WorkerTaskServices::prepare(model, requestFor({"pico"}));
    check(hardware.value("risk").toObject().value("level").toString() == "HIGH" && hardware.value("requiredEvidence").toArray().contains("physical-certification"), "hardware requires physical evidence");
    auto unmapped = ui; unmapped.files = {"source/unmapped.cpp"};
    check(hasCode(WorkerTaskServices::prepare(model, unmapped), "TASK_SCOPE_VIOLATION"), "unmapped file blocked");
    auto unsafe = ui; unsafe.files = {"../outside.cpp"};
    check(WorkerTaskServices::prepare(model, unsafe).value("preflight").toObject().value("status").toString() == "FATAL", "traversal rejected");
    auto forbidden = ui; forbidden.files = {"ARAMF_WORKER/memory/event-log.jsonl"};
    check(!ready(WorkerTaskServices::prepare(model, forbidden)), "event history cannot be granted by intent");
    auto destructive = ui; destructive.destructive = true;
    check(!ready(WorkerTaskServices::prepare(model, destructive)), "destructive request does not invent authority");

    const auto initial = WorkerTaskServices::postflight(model, uiContract);
    check(initial.value("completionState").toString() == "IN_PROGRESS", "not verified without evidence");
    check(write(QDir(project.path()).filePath("source/ui.cpp"), "implemented\n"), "modify permitted file");
    auto post = WorkerTaskServices::postflight(model, uiContract);
    check(post.value("status").toString() == "PASS" && post.value("completionState").toString() == "IMPLEMENTED_UNVERIFIED", "implementation differs from verification");
    QJsonArray evidence;
    for (const auto& value : uiContract.value("requiredEvidence").toArray()) {
        if (value.toString() == "diff-boundary") continue;
        const QString path = "ARAMF_WORKER/verification/tasks/" + value.toString() + ".json";
        const QJsonObject record{{"check", value}, {"status", "PASS"}, {"contractId", uiContract.value("contractId")}, {"resultFingerprint", post.value("resultFingerprint")}};
        const QByteArray content = QJsonDocument(record).toJson();
        check(write(QDir(project.path()).filePath(path), content), "persist bound evidence fixture");
        evidence.append(QJsonObject{{"check", value}, {"artifact", path}, {"artifactFingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())}});
    }
    check(WorkerTaskServices::postflight(model, uiContract, evidence).value("completionState").toString() == "VERIFIED", "all bound software evidence permits VERIFIED");
    check(write(QDir(project.path()).filePath("source/ui.cpp"), "changed after validation\n"), "subsequent source change");
    check(WorkerTaskServices::postflight(model, uiContract, evidence).value("completionState").toString() == "IMPLEMENTED_UNVERIFIED", "stale evidence cannot verify later edits");
    const auto staleUi = WorkerTaskServices::postflight(model, uiContract, evidence);
    for (const QString& id : {QStringLiteral("affected-target-build"), QStringLiteral("ui-tests"), QStringLiteral("application-startup"), QStringLiteral("impacted-validation")})
        check(staleUi.value("missingEvidence").toArray().contains(id), "changed source invalidates " + id);
    auto tampered = uiContract; tampered.insert("permittedFiles", QJsonArray{"source/pico.cpp"});
    check(hasCode(WorkerTaskServices::postflight(model, tampered), "TASK_CONTRACT_TAMPERED"), "contract tampering rejected");
    check(write(QDir(project.path()).filePath("source/pico.cpp"), "out of scope\n"), "out-of-scope fixture edit");
    check(hasCode(WorkerTaskServices::postflight(model, uiContract), "TASK_SCOPE_VIOLATION"), "actual out-of-scope modification detected");
    check(write(QDir(project.path()).filePath("source/pico.cpp"), "before\n"), "restore test-owned source");
    // Baseline records actual bytes, including pre-existing user edits.
    uiContract = WorkerTaskServices::prepare(model, ui);
    check(WorkerTaskServices::postflight(model, uiContract).value("modifiedFiles").toArray().isEmpty(), "pre-existing modified bytes are not task changes");
    const QString logPath = QDir(project.path()).filePath("ARAMF_WORKER/memory/event-log.jsonl");
    const auto log = bytes(logPath);
    check(write(logPath, "fake history"), "test-only history corruption");
    check(hasCode(WorkerTaskServices::postflight(model, uiContract), "FORBIDDEN_FILE_MODIFICATION"), "event history rewriting detected");
    check(write(logPath, log), "restore test-owned history bytes");
    // Canonical recorder mutations remain legal when append-only history and
    // its derived cross-file consistency pass the existing memory validator.
    QString recorderError;
    check(ProjectMemory().recordOperation(project.path(), "test-result", QJsonObject{{"task", ui.goal}, {"status", "PASS"}, {"summary", "fixture evidence"}, {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "system"}, {"tool", "WorkerTaskTests"}}}}, nullptr, &recorderError), "canonical recorder operation");
    check(!hasCode(WorkerTaskServices::postflight(model, uiContract), "FORBIDDEN_FILE_MODIFICATION"), "legitimate recorder writes preserve their ownership boundary");

    // Persistence: metadata-only changes notify, legacy absence clears old state,
    // and reloading a different project never inherits policy from the last one.
    ProjectPersistence persistence;
    auto saved = persistence.toJson(model);
    ProjectModel loaded;
    QString error;
    check(persistence.fromJson(&loaded, saved, &error) && loaded.ruleConfiguration().scopeMetadata == rules.scopeMetadata, "metadata round-trip");
    auto legacy = saved; auto oldRules = legacy.value("rules").toObject(); oldRules.remove("scopeMetadata"); legacy.insert("rules", oldRules);
    check(persistence.fromJson(&loaded, legacy, &error) && loaded.ruleConfiguration().scopeMetadata.isEmpty(), "legacy input clears prior mappings");
    check(persistence.fromJson(&loaded, saved, &error) && loaded.ruleConfiguration().scopeMetadata == rules.scopeMetadata, "A/B/A metadata isolation");
    const auto normalizedSaved = persistence.toJson(loaded);
    check(persistence.fromJson(&loaded, normalizedSaved, &error) && QJsonDocument(persistence.toJson(loaded)).toJson() == QJsonDocument(normalizedSaved).toJson(), "repeated normalized persistence is byte stable");
    auto malformed = normalizedSaved; auto malformedRules = malformed.value("rules").toObject(); malformedRules.insert("scopeMetadata", "invalid"); malformed.insert("rules", malformedRules);
    check(!persistence.fromJson(&loaded, malformed, &error) && persistence.toJson(loaded) == normalizedSaved, "malformed metadata rejected without changing loaded project");
    auto changedRules = model.ruleConfiguration(); auto changedMetadata = changedRules.scopeMetadata.value("ui").toObject(); changedMetadata.insert("tests", QJsonArray{"replacement-tests"}); changedRules.scopeMetadata.insert("ui", changedMetadata);
    model.setRuleConfiguration(changedRules);
    check(model.ruleConfiguration().scopeMetadata == changedRules.scopeMetadata, "metadata-only setter updates model");
    check(hasCode(WorkerTaskServices::prepare(model, ui), "STALE_DERIVED_ARTIFACT"), "changed canonical mapping invalidates generated routes");
    model.setRuleConfiguration(rules);

    ProjectResource resource;
    resource.id = "external-thesis"; resource.location = "external/template.md"; resource.scopes = {"thesis"}; resource.role = "thesis-template";
    model.setResources({resource}); generate(model);
    check(hasCode(WorkerTaskServices::prepare(model, requestFor({"thesis"})), "USER_SOURCE_MISSING"), "missing template blocked without fabrication");
    check(write(QDir(project.path()).filePath(resource.location), "canonical template\n"), "external template fixture");
    const auto thesisContract = WorkerTaskServices::prepare(model, requestFor({"thesis"}));
    check(ready(thesisContract), "available scoped external template");
    check(write(QDir(project.path()).filePath(resource.location), "unauthorized replacement\n"), "test-only external overwrite");
    check(hasCode(WorkerTaskServices::postflight(model, thesisContract), "FORBIDDEN_FILE_MODIFICATION"), "external overwrite detected");
    resource.role = "source-of-truth"; ProjectResource second = resource; second.id = "second-source"; second.location = "external/second.md";
    model.setResources({resource}); generate(model);
    const auto sourceContract = WorkerTaskServices::prepare(model, requestFor({"thesis"}));
    check(write(QDir(project.path()).filePath(resource.location), "changed authoritative source\n"), "Source of Truth mutation fixture");
    check(hasCode(WorkerTaskServices::postflight(model, sourceContract), "FORBIDDEN_FILE_MODIFICATION"), "Source of Truth cannot be modified by task");
    auto missingSource = resource; missingSource.location = "external/missing-source.md"; model.setResources({missingSource}); generate(model);
    check(hasCode(WorkerTaskServices::prepare(model, requestFor({"thesis"})), "SOURCE_OF_TRUTH_MISSING"), "missing Source of Truth blocks without fabrication");
    check(write(QDir(project.path()).filePath(second.location), "second\n"), "second source fixture");
    model.setResources({resource, second}); generate(model);
    check(hasCode(WorkerTaskServices::prepare(model, requestFor({"thesis"})), "RESOURCE_AUTHORITY_CONFLICT"), "overlapping Source of Truth conflict blocks");
    model.setResources({}); generate(model);
    // Dependency-aware evidence across a mixed-scope task.
    const auto collectEvidence = [&](const QJsonObject& contract, const QJsonObject& post, const QString& prefix) {
        QJsonArray collected;
        for (const auto& value : contract.value("requiredEvidence").toArray()) {
            if (value.toString() == "diff-boundary" || value.toString() == "physical-certification") continue;
            const QString path = "ARAMF_WORKER/verification/tasks/" + prefix + '-' + value.toString() + ".json";
            const QJsonObject record{{"check", value}, {"status", "PASS"}, {"contractId", contract.value("contractId")},
                {"dependencyFingerprint", post.value("evidenceFingerprints").toObject().value(value.toString())}, {"resultFingerprint", post.value("resultFingerprint")}};
            const QByteArray content = QJsonDocument(record).toJson();
            check(write(QDir(project.path()).filePath(path), content), "persist result-bound evidence");
            collected.append(QJsonObject{{"check", value}, {"artifact", path}, {"artifactFingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())}});
        }
        return collected;
    };
    const auto mixedContract = WorkerTaskServices::prepare(model, requestFor({"thesis", "report"}));
    const auto mixedPost = WorkerTaskServices::postflight(model, mixedContract);
    const auto mixedEvidence = collectEvidence(mixedContract, mixedPost, "mixed");
    check(WorkerTaskServices::postflight(model, mixedContract, mixedEvidence).value("completionState").toString() == "VERIFIED", "mixed task software verified");
    check(write(QDir(project.path()).filePath("source/report.cpp"), "report changed\n"), "later report edit");
    const auto changedMixed = WorkerTaskServices::postflight(model, mixedContract, mixedEvidence);
    bool thesisEvidenceRetained = false;
    for (const auto& value : changedMixed.value("acceptedEvidence").toArray()) thesisEvidenceRetained |= value.toObject().value("check").toString() == "thesis-tests";
    check(thesisEvidenceRetained && changedMixed.value("missingEvidence").toArray().contains("report-tests"), "Report edit stales Report evidence but preserves Thesis evidence");
    check(hasCode(changedMixed, "STALE_EVIDENCE"), "stale evidence has stable diagnostic");
    check(changedMixed.value("missingEvidence").toArray().contains("affected-workflow-tests"), "Report change stales shared workflow evidence");

    // Explicitly expected regeneration is accepted only if it matches the
    // canonical service's expected content. Mere generated ownership is not enough.
    auto regenerationRules = model.ruleConfiguration();
    auto uiMetadata = regenerationRules.scopeMetadata.value("ui").toObject();
    uiMetadata.insert("generatedArtifacts", QJsonArray{"ARAMF_WORKER/project.json", "ARAMF_WORKER/worker-manifest.json", "ARAMF_WORKER/routing/task-routes.json", "ARAMF_WORKER/routing/scope-routes.json", "ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json"});
    regenerationRules.scopeMetadata.insert("ui", uiMetadata); model.setRuleConfiguration(regenerationRules); generate(model);
    const QString projectJsonPath = QDir(project.path()).filePath("ARAMF_WORKER/project.json");
    const auto projectJson = QJsonDocument::fromJson(bytes(projectJsonPath));
    for (const auto& path : uiMetadata.value("generatedArtifacts").toArray()) {
        const auto absolute = QDir(project.path()).filePath(path.toString());
        check(write(absolute, QJsonDocument::fromJson(bytes(absolute)).toJson(QJsonDocument::Compact)), "equivalent pre-existing derived formatting: " + path.toString());
    }
    const auto regenerationContract = WorkerTaskServices::prepare(model, ui);
    check(ready(regenerationContract), "regeneration contract READY");
    const auto repaired = GenerationServices().repairDerivedArtifacts(model, model.generationOptions());
    check(repaired.success, "canonical repair runs");
    const auto regenerationPost = WorkerTaskServices::postflight(model, regenerationContract);
    if (regenerationPost.value("status").toString() != "PASS") std::cerr << QJsonDocument(regenerationPost).toJson().toStdString();
    check(regenerationPost.value("status").toString() == "PASS" && !regenerationPost.value("serviceChanges").toArray().isEmpty(), "expected canonical regeneration accepted");
    for (const auto& path : uiMetadata.value("generatedArtifacts").toArray()) {
        bool observed = false;
        for (const auto& change : regenerationPost.value("serviceChanges").toArray()) observed |= change.toObject().value("path") == path;
        check(observed, "canonical producer checked per artifact: " + path.toString());
    }
    const auto wrongGeneratedContract = WorkerTaskServices::prepare(model, ui);
    auto wrongGenerated = projectJson.object(); wrongGenerated.insert("forged", true);
    check(write(projectJsonPath, QJsonDocument(wrongGenerated).toJson()), "wrong generated content fixture");
    check(hasCode(WorkerTaskServices::postflight(model, wrongGeneratedContract), "GENERATED_CONTENT_MISMATCH"), "generated owner cannot authorize arbitrary content");
    check(GenerationServices().repairDerivedArtifacts(model, model.generationOptions()).success, "restore wrong-content fixture");
    const auto noRegenerationContract = WorkerTaskServices::prepare(model, requestFor({"thesis"}));
    check(write(projectJsonPath, projectJson.toJson(QJsonDocument::Compact)), "unrelated derived modification fixture");
    check(hasCode(WorkerTaskServices::postflight(model, noRegenerationContract), "UNEXPECTED_SERVICE_REGENERATION"), "undeclared generated change rejected");
    check(GenerationServices().repairDerivedArtifacts(model, model.generationOptions()).success, "restore canonical test fixture");

    const auto certify = [&](const QJsonObject& contract, const QJsonObject& post, const QString& level, const QString& prefix, bool wrongResult) {
        const QString path = "ARAMF_WORKER/verification/tasks/" + prefix + "-certification.json";
        const QByteArray content = QJsonDocument(QJsonObject{{"check", "certification"}, {"status", "PASS"}, {"contractId", contract.value("contractId")}, {"resultFingerprint", post.value("resultFingerprint")}}).toJson();
        check(write(QDir(project.path()).filePath(path), content), "physical/software certification evidence fixture");
        QJsonObject started; QString error;
        CertificationService certificates;
        check(certificates.start(project.path(), contract.value("contractId").toString(), "worker-task", "task", level, QJsonArray{"task-evidence"},
            QJsonObject{{"resultFingerprint", wrongResult ? QJsonValue("wrong-result") : post.value("resultFingerprint")}}, &started, &error), "canonical certificate start: " + error);
        const QJsonArray references{QJsonObject{{"reference", path}, {"fingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())},
            {"verified", true}, {"type", level == "HARDWARE_CERTIFIED" ? "physical" : "host-test"}}};
        check(certificates.issue(project.path(), started, "PASS", references, nullptr, &error), "canonical certificate issue: " + error);
        const auto context = ContextCoordinationService::generate(model);
        check(context.value(QStringLiteral("success")).toBool(), "refresh context after canonical certificate issue");
        return path;
    };
    const auto softwareContract = WorkerTaskServices::prepare(model, ui);
    const auto softwarePost = WorkerTaskServices::postflight(model, softwareContract);
    const auto softwareEvidence = collectEvidence(softwareContract, softwarePost, "software");
    check(WorkerTaskServices::postflight(model, softwareContract, softwareEvidence).value("completionState").toString() == "VERIFIED", "software VERIFIED before certification");
    certify(softwareContract, softwarePost, "HOST_TEST", "software", false);
    const auto softwareCertified = WorkerTaskServices::postflight(model, softwareContract, softwareEvidence);
    if (softwareCertified.value("completionState").toString() != "CERTIFIED") std::cerr << QJsonDocument(softwareCertified).toJson().toStdString();
    check(softwareCertified.value("completionState").toString() == "CERTIFIED", "software task CERTIFIED through canonical service");
    const auto physicalContract = WorkerTaskServices::prepare(model, requestFor({"pico"}));
    const auto physicalPost = WorkerTaskServices::postflight(model, physicalContract);
    const auto physicalEvidence = collectEvidence(physicalContract, physicalPost, "physical");
    const auto beforePhysical = WorkerTaskServices::postflight(model, physicalContract, physicalEvidence);
    check(beforePhysical.value("completionState").toString() == "VERIFIED" && beforePhysical.value("missingEvidence").toArray().contains("physical-certification"), "hardware software PASS cannot claim physical certification");
    const auto physicalPath = certify(physicalContract, physicalPost, "HARDWARE_CERTIFIED", "physical", false);
    check(WorkerTaskServices::postflight(model, physicalContract, physicalEvidence).value("completionState").toString() == "CERTIFIED", "hardware CERTIFIED with current canonical physical evidence");
    const auto oldCertification = model.certificationConfiguration(); auto changedCertification = oldCertification; changedCertification.defaultVerificationLevel = "PHYSICAL";
    model.setCertificationConfiguration(changedCertification);
    check(hasCode(WorkerTaskServices::postflight(model, physicalContract, physicalEvidence), "STALE_TASK_CONTRACT"), "certificate cannot validate a changed configuration");
    model.setCertificationConfiguration(oldCertification);
    auto physicalReadout = QJsonDocument::fromJson(bytes(QDir(project.path()).filePath(physicalPath))).object(); physicalReadout.insert("altered", true);
    check(write(QDir(project.path()).filePath(physicalPath), QJsonDocument(physicalReadout).toJson()), "alter physical evidence fixture");
    check(WorkerTaskServices::postflight(model, physicalContract, physicalEvidence).value("completionState").toString() == "VERIFIED", "stale physical certificate preserves software VERIFIED without CERTIFIED");
    const auto wrongContract = WorkerTaskServices::prepare(model, requestFor({"pico"}));
    const auto wrongPost = WorkerTaskServices::postflight(model, wrongContract);
    const auto wrongEvidence = collectEvidence(wrongContract, wrongPost, "wrong");
    certify(wrongContract, wrongPost, "HARDWARE_CERTIFIED", "wrong", true);
    check(WorkerTaskServices::postflight(model, wrongContract, wrongEvidence).value("completionState").toString() != "CERTIFIED", "wrong-result physical certification rejected");

    // Real Git dirty baseline, without creating any fixture commits.
    const auto runGit = [&](const QStringList& args) { QProcess command; command.start("git", QStringList{"-C", project.path()} + args); return command.waitForFinished(10000) && command.exitCode() == 0; };
    check(runGit({"init", "-q"}) && runGit({"add", "source"}), "Git fixture index initialized");
    check(write(QDir(project.path()).filePath("source/machine-learning.cpp"), "user dirty before prepare\n"), "unrelated pre-existing dirty file");
    const auto dirtyContract = WorkerTaskServices::prepare(model, ui);
    check(ready(dirtyContract) && WorkerTaskServices::postflight(model, dirtyContract).value("modifiedFiles").toArray().isEmpty(), "Git dirty baseline preserved and not attributed to task");
    check(write(QDir(project.path()).filePath("source/machine-learning.cpp"), "new unrelated task change\n"), "modify dirty unrelated file again");
    check(hasCode(WorkerTaskServices::postflight(model, dirtyContract), "TASK_SCOPE_VIOLATION"), "subsequent edit to already-dirty unrelated file detected");
    const auto permittedDirty = WorkerTaskServices::prepare(model, ui);
    check(write(QDir(project.path()).filePath("source/ui.cpp"), "allowed edit over pre-existing dirt\n"), "edit previously dirty permitted file");
    check(WorkerTaskServices::postflight(model, permittedDirty).value("status").toString() == "PASS", "pre-existing dirty permitted source remains mutable");
    auto ignoredRules = model.ruleConfiguration(); auto ignoredMetadata = ignoredRules.scopeMetadata.value("ui").toObject();
    ignoredMetadata.insert("files", QJsonArray{"source/ui.cpp", "ignored/input.h"}); ignoredRules.scopeMetadata.insert("ui", ignoredMetadata);
    model.setRuleConfiguration(ignoredRules); generate(model);
    QFile ignoredList(QDir(project.path()).filePath(".git/info/exclude"));
    check(ignoredList.open(QIODevice::Append) && ignoredList.write("\n/ignored/\n") > 0, "Git ignored dependency fixture"); ignoredList.close();
    check(write(QDir(project.path()).filePath("ignored/input.h"), "input A\n"), "ignored canonical input");
    const auto ignoredContract = WorkerTaskServices::prepare(model, ui);
    const auto ignoredBefore = WorkerTaskServices::postflight(model, ignoredContract);
    check(write(QDir(project.path()).filePath("ignored/input.h"), "input B\n"), "change ignored dependency");
    const auto ignoredAfter = WorkerTaskServices::postflight(model, ignoredContract);
    check(ignoredBefore.value("resultFingerprint") != ignoredAfter.value("resultFingerprint") && hasCode(ignoredAfter, "TASK_SCOPE_VIOLATION"), "ignored mapped dependencies cannot evade fingerprints or boundary checks");
    model.setRuleConfiguration(regenerationRules); generate(model);
    loaded.resetForNewProject(); check(loaded.ruleConfiguration().scopeMetadata.isEmpty(), "New Project clears task metadata");
    check(persistence.fromJson(&loaded, saved, &error), "reload A before template transition");
    TemplateManager templates;
    check(templates.applyTemplate(&loaded, "qt-desktop-application", &error) && loaded.ruleConfiguration().scopeMetadata.isEmpty(), "template transition clears former task metadata");
    QTemporaryDir otherProject;
    ProjectModel other; check(persistence.fromJson(&other, saved, &error), "load independent B");
    other.setProjectPath(otherProject.path()); other.setProjectId("task-test-B"); other.setWorkerNameSuffix("B");
    check(write(QDir(otherProject.path()).filePath("source/ui.cpp"), "B only\n"), "isolated B source"); generate(other);
    const auto otherContract = WorkerTaskServices::prepare(other, ui);
    check(ready(otherContract), "suffixed B preflight");
    check(hasCode(WorkerTaskServices::postflight(other, dirtyContract), "STALE_TASK_CONTRACT"), "A contract cannot authorize B");
    check(WorkerTaskServices::prepare(model, ui).value("binding").toObject().value("projectId").toString() == "task-test-A", "return to A restores only A identity");

    // Production CLI, including invalid preflight/postflight exit paths.
    model.setProjectFilePath(QDir(project.path()).filePath("project.aramf.json"));
    check(persistence.save(model, model.projectFilePath(), &error), "save canonical CLI model");
    ProjectModel cliModel; check(persistence.load(&cliModel, model.projectFilePath(), &error), "load canonical CLI model"); generate(cliModel);
    const QString requestPath = QDir(project.path()).filePath("task-request.json"); check(write(requestPath, QJsonDocument(ui.toJson()).toJson()), "CLI request fixture");
    const auto command = [&](const QStringList& args, QByteArray* output) {
        QProcess process; process.start(QDir(QCoreApplication::applicationDirPath()).filePath("aramf.exe"), args);
        if (!process.waitForFinished(30000)) return -1;
        *output = process.readAllStandardOutput(); return process.exitCode();
    };
    QByteArray cliOutput;
    check(command({"task", "prepare", "--config", model.projectFilePath(), "--request", requestPath}, &cliOutput) == 0, "production task prepare succeeds");
    const auto cliContract = QJsonDocument::fromJson(cliOutput).object().value("contract").toObject();
    const QString contractPath = QDir(project.path()).filePath("ARAMF_WORKER/verification/tasks/prepared.json"); check(write(contractPath, cliOutput), "persist derived CLI contract");
    check(command({"task", "postflight", "--config", model.projectFilePath(), "--contract", contractPath}, &cliOutput) == 2
        && QJsonDocument::fromJson(cliOutput).object().value("completionState").toString() != "VERIFIED", "CLI missing evidence returns nonzero");
    const auto cliEvidence = collectEvidence(cliContract, WorkerTaskServices::postflight(cliModel, cliContract), "cli");
    const auto evidencePath = QDir(otherProject.path()).filePath("cli-evidence-index.json");
    check(write(evidencePath, QJsonDocument(QJsonObject{{"evidence", cliEvidence}}).toJson()), "external caller evidence index fixture");
    check(command({"task", "postflight", "--config", model.projectFilePath(), "--contract", contractPath, "--evidence", evidencePath}, &cliOutput) == 0
        && QJsonDocument::fromJson(cliOutput).object().value("completionState").toString() == "VERIFIED", "production postflight returns success only for verified evidence");
    check(write(requestPath, QJsonDocument(unmapped.toJson()).toJson()), "CLI unmapped fixture");
    check(command({"task", "prepare", "--config", model.projectFilePath(), "--request", requestPath}, &cliOutput) == 2, "CLI blocked preflight returns nonzero");
    check(command({"task", "invalid", "--config", model.projectFilePath()}, &cliOutput) == 2, "CLI invalid command returns nonzero");
    check(command({"task", "prepare"}, &cliOutput) == 2, "CLI missing arguments returns nonzero");
    const QJsonObject ownershipContract{{"contractId", "ownership-contract-a"},
                                        {"binding", QJsonObject{{"projectId", model.projectId()}}},
                                        {"preflight", QJsonObject{{"status", "READY"}}},
                                        {"permittedFiles", QJsonArray{"source/ui.cpp"}}};
    const QJsonObject independentContract{{"contractId", "ownership-contract-c"},
                                          {"binding", QJsonObject{{"projectId", model.projectId()}}},
                                          {"preflight", QJsonObject{{"status", "READY"}}},
                                          {"permittedFiles", QJsonArray{"source/generic.cpp"}}};
    const auto claimA = RuntimeOwnershipService::claim(&model, ownershipContract, "task-a", "worker-a", {"source/ui.cpp"});
    check(claimA.value("granted").toBool() && claimA.value("code").toString() == "OWNERSHIP_GRANTED", "canonical runtime ownership grants permitted resource");
    const auto equivalentConflict = RuntimeOwnershipService::claim(&model, ownershipContract, "task-b", "worker-b", {"source/../source/ui.cpp"});
    check(!equivalentConflict.value("granted").toBool() && equivalentConflict.value("code").toString() == "OWNERSHIP_CONFLICT", "canonical ownership normalizes equivalent paths");
    const auto independentClaim = RuntimeOwnershipService::claim(&model, independentContract, "task-c", "worker-c", {"source/generic.cpp"});
    check(independentClaim.value("granted").toBool(), "independent canonical resource is claimable");
    check(RuntimeOwnershipService::release(&model, "task-a", "worker-a").value("code").toString() == "OWNERSHIP_RELEASED", "canonical ownership releases on success");
    check(RuntimeOwnershipService::release(&model, "task-a", "worker-a").value("code").toString() == "OWNERSHIP_ALREADY_RELEASED", "duplicate release is deterministic");
    check(RuntimeOwnershipService::markWorkerUnavailable(&model, "worker-c").value("code").toString() == "OWNERSHIP_RECOVERABLE", "worker loss makes claim recoverable");
    check(RuntimeOwnershipService::recoverWorker(&model, "worker-c").value("code").toString() == "OWNERSHIP_RECOVERED", "stale ownership is explicitly recovered");
    const QJsonObject wrongProject = ownershipContract;
    auto wrongBinding = wrongProject;
    wrongBinding.insert("binding", QJsonObject{{"projectId", "task-test-B"}});
    check(RuntimeOwnershipService::claim(&model, wrongBinding, "task-wrong-project", "worker-d", {"source/ui.cpp"}).value("code").toString() == "TASK_CONTRACT_INVALID", "ownership rejects a contract bound to another project");
    check(RuntimeOwnershipService::claim(&model, ownershipContract, "task-forbidden", "worker-e", {"source/not-permitted.cpp"}).value("code").toString() == "OWNERSHIP_NOT_PERMITTED", "ownership rejects a resource outside the contract");
    check(RuntimeOwnershipService::claim(&model, ownershipContract, "task-escape", "worker-f", {"../outside.cpp"}).value("code").toString() == "OWNERSHIP_NOT_PERMITTED", "ownership rejects a project-root escape");
    ProjectModel concurrentModel;
    concurrentModel.setProjectPath(project.path());
    concurrentModel.setProjectId(model.projectId() + "-concurrent");
    auto concurrentContract = independentContract;
    concurrentContract.insert("binding", QJsonObject{{"projectId", concurrentModel.projectId()}});
    std::atomic<int> grantedClaims{0};
    std::thread claimantA([&] { if (RuntimeOwnershipService::claim(&concurrentModel, concurrentContract, "parallel-a", "worker-a", {"source/generic.cpp"}).value("granted").toBool()) ++grantedClaims; });
    std::thread claimantB([&] { if (RuntimeOwnershipService::claim(&concurrentModel, concurrentContract, "parallel-b", "worker-b", {"source/generic.cpp"}).value("granted").toBool()) ++grantedClaims; });
    claimantA.join(); claimantB.join();
    check(grantedClaims == 1, "concurrent claims have one canonical winner");
    const QString ownershipPath = QDir(project.path()).filePath("ownership.aramf.json");
    ProjectPersistence ownershipPersistence;
    check(ownershipPersistence.save(model, ownershipPath), "runtime ownership persists");
    ProjectModel ownershipReload;
    check(ownershipPersistence.load(&ownershipReload, ownershipPath) && RuntimeOwnershipService::inspect(ownershipReload).value("claims").toArray().size() == 2, "released ownership history survives reload");
    const QString manifestPath = QDir(project.path()).filePath("ARAMF_WORKER/worker-manifest.json");
    const QByteArray manifest = bytes(manifestPath);
    check(write(QDir(project.path()).filePath("ARAMF_WORKER/duplicate/worker-manifest.json"), manifest), "duplicate manifest fixture");
    check(hasCode(WorkerTaskServices::prepare(model, ui), "CANONICAL_OWNER_DUPLICATE"), "parallel state blocked");
    for (const auto& scenario : QStringList{"exact", "delete", "fake", "ambiguous", "unauthorized", "escape", "existing"}) {
        QTemporaryDir relocationFixture; relocationFixture.setAutoRemove(false);
        const QString relocationRoot = relocationFixture.path() + "/project";
        QDir().mkpath(relocationRoot);
        ProjectModel relocated; relocated.setProjectPath(relocationRoot);
        relocated.setAiConfiguration(ai);
        auto relocationRules = relocated.ruleConfiguration();
        relocationRules.projectScopes = {"source-code"};
        relocationRules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray{"src/old.cpp", "src/new.cpp", "src/other.cpp"}},
            {"tests", QJsonArray{"relocation-tests"}}, {"riskTraits", QJsonArray{}}, {"affects", QJsonArray{}}, {"generatedArtifacts", QJsonArray{}}}}};
        relocated.setRuleConfiguration(relocationRules);
        check(write(relocationRoot + "/src/old.cpp", "original governed content\n"), "relocation fixture original");
        const auto runGit = [&](const QStringList& arguments) {
            QProcess process; process.setWorkingDirectory(relocationRoot); process.start("git", arguments);
            return process.waitForFinished(30000) && process.exitCode() == 0;
        };
        check(runGit({"init"}) && runGit({"add", "src/old.cpp"})
            && runGit({"-c", "user.name=Relocation Test", "-c", "user.email=relocation@example.invalid", "-c", "commit.gpgsign=false", "commit", "-m", "isolated relocation baseline"}), "relocation committed baseline");
        generate(relocated);
        WorkerTaskRequest move;
        move.goal = "Relocate the exact governed source"; move.type = "coding"; move.scopes = {"source-code"};
        move.files = {"src/old.cpp", "src/new.cpp", "src/other.cpp"}; move.definitionOfDone = {"Byte identity and scoped authorization"};
        if (scenario != "unauthorized") move.relocations = {QJsonObject{{"oldPath", "src/old.cpp"}, {"newPath", "src/new.cpp"}}};
        if (scenario == "ambiguous") move.relocations.append(QJsonObject{{"oldPath", "src/old.cpp"}, {"newPath", "src/other.cpp"}});
        if (scenario == "escape") move.relocations = {QJsonObject{{"oldPath", "src/old.cpp"}, {"newPath", "../escape.cpp"}}};
        if (scenario == "existing") write(relocationRoot + "/src/new.cpp", "original governed content\n");
        const auto prepared = WorkerTaskServices::prepare(relocated, move);
        if (scenario == "ambiguous" || scenario == "escape" || scenario == "existing") {
            check(!ready(prepared) && hasCode(prepared, "RELOCATION_UNPROVEN"), "reject " + scenario + " relocation at preflight");
            continue;
        }
        check(ready(prepared), "relocation preflight " + scenario);
        const QString target = scenario == "delete" ? relocationFixture.path() + "/preserved-outside-project.cpp" : relocationRoot + "/src/new.cpp";
        check(QFile::rename(relocationRoot + "/src/old.cpp", target), "isolated fixture move, never delete content");
        if (scenario == "fake") write(target, "unrelated replacement\n");
        const auto observed = WorkerTaskServices::postflight(relocated, prepared);
        if (scenario == "exact") {
            check(observed.value("status") == "PASS" && observed.value("relocations").toArray().size() == 1,
                  "authorized exact relocation accepted with audit record");
            check(observed.value("completionState") != "VERIFIED", "relocation alone cannot replace required test evidence");
            check(observed.value("relocations").toArray().first().toObject().value("contractId") == prepared.value("contractId"), "relocation bound to task identity");
        } else check(observed.value("status") == "FAIL" && hasCode(observed, "FORBIDDEN_FILE_MODIFICATION"), "reject " + scenario + " as unauthorized deletion");
    }
    QTemporaryDir folderTaskFixture; folderTaskFixture.setAutoRemove(false);
    ProjectModel folderTask; folderTask.setProjectPath(folderTaskFixture.path()); folderTask.setAiConfiguration(ai);
    auto folderRules = folderTask.ruleConfiguration(); folderRules.projectScopes = {"source-code"};
    folderRules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray{"bundle/allowed.cpp"}}, {"tests", QJsonArray{"folder-tests"}},
        {"generatedArtifacts", QJsonArray{"ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json"}}}}};
    folderTask.setRuleConfiguration(folderRules);
    write(folderTaskFixture.path() + "/bundle/allowed.cpp", "before\n");
    write(folderTaskFixture.path() + "/bundle/protected.cpp", "protected\n");
    ProjectResource bundle; bundle.id = "folder-task"; bundle.type = "folder"; bundle.location = "bundle"; bundle.scopes = {"source-code"};
    folderTask.setResources({bundle}); generate(folderTask);
    WorkerTaskRequest folderRequest; folderRequest.goal = "Modify one mapped folder child"; folderRequest.type = "coding";
    folderRequest.scopes = {"source-code"}; folderRequest.files = {"bundle/allowed.cpp"}; folderRequest.definitionOfDone = {"Only mapped child changes"};
    const auto folderContract = WorkerTaskServices::prepare(folderTask, folderRequest);
    check(ready(folderContract), "folder preflight accepts supported folder type");
    write(folderTaskFixture.path() + "/bundle/allowed.cpp", "after\n");
    ContextCoordinationService::generate(folderTask); VerificationServices().verify(folderTask, folderTask.generationOptions());
    const auto folderPostflight = WorkerTaskServices::postflight(folderTask, folderContract);
    if (folderPostflight.value("status") != "PASS") std::cerr << QJsonDocument(folderPostflight.value("errors").toArray()).toJson().toStdString();
    check(folderPostflight.value("status") == "PASS", "authorized child passes folder aggregate boundary");
    write(folderTaskFixture.path() + "/bundle/protected.cpp", "unauthorized\n");
    ContextCoordinationService::generate(folderTask); VerificationServices().verify(folderTask, folderTask.generationOptions());
    check(WorkerTaskServices::postflight(folderTask, folderContract).value("status") == "FAIL", "unauthorized folder child remains blocked");

    QTemporaryDir grantFixture; grantFixture.setAutoRemove(false);
    ProjectModel grantModel; grantModel.setProjectPath(grantFixture.path()); grantModel.setAiConfiguration(ai);
    WorkerTaskRequest grantTask;
    grantTask.goal = "Implement P6 Canonical Code Bank"; grantTask.type = "coding"; grantTask.scopes = {"source-code"};
    grantTask.files = {"CMakeLists.txt", "src/processes/P6/CodeBank.h", "src/processes/P6/CodeBank.cpp",
        "src/processes/P6/CodeBankStorage.cpp", "src/processes/P6/CodeBankVerification.cpp", "src/processes/P6/README.md", "tests/P6CodeBankTests.cpp"};
    grantTask.definitionOfDone = {"Exact approved P6 build registration"};
    auto grantRules = grantModel.ruleConfiguration(); grantRules.projectScopes = {"source-code"};
    grantRules.scopeMetadata = {{"source-code", QJsonObject{{"files", QJsonArray::fromStringList(grantTask.files)}, {"tests", QJsonArray{"grant-tests"}},
        {"generatedArtifacts", QJsonArray{"ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json"}}}}};
    grantModel.setRuleConfiguration(grantRules);
    write(grantFixture.path() + "/CMakeLists.txt", "# CMakeLists.txt\n");
    ProjectResource cmakeResource; cmakeResource.id = "cmake"; cmakeResource.location = "CMakeLists.txt";
    cmakeResource.role = "source-of-truth"; cmakeResource.scopes = {"source-code"}; grantModel.setResources({cmakeResource});
    generate(grantModel);
    const QString instruction = "Admin Morgan Lindbom explicitly authorizes override for isolated P6 grant test";
    const QString expires = QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODate);
    check(!ready(WorkerTaskServices::prepare(grantModel, grantTask)), "CMake mapping alone never grants protected write");
    check(WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, "unverified override", expires).value("status") == "BLOCKED", "unverified admin rejected");
    check(WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, instruction, "2000-01-01T00:00:00Z").value("status") == "BLOCKED", "expired issuance rejected");
    grantModel.setWorkerNameSuffix("other");
    check(WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, instruction, expires).value("status") == "BLOCKED", "named-worker authority cannot borrow default-worker grant");
    grantModel.setWorkerNameSuffix({});
    const auto issuedGrant = WorkerTaskServices::issueP6BuildGrant(grantModel, grantTask, instruction, expires);
    check(issuedGrant.value("status") == "PASS", "scoped grant issued through administrative workflow");
    const auto grant = issuedGrant.value("grant").toObject();
    grantTask.protectedGrantId = grant.value("grantId").toString();
    ContextCoordinationService::generate(grantModel); VerificationServices().verify(grantModel, grantModel.generationOptions());
    const auto grantContract = WorkerTaskServices::prepare(grantModel, grantTask);
    check(ready(grantContract), "valid exact grant permits protected CMake in preparation");
    auto forgedTask = grantTask; forgedTask.protectedGrantId = "forged-id";
    check(hasCode(WorkerTaskServices::prepare(grantModel, forgedTask), "PROTECTED_GRANT_INVALID"), "forged grant rejected");
    auto wrongTask = grantTask; wrongTask.goal += " unrelated";
    check(hasCode(WorkerTaskServices::prepare(grantModel, wrongTask), "PROTECTED_GRANT_INVALID"), "wrong task rejected");
    auto wrongPath = grantTask; wrongPath.files.append("LICENSE");
    check(hasCode(WorkerTaskServices::prepare(grantModel, wrongPath), "PROTECTED_GRANT_INVALID"), "wrong path set rejected");
    for (const auto& scenario : QStringList{"expired", "wrong-path", "wrong-operation", "wrong-worker"}) {
        auto invalidGrant = grant;
        const QString testId = "negative-" + scenario;
        invalidGrant.insert("grantId", testId);
        if (scenario == "expired") invalidGrant.insert("expiresAt", "2000-01-01T00:00:00Z");
        if (scenario == "wrong-path") invalidGrant.insert("path", "LICENSE");
        if (scenario == "wrong-operation") invalidGrant.insert("operation", "DELETE");
        if (scenario == "wrong-worker") invalidGrant.insert("workerIdentity", "ARAMF_WORKER_other");
        QString authorityError;
        check(ProjectMemory().recordAdministrativeOverride(grantFixture.path(), instruction, "isolated negative grant fixture",
            "Exercise fail-closed validation", "project", "test", {"CMakeLists.txt"}, {"P6"}, false,
            {{"protectedPathGrant", invalidGrant}}, nullptr, &authorityError), "negative audit fixture " + scenario);
        auto invalidTask = grantTask; invalidTask.protectedGrantId = testId;
        const auto invalidContract = WorkerTaskServices::prepare(grantModel, invalidTask);
        check(hasCode(invalidContract, "PROTECTED_GRANT_INVALID"), "reject " + scenario + " grant");
        check(hasCode(WorkerTaskServices::postflight(grantModel, invalidContract), "PROTECTED_GRANT_INVALID"), "postflight rejects " + scenario + " grant");
    }
    write(grantFixture.path() + "/CMakeLists.txt", "unapproved replacement\n");
    check(hasCode(WorkerTaskServices::prepare(grantModel, grantTask), "PROTECTED_GRANT_INVALID"), "grant cannot authorize arbitrary CMake bytes");
    check(hasCode(WorkerTaskServices::postflight(grantModel, grantContract), "PROTECTED_GRANT_INVALID"), "postflight rejects arbitrary CMake bytes");
    write(grantFixture.path() + "/CMakeLists.txt", "# CMakeLists.txt\n" + issuedGrant.value("registration").toString().toUtf8());
    ContextCoordinationService::generate(grantModel); VerificationServices().verify(grantModel, grantModel.generationOptions());
    const auto grantPostflight = WorkerTaskServices::postflight(grantModel, grantContract);
    if (grantPostflight.value("status") != "PASS") std::cerr << QJsonDocument(grantPostflight.value("errors").toArray()).toJson().toStdString();
    check(grantPostflight.value("status") == "PASS", "exact CMake edit accepted by postflight");
    check(WorkerTaskServices::consumeP6BuildGrant(grantModel, grantContract, {}).value("status") == "BLOCKED", "incomplete evidence cannot consume grant");
    QJsonArray grantEvidence;
    for (const auto& value : grantContract.value("requiredEvidence").toArray()) {
        if (value == "diff-boundary") continue;
        const QString path = "ARAMF_WORKER/verification/tasks/grant-" + value.toString() + ".json";
        const QJsonObject record{{"check", value}, {"contractId", grantContract.value("contractId")}, {"status", "PASS"},
            {"dependencyFingerprint", grantPostflight.value("evidenceFingerprints").toObject().value(value.toString())}};
        const auto content = QJsonDocument(record).toJson(); write(grantFixture.path() + '/' + path, content);
        grantEvidence.append(QJsonObject{{"check", value}, {"artifact", path},
            {"artifactFingerprint", QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex())}});
    }
    check(WorkerTaskServices::consumeP6BuildGrant(grantModel, grantContract, grantEvidence).value("status") == "PASS", "verified fixture consumes grant once");
    check(hasCode(WorkerTaskServices::prepare(grantModel, grantTask), "PROTECTED_GRANT_INVALID"), "consumed grant rejected in preparation");
    check(hasCode(WorkerTaskServices::postflight(grantModel, grantContract), "PROTECTED_GRANT_INVALID"), "consumed grant rejected in postflight");
    check(WorkerTaskServices::consumeP6BuildGrant(grantModel, grantContract, grantEvidence).value("status") == "BLOCKED", "replay consumption rejected");
    std::cout << "WORKER-TASK checks=" << checks << " failed=" << failed << " matrix=" << matrix.size() << '\n';
    return failed == 0;
}
