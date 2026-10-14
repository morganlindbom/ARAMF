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
#include <QLockFile>
#include <QMap>
#include <QProcess>
#include <QTemporaryDir>
#include <QDateTime>
#include <atomic>
#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>

QJsonObject canonicalCertificationOperationAssertions();

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

// Adversarial fixture contracts need valid checksums so the tested authority
// boundary, rather than an unrelated checksum failure, decides the result.
QJsonValue canonicalValue(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject result;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) result.insert(it.key(), canonicalValue(it.value()));
        return result;
    }
    if (value.isArray()) {
        QList<QByteArray> values;
        for (const auto& entry : value.toArray())
            values.append(QJsonDocument(QJsonArray{canonicalValue(entry)}).toJson(QJsonDocument::Compact));
        std::sort(values.begin(), values.end());
        QJsonArray result;
        for (const auto& entry : values) result.append(QJsonDocument::fromJson(entry).array().first());
        return result;
    }
    return value;
}
void checksumFixtureContract(QJsonObject& contract)
{
    contract.remove("contractId");
    contract.insert("contractId", QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(canonicalValue(contract).toObject()).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex()));
}
}

bool runWorkerTaskTests()
{
    // Verify task permission and evidence boundaries using isolated fixtures.

    // Grant PASS records below are synthetic test evidence only; they cannot
    // certify the live repository or authorize its protected CMake file.
    int checks = 0, failed = 0;
    QJsonObject acceptanceAssertions;
    QSet<QString> requiredAssertions;
    const QStringList acceptanceProfiles{"task-reauthorization-security/v1", "task-certification-operations/v1"};
    for (const auto& id : acceptanceProfiles)
        for (const auto& criterion : WorkerTaskServices::acceptanceProfile(id).value("criteria").toArray())
            for (const auto& assertion : criterion.toObject().value("assertions").toArray()) requiredAssertions.insert(assertion.toString());
    const auto check = [&](bool pass, const QString& name) {
        ++checks;
        if (!pass) { ++failed; std::cerr << "TASK FAIL: " << name.toStdString() << '\n'; }
        if (requiredAssertions.contains(name))
            acceptanceAssertions.insert(name, !pass || acceptanceAssertions.value(name) == "FAIL" ? "FAIL" : "PASS");
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
    const auto originalRequestBytes = freshPrepare.value("request").toObject();
    check(WorkerTaskRequest::fromJson(originalRequestBytes).toJson() == originalRequestBytes
          && !originalRequestBytes.contains("acceptanceProfile"), "optional acceptance profile preserves original request serialization byte shape");
    auto arbitraryProfileRequest = requestFor({"ui"});
    arbitraryProfileRequest.acceptanceProfile = "task-reauthorization-security/v1";
    check(hasCode(WorkerTaskServices::prepare(model, arbitraryProfileRequest), "ACCEPTANCE_PROFILE_INVALID"),
          "arbitrary freeform requirements and unrelated scope cannot inherit canonical profile acceptance");
    auto forgedProfileContract = freshPrepare;
    auto forgedProfileRequest = forgedProfileContract.value("request").toObject();
    forgedProfileRequest.insert("acceptanceProfile", arbitraryProfileRequest.acceptanceProfile);
    forgedProfileContract.insert("request", forgedProfileRequest);
    checksumFixtureContract(forgedProfileContract);
    check(hasCode(WorkerTaskServices::postflight(model, forgedProfileContract), "TASK_PREFLIGHT_BLOCKED")
          && WorkerTaskServices::runCheck(model, forgedProfileContract, "acceptance-criteria").value("status") == "BLOCKED",
          "recomputed contract checksum cannot bypass exact acceptance-profile requirements");
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
            if (observed.value("status") != "PASS")
                QTextStream(stderr) << "Relocation postflight diagnostics: " << QJsonDocument(observed).toJson(QJsonDocument::Compact) << '\n';
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

    // Exercise an approved reauthorization from immutable origin records through successor postflight.
    QTemporaryDir reauthFixture;
    ProjectModel reauthModel;
    reauthModel.setProjectPath(reauthFixture.path());
    reauthModel.setProjectId("reauthorization-test");
    auto reauthAi = reauthModel.aiConfiguration();
    reauthAi.permissions = {"read-project-files", "modify-files", "create-files", "run-project-tools", "run-tests", "run-builds"};
    reauthModel.setAiConfiguration(reauthAi);
    auto reauthMemory = reauthModel.memoryConfiguration();
    reauthMemory.maintenanceOptions = {"record-test-results", "update-current-state"};
    reauthModel.setMemoryConfiguration(reauthMemory);
    check(write(reauthFixture.path() + "/source/ui.cpp", "before ui\n"), "write reauthorization ui source");
    check(write(reauthFixture.path() + "/source/other.cpp", "before other\n"), "write reauthorization other source");
    auto reauthRules = reauthModel.ruleConfiguration();
    reauthRules.projectScopes = {"ui", "other"};
    reauthRules.scopeMetadata = {
        {"ui", QJsonObject{{"files", QJsonArray{"source/ui.cpp"}}, {"tests", QJsonArray{"ui-tests"}}}},
        {"other", QJsonObject{{"files", QJsonArray{"source/other.cpp"}}, {"tests", QJsonArray{"other-tests"}}}}};
    reauthModel.setRuleConfiguration(reauthRules);
    check(write(reauthFixture.path() + "/.gitignore", "build/\n"), "ignore isolated immutable check artifacts");
    generate(reauthModel);
    const auto git = [&](const QStringList& args) {
        QProcess process;
        process.start("git", QStringList{"-C", reauthFixture.path()} + args);
        return process.waitForStarted() && process.waitForFinished() && process.exitCode() == 0;
    };
    check(git({"init"}), "initialize isolated reauthorization repository");
    check(git({"config", "user.email", "worker-task-tests@example.invalid"}), "configure isolated git email");
    check(git({"config", "user.name", "Worker Task Tests"}), "configure isolated git name");
    check(git({"add", "-A"}) && git({"commit", "-m", "fixture baseline"}), "commit isolated reauthorization baseline");
    WorkerTaskRequest originTask;
    originTask.goal = "Implement the ui workflow change";
    originTask.type = "coding";
    originTask.scopes = {"ui"};
    originTask.files = {"source/ui.cpp"};
    originTask.definitionOfDone = {"The ui behavior is implemented and validated."};
    const auto originContract = WorkerTaskServices::prepare(reauthModel, originTask);
    check(ready(originContract), "reauthorization origin contract is READY");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui\n"), "edit authorized source for origin report");
    check(write(reauthFixture.path() + "/source/other.cpp", "after other\n"), "edit out-of-scope source for origin report");
    const auto originReport = WorkerTaskServices::postflight(reauthModel, originContract);
    check(originReport.value("status") == "FAIL", "origin postflight records scope failure");
    const QByteArray originContractBytes = QJsonDocument(originContract).toJson(QJsonDocument::Indented);
    const QByteArray originReportBytes = QJsonDocument(originReport).toJson(QJsonDocument::Indented);
    const QByteArray originBaselineBytes = QJsonDocument(originContract.value("baseline").toObject()).toJson(QJsonDocument::Indented);
    const QByteArray originEvidenceBytes = QJsonDocument(originReport.value("acceptedEvidence").toArray()).toJson(QJsonDocument::Indented);
    QTemporaryDir reauthArchive;
    const QString originContractPath = reauthArchive.path() + "/origin-contract-record.json";
    const QString originReportPath = reauthArchive.path() + "/origin-postflight-record.json";
    const QString originBaselinePath = reauthArchive.path() + "/origin-baseline-record.json";
    const QString originEvidencePath = reauthArchive.path() + "/origin-evidence-record.json";
    check(write(originContractPath, originContractBytes) && write(originReportPath, originReportBytes)
          && write(originBaselinePath, originBaselineBytes) && write(originEvidencePath, originEvidenceBytes),
          "persist byte-stability test copies of original records");
    const QJsonObject grantOutputs{{"ARAMF_WORKER/project.json", "REGENERATE"},
        {"ARAMF_WORKER/worker-manifest.json", "REGENERATE"},
        {"ARAMF_WORKER/routing/scope-routes.json", "REGENERATE"},
        {"ARAMF_WORKER/routing/task-routes.json", "REGENERATE"},
        {"ARAMF_WORKER/context/context-index.json", "REGENERATE"},
        {"ARAMF_WORKER/context/compressed-context.json", "REGENERATE"},
        {"ARAMF_WORKER/context/freshness.json", "REGENERATE"},
        {"ARAMF_WORKER/context/agent-adapters.json", "REGENERATE"},
        {"ARAMF_WORKER/context/task-dag.json", "REGENERATE"},
        {"ARAMF_WORKER/verification/latest-validation.json", "REGENERATE"},
        {"ARAMF_WORKER/verification/verification-result.json", "REGENERATE"},
        {"ARAMF_WORKER/verification/generation-state.json", "REGENERATE"}};
    TaskReauthorizationRequest reauthRequest;
    reauthRequest.task.goal = "Reauthorize the exact existing ui and other source edits";
    reauthRequest.task.type = "coding";
    reauthRequest.task.scopes = {"ui", "other"};
    reauthRequest.task.files = {"source/ui.cpp", "source/other.cpp"};
    reauthRequest.task.definitionOfDone = {"Only the reviewed source edits and canonical refreshes are accepted."};
    for (auto it = grantOutputs.begin(); it != grantOutputs.end(); ++it) {
        reauthRequest.canonicalInputs.append(it.key());
        reauthRequest.operations.insert(it.key(), it.value());
    }
    reauthRequest.operations.insert("source/ui.cpp", "MODIFY");
    reauthRequest.operations.insert("source/other.cpp", "MODIFY");
    reauthRequest.validityMinutes = 60;
    const QString reauthInstruction = "Admin Morgan Lindbom explicitly authorizes override for the isolated reauthorization test";
    auto reauthApproval = WorkerTaskServices::authorizeReauthorization(reauthModel, originContract, originReport,
        reauthRequest, reauthInstruction);
    check(reauthApproval.value("status") == "PASS", "exact reauthorization authority is appended");
    auto wrongPreparationOrigin = originContract;
    wrongPreparationOrigin.insert("completionState", "different-origin");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), wrongPreparationOrigin, originReport),
          "REAUTHORIZATION_BINDING_MISMATCH"), "unused approval rejects a different original contract");
    auto wrongPreparationReport = originReport;
    wrongPreparationReport.insert("completionState", "different-report");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, wrongPreparationReport),
          "REAUTHORIZATION_BINDING_MISMATCH"), "unused approval rejects a different original report");
    check(write(reauthFixture.path() + "/source/ui.cpp", "changed after approval\n"), "alter reviewed source fixture");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, originReport),
          "REAUTHORIZATION_SOURCE_CHANGED"), "unused approval rejects a changed source fingerprint");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui\n"), "restore reviewed fixture bytes");
    QLockFile heldBindingLock(QDir::tempPath() + "/aramf-reauthorization-"
        + QString::fromLatin1(QCryptographicHash::hash(QDir(reauthFixture.path()).canonicalPath().toUtf8(),
            QCryptographicHash::Sha256).toHex()) + ".lock");
    check(heldBindingLock.tryLock(0), "hold the independent reauthorization binding lock");
    const auto beforeBusyPreparation = treeBytes(reauthFixture.path());
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, originReport), "REAUTHORIZATION_BUSY")
          && treeBytes(reauthFixture.path()) == beforeBusyPreparation, "concurrent binding is blocked without consuming authority");
    heldBindingLock.unlock();
    auto reauthContract = WorkerTaskServices::prepareReauthorized(reauthModel,
        reauthApproval.value("authorizationId").toString(), originContract, originReport);
    if (!ready(reauthContract))
        std::cerr << "REAUTH PREFLIGHT: " << QJsonDocument(reauthContract.value("preflight").toObject().value("errors").toArray()).toJson().toStdString() << '\n';
    check(ready(reauthContract), "authorized successor contract is READY");
    const auto approvedEventList = ProjectMemory().events(reauthFixture.path());
    QJsonObject persistedGrant;
    for (const auto& event : approvedEventList)
        if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == reauthApproval.value("authorizationId"))
            persistedGrant = event.value("taskReauthorizationGrant").toObject();
    check(persistedGrant.value("originContractSnapshot") == originContract
          && persistedGrant.value("originPostflightSnapshot") == originReport
          && persistedGrant.value("originEvidenceSnapshot") == originReport.value("acceptedEvidence"),
          "grant retains byte-stable origin contract, baseline, report and evidence snapshots");
    check(QJsonDocument(originContract).toJson(QJsonDocument::Indented) == originContractBytes
          && QJsonDocument(originReport).toJson(QJsonDocument::Indented) == originReportBytes
          && bytes(originContractPath) == originContractBytes && bytes(originReportPath) == originReportBytes
          && bytes(originBaselinePath) == originBaselineBytes && bytes(originEvidencePath) == originEvidenceBytes,
          "authorization and preparation preserve original contract, baseline, report, and evidence bytes");
    check(hasCode(WorkerTaskServices::prepareReauthorized(reauthModel,
          reauthApproval.value("authorizationId").toString(), originContract, originReport), "REAUTHORIZATION_INVALID"),
          "consumed approval cannot be replayed for preparation");
    const auto refreshed = GenerationServices().generate(reauthModel, reauthModel.generationOptions());
    check(refreshed.success, "governance fixture canonical Generate succeeds: " + refreshed.error);
    const auto refreshedContext = ContextCoordinationService::generate(reauthModel);
    check(refreshedContext.value("success").toBool(), "governance fixture context refresh succeeds");
    const auto refreshedVerification = VerificationServices().verify(reauthModel, reauthModel.generationOptions());
    check(refreshedVerification.overallStatus == VerificationStatus::Pass, "governance fixture Verify succeeds");
    const auto acceptedReauthPostflight = WorkerTaskServices::postflight(reauthModel, reauthContract);
    if (acceptedReauthPostflight.value("status") != "PASS")
        std::cerr << "REAUTH POSTFLIGHT: " << QJsonDocument(acceptedReauthPostflight.value("errors").toArray()).toJson().toStdString() << '\n';
    check(acceptedReauthPostflight.value("status") == "PASS",
          "unique audit-bound reauthorization is accepted by postflight");
    const auto stableReauthTree = treeBytes(reauthFixture.path());
    check(WorkerTaskServices::postflight(reauthModel, reauthContract) == acceptedReauthPostflight
          && WorkerTaskServices::postflight(reauthModel, reauthContract) == acceptedReauthPostflight
          && treeBytes(reauthFixture.path()) == stableReauthTree,
          "repeated valid postflight is read-only and never consumes the bound approval again");

    const auto producerPass = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    if (producerPass.value("status") != "PASS")
        std::cerr << "PRODUCER: " << QJsonDocument(producerPass).toJson().toStdString() << '\n';
    check(producerPass.value("status") == "PASS", "canonical producer executes real git whitespace checks");
    const auto producedEntries = producerPass.value("evidence").toArray();
    const auto producedEntry = producedEntries.isEmpty() ? QJsonObject{} : producedEntries.first().toObject();
    const QString producedPath = reauthFixture.path() + '/' + producedEntry.value("artifact").toString();
    const QByteArray producedBytes = bytes(producedPath);
    check(!producedBytes.isEmpty() && producerPass.value("record").toObject().value("observations").toArray().size() == 2
          && producerPass.value("record").toObject().value("dependencyFingerprint")
              == acceptedReauthPostflight.value("evidenceFingerprints").toObject().value("git-diff-check"),
          "producer persists command observations and internally derived dependency binding");
    check(WorkerTaskServices::postflight(reauthModel, reauthContract, producedEntries).value("acceptedEvidence").toArray() == producedEntries,
          "read-back producer evidence satisfies only its actual check");
    const auto producerTree = treeBytes(reauthFixture.path());
    check(WorkerTaskServices::runCheck(reauthModel, reauthContract, "acceptance-criteria").value("status") == "BLOCKED"
          && WorkerTaskServices::runCheck(reauthModel, reauthContract, "arbitrary-exit-zero").value("status") == "BLOCKED"
          && WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check", {}, 0).value("status") == "BLOCKED"
          && treeBytes(reauthFixture.path()) == producerTree,
          "unsupported semantic acceptance, arbitrary checks and invalid timeout cannot create PASS evidence");
    auto tamperedProducerContract = reauthContract;
    tamperedProducerContract.insert("completionState", "forged");
    check(WorkerTaskServices::runCheck(reauthModel, tamperedProducerContract, "git-diff-check").value("status") == "BLOCKED"
          && treeBytes(reauthFixture.path()) == producerTree, "tampered contract cannot start evidence execution");
    const auto producerAgain = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    const auto collected = WorkerTaskServices::checkEvidence(reauthModel, reauthContract);
    check(producerAgain.value("status") == "PASS" && producerAgain.value("index") != producerPass.value("index")
          && bytes(producedPath) == producedBytes && collected.value("status") == "PASS"
          && collected.value("evidence") == producerAgain.value("evidence"),
          "latest immutable observation is selected while previous exact bytes remain intact");
    check(write(producedPath, producedBytes + " "), "alter isolated producer record for integrity negative");
    check(WorkerTaskServices::checkEvidence(reauthModel, reauthContract).value("status") == "BLOCKED"
          && hasCode(WorkerTaskServices::postflight(reauthModel, reauthContract, producedEntries), "EVIDENCE_INTEGRITY_INVALID"),
          "tampered artifact is rejected by collector and postflight independently");
    check(write(producedPath, producedBytes), "restore isolated producer fixture bytes");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui \n"), "introduce isolated actual whitespace failure");
    check(ContextCoordinationService::generate(reauthModel).value("success").toBool()
          && VerificationServices().verify(reauthModel, reauthModel.generationOptions()).overallStatus == VerificationStatus::Pass,
          "refresh fixture governance independently of whitespace validation");
    check(hasCode(WorkerTaskServices::postflight(reauthModel, reauthContract, producedEntries), "STALE_EVIDENCE"),
          "changing covered source stales previously real producer evidence");
    const auto producerFailure = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    check(producerFailure.value("status") == "FAIL" && !producerFailure.value("evidence").toArray().isEmpty(),
          "actual command failure is persisted as FAIL with evidence rather than discarded or promoted");
    const auto failureRecord = producerFailure.value("record").toObject();
    check(failureRecord.value("observations").toArray().first().toObject().value("exitCode").toInt() != 0
          && !failureRecord.value("observations").toArray().first().toObject().value("stdoutBase64").toString().isEmpty(),
          "failed command retains real nonzero exit and raw output bytes");
    check(write(reauthFixture.path() + "/source/ui.cpp", "after ui\n"), "correct isolated whitespace regression");
    check(ContextCoordinationService::generate(reauthModel).value("success").toBool()
          && VerificationServices().verify(reauthModel, reauthModel.generationOptions()).overallStatus == VerificationStatus::Pass,
          "refresh fixture after actual correction");
    const auto correctedProducer = WorkerTaskServices::runCheck(reauthModel, reauthContract, "git-diff-check");
    check(correctedProducer.value("status") == "PASS" && bytes(producedPath) == producedBytes
          && WorkerTaskServices::checkEvidence(reauthModel, reauthContract).value("evidence") == correctedProducer.value("evidence"),
          "corrected check has a new observation while original pass and failure artifacts remain retained");
    const QString coldPath = reauthFixture.path() + "/ARAMF_WORKER/memory/cold-start-validation.json";
    auto historicalColdReport = QJsonDocument::fromJson(bytes(coldPath)).object();
    historicalColdReport.insert("checkedAt", "2000-01-01T00:00:00Z");
    check(write(coldPath, QJsonDocument(historicalColdReport).toJson()), "retain old PASS cold-start timestamp in isolated fixture");
    const auto liveColdCheck = WorkerTaskServices::runCheck(reauthModel, reauthContract, "cold-start");
    const auto liveColdObservations = liveColdCheck.value("record").toObject().value("observations").toArray();
    check(liveColdCheck.value("status") == "PASS" && !liveColdObservations.isEmpty()
          && liveColdObservations.first().toObject().value("report").toObject().value("checkedAt") != "2000-01-01T00:00:00Z",
          "cold-start producer reconstructs current state rather than republishing historical PASS");
    const QString disabledBuild = reauthFixture.path() + "/build/disabled";
    check(write(disabledBuild + "/CMakeCache.txt", ("CMAKE_HOME_DIRECTORY:INTERNAL=" + reauthFixture.path()
          + "\nBUILD_TESTING:BOOL=OFF\n").toUtf8()) && write(disabledBuild + "/aramf_core_tests.exe", "stale executable sentinel"),
          "create isolated disabled test configuration with leftover executable");
    const auto beforeDisabled = treeBytes(reauthFixture.path());
    const auto disabledEvidence = WorkerTaskServices::runCheck(reauthModel, reauthContract, "ctest", "build/disabled");
    check(disabledEvidence.value("status") == "BLOCKED" && disabledEvidence.value("error").toString().contains("enable tests")
          && treeBytes(reauthFixture.path()) == beforeDisabled,
          "disabled test configuration cannot run or attest leftover test binaries");
    const QString orphanDirectory = QFileInfo(producedPath).absolutePath() + "/../orphan-failure";
    check(write(orphanDirectory + "/record.json", QJsonDocument(failureRecord).toJson()), "retain isolated unindexed failure observation");
    check(WorkerTaskServices::checkEvidence(reauthModel, reauthContract).value("status") == "BLOCKED",
          "collector cannot skip an orphan failure to select an older PASS");
    // This negative fixture remains retained. Subsequent audit tests do not
    // collect its evidence and therefore cannot mistake it for a valid index.

    // Every case gets new append-only fixture events and a unique authorization.
    // Start from a complete valid audit chain, changing exactly the field under
    // test. No original contract, baseline, report or audit event is rewritten.
    using Mutation = std::function<void(QJsonObject&)>;
    const Mutation unchanged = [](QJsonObject&) {};
    struct AuditChain { QJsonObject contract, grant, approval, validation, bound; QString id; };
    int chainNumber = 0;
    const auto appendFixtureEvent = [&](const QString& type, QJsonObject fields) {
        for (const auto& key : {"eventId", "sequenceNumber", "timestamp", "eventType", "task"}) fields.remove(key);
        QString error;
        check(ProjectMemory().appendEvent(reauthFixture.path(), type, "isolated reauthorization negative fixture", fields, &error),
              "append isolated " + type + " fixture: " + error);
        return ProjectMemory().events(reauthFixture.path()).last();
    };
    QJsonObject approvalTemplate, validationTemplate, boundTemplate;
    for (const auto& event : approvedEventList) {
        if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == reauthApproval.value("authorizationId"))
            approvalTemplate = event;
        if (event.value("authorizationId") != reauthApproval.value("authorizationId")) continue;
        if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED") validationTemplate = event;
        if (event.value("eventType") == "TASK_REAUTHORIZATION_BOUND") boundTemplate = event;
    }
    const auto auditChain = [&](const Mutation& mutateGrant, const Mutation& mutateApproval,
                               const Mutation& mutateValidation, const Mutation& mutateContract,
                               const Mutation& mutateBinding, bool bind = true, bool approve = true) {
        AuditChain chain;
        chain.id = "independent-reauthorization-" + QString::number(++chainNumber);
        chain.grant = persistedGrant;
        chain.grant.insert("authorizationId", chain.id);
        mutateGrant(chain.grant);
        chain.approval = approvalTemplate;
        chain.approval.insert("taskReauthorizationGrant", chain.grant);
        mutateApproval(chain.approval);
        if (approve) chain.approval = appendFixtureEvent("ADMIN_OVERRIDE", chain.approval);
        else chain.approval.insert("eventId", "missing-approval-event");
        chain.validation = validationTemplate;
        chain.validation.insert("authorizationId", chain.id);
        chain.validation.insert("authorityEventId", chain.approval.value("eventId"));
        mutateValidation(chain.validation);
        chain.validation = appendFixtureEvent("TASK_REAUTHORIZATION_AUTHORIZED", chain.validation);
        chain.contract = reauthContract;
        auto link = chain.contract.value("reauthorization").toObject();
        link.insert("authorizationId", chain.id);
        link.insert("authorityEventId", chain.approval.value("eventId"));
        chain.contract.insert("reauthorization", link);
        auto request = chain.contract.value("request").toObject();
        request.insert("reauthorizationId", chain.id);
        chain.contract.insert("request", request);
        mutateContract(chain.contract);
        checksumFixtureContract(chain.contract);
        chain.bound = boundTemplate;
        chain.bound.insert("authorizationId", chain.id);
        chain.bound.insert("authorityEventId", chain.approval.value("eventId"));
        chain.bound.insert("contractId", chain.contract.value("contractId"));
        mutateBinding(chain.bound);
        if (bind) chain.bound = appendFixtureEvent("TASK_REAUTHORIZATION_BOUND", chain.bound);
        return chain;
    };
    const auto validChain = [&] { return auditChain(unchanged, unchanged, unchanged, unchanged, unchanged); };
    const auto auditPostflight = [&](const QJsonObject& contract) {
        // Audit appends legitimately change current-state context. Refresh its
        // canonical producers so stale derived state cannot mask this case.
        check(ContextCoordinationService::generate(reauthModel).value("success").toBool(),
              "refresh isolated audit fixture context");
        check(VerificationServices().verify(reauthModel, reauthModel.generationOptions()).overallStatus
                  == VerificationStatus::Pass, "verify isolated audit fixture canonical state");
        return WorkerTaskServices::postflight(reauthModel, contract);
    };
    const auto rejectedChain = [&](const AuditChain& chain, const QString& code, const QString& name) {
        const auto report = auditPostflight(chain.contract);
        check(report.value("status") == "FAIL" && hasCode(report, code)
              && !hasCode(report, "TASK_CONTRACT_TAMPERED"), name);
    };
    const auto controlChain = validChain();
    const auto controlReport = auditPostflight(controlChain.contract);
    if (controlReport.value("status") != "PASS")
        std::cerr << "CONTROL POSTFLIGHT: " << QJsonDocument(controlReport.value("errors").toArray()).toJson().toStdString() << '\n';
    check(controlReport.value("status") == "PASS",
          "independent audit-chain fixture control passes postflight");

    const auto unusedRevokedChain = auditChain(unchanged, unchanged, unchanged, unchanged, unchanged, false);
    appendFixtureEvent("TASK_REAUTHORIZATION_REVOKED", QJsonObject{{"authorizationId", unusedRevokedChain.id}});
    const auto revokedPreparation = WorkerTaskServices::prepareReauthorized(reauthModel,
        unusedRevokedChain.id, originContract, originReport);
    check(hasCode(revokedPreparation, "REAUTHORIZATION_INVALID")
          && revokedPreparation.value("preflight").toObject().value("errors").toArray().first().toObject()
              .value("message").toString().contains("revoked"), "revoked unused grant is independently rejected before preparation");

    const auto unboundChain = auditChain(unchanged, unchanged, unchanged, unchanged, unchanged, false);
    rejectedChain(unboundChain, "REAUTHORIZATION_AUDIT_INVALID", "missing unique binding is independently rejected");
    const auto unapprovedChain = auditChain(unchanged, unchanged, unchanged, unchanged, unchanged, true, false);
    rejectedChain(unapprovedChain, "REAUTHORIZATION_AUDIT_INVALID", "missing approval for an unknown grant is independently rejected");
    const auto expiredChain = auditChain([](QJsonObject& grant) {
        grant.insert("issuedAt", "2000-01-01T00:00:00Z");
        grant.insert("expiresAt", "2000-01-01T01:00:00Z");
    }, unchanged, unchanged, unchanged, unchanged);
    rejectedChain(expiredChain, "REAUTHORIZATION_AUDIT_INVALID", "complete expired audit chain is independently rejected");
    for (const auto& field : {"originContractId", "originContractFingerprint", "originPostflightFingerprint", "authorityEventId"}) {
        const auto chain = auditChain(unchanged, unchanged, [&](QJsonObject& validation) {
            validation.insert(field, "wrong-original-record");
        }, unchanged, unchanged);
        rejectedChain(chain, "REAUTHORIZATION_AUDIT_INVALID", "authorization event independently rejects wrong " + QString(field));
    }
    const auto identityChain = auditChain(unchanged, [](QJsonObject& approval) {
        approval.insert("administrator", "Different Administrator");
    }, unchanged, unchanged, unchanged);
    rejectedChain(identityChain, "REAUTHORIZATION_AUDIT_INVALID", "wrong administrator is independently rejected");
    const auto projectChain = auditChain([](QJsonObject& grant) {
        grant.insert("projectId", "different-project");
    }, unchanged, unchanged, unchanged, unchanged);
    rejectedChain(projectChain, "REAUTHORIZATION_AUDIT_INVALID", "grant for a different project is independently rejected");
    for (const auto& field : {"originContractId", "originContractFingerprint", "originPostflightFingerprint", "contractId"}) {
        const auto chain = auditChain(unchanged, unchanged, unchanged, unchanged, [&](QJsonObject& binding) {
            binding.insert(field, "wrong-bound-record");
        });
        rejectedChain(chain, "REAUTHORIZATION_AUDIT_INVALID", "binding event independently rejects wrong " + QString(field));
    }
    ProjectModel wrongProjectModel;
    wrongProjectModel.setProjectPath(reauthFixture.path());
    wrongProjectModel.setProjectId("different-project");
    check(hasCode(WorkerTaskServices::postflight(wrongProjectModel, reauthContract), "REAUTHORIZATION_AUDIT_INVALID"),
          "wrong project is rejected at successor postflight");
    const auto originChain = auditChain(unchanged, unchanged, unchanged, [](QJsonObject& contract) {
        auto link = contract.value("reauthorization").toObject();
        link.insert("originContractId", "different-origin");
        contract.insert("reauthorization", link);
    }, unchanged);
    rejectedChain(originChain, "REAUTHORIZATION_AUDIT_INVALID", "wrong origin contract link is independently rejected");
    auto wrongSuccessor = reauthContract;
    wrongSuccessor.insert("completionState", "TAMPERED");
    check(hasCode(WorkerTaskServices::postflight(reauthModel, wrongSuccessor), "TASK_CONTRACT_TAMPERED"),
          "wrong successor contract is rejected");
    for (const auto& field : {"sourceFiles", "canonicalInputs", "operations", "baseline"}) {
        const auto chain = auditChain(unchanged, unchanged, unchanged, [&](QJsonObject& contract) {
            auto link = contract.value("reauthorization").toObject();
            if (QString(field) == "sourceFiles") link.insert(field, QJsonArray{"source/unapproved.cpp"});
            else if (QString(field) == "canonicalInputs") link.insert(field, QJsonArray{"ARAMF_WORKER/custom/unapproved.json"});
            else if (QString(field) == "operations") {
                auto operations = link.value(field).toObject(); operations.insert("source/ui.cpp", "DELETE");
                link.insert(field, operations);
            } else link.insert(field, QJsonObject{});
            contract.insert("reauthorization", link);
        }, unchanged);
        rejectedChain(chain, "REAUTHORIZATION_SCOPE_INVALID", "independent exact-scope rejection for " + QString(field));
    }
    const auto revokedChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_REVOKED", QJsonObject{{"authorizationId", revokedChain.id},
        {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}}}});
    rejectedChain(revokedChain, "REAUTHORIZATION_AUDIT_INVALID", "revocation independently rejects a previously valid bound grant");
    const auto duplicateGrantChain = validChain();
    appendFixtureEvent("ADMIN_OVERRIDE", duplicateGrantChain.approval);
    rejectedChain(duplicateGrantChain, "REAUTHORIZATION_AUDIT_INVALID", "duplicate approval independently rejects its chain");
    const auto duplicateBindingChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_BOUND", duplicateBindingChain.bound);
    rejectedChain(duplicateBindingChain, "REAUTHORIZATION_AUDIT_INVALID", "duplicate binding independently rejects its chain");
    const auto duplicateValidationChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_AUTHORIZED", duplicateValidationChain.validation);
    rejectedChain(duplicateValidationChain, "REAUTHORIZATION_AUDIT_INVALID", "duplicate authorization independently rejects its chain");
    const auto consumedChain = validChain();
    appendFixtureEvent("TASK_REAUTHORIZATION_CONSUMED", consumedChain.bound);
    rejectedChain(consumedChain, "REAUTHORIZATION_AUDIT_INVALID", "extra consumption independently rejects a bound chain");
    check(auditPostflight(controlChain.contract).value("status") == "PASS",
          "unrelated negative chains do not revoke or contaminate the valid control");
    QString configError;
    const QString commandConfig = reauthFixture.path() + "/reauthorization-test.aramf.json";
    check(ProjectPersistence().save(reauthModel, commandConfig, &configError), "save reauthorization command fixture");
    for (const auto& forbidden : QStringList{"--command", "--status", "--dependency-fingerprint", "--baseline"}) {
        QString commandOutput, commandError;
        QTextStream outputStream(&commandOutput), errorStream(&commandError);
        check(runWorkerTaskCommand({"task", "run-check", "--config", commandConfig, "--contract", originContractPath,
              "--check", "git-diff-check", forbidden, "caller-authority"}, outputStream, errorStream) == 2
              && commandError.contains("caller commands"), "producer rejects caller evidence authority option " + forbidden);
    }
    const QString requestFile = reauthFixture.path() + "/reauthorization-request.json";
    for (const auto& forbidden : QStringList{"baseline", "sourceFingerprint", "authorizationId", "grantId", "contractId"}) {
        QJsonObject requestJson = reauthRequest.toJson(); requestJson.insert(forbidden, QJsonObject{});
        write(requestFile, QJsonDocument(requestJson).toJson());
        QString commandOutput, commandError;
        QTextStream outputStream(&commandOutput), errorStream(&commandError);
        check(runWorkerTaskCommand({"task", "reauthorize-approve", "--config", commandConfig,
              "--request", requestFile, "--instruction", reauthInstruction}, outputStream, errorStream) == 2,
              "client supplied " + forbidden + " is rejected");
    }
    QJsonObject badPostflight = originReport; badPostflight.insert("completionState", "VERIFIED");
    check(WorkerTaskServices::authorizeReauthorization(reauthModel, originContract, badPostflight,
          reauthRequest, reauthInstruction).value("status") == "BLOCKED", "mismatched origin report is rejected");
    const auto operationAssertions = canonicalCertificationOperationAssertions();
    for (auto it = operationAssertions.begin(); it != operationAssertions.end(); ++it) check(it.value().toBool(), it.key());
    check(requiredAssertions.size() == acceptanceAssertions.size(), "acceptance profile has observations for every exact named assertion");
    for (const auto& id : acceptanceProfiles)
        std::cout << "WORKER-TASK-ACCEPTANCE " << QJsonDocument(QJsonObject{{"profile", id},
            {"assertions", acceptanceAssertions}}).toJson(QJsonDocument::Compact).toStdString() << '\n';
    std::cout << "WORKER-TASK checks=" << checks << " failed=" << failed << " matrix=" << matrix.size() << '\n';
    return failed == 0;
}
