#include "core/FoundationCertificationCampaign.h"
#include "core/FoundationServices.h"
#include "core/CertificationFreshness.h"
#include "core/CertificationService.h"
#include "core/CertificationRevalidation.h"
#include "core/ProjectMemory.h"
#include "core/ProjectPersistence.h"
#include "core/ProjectModel.h"
#include "core/AramfPaths.h"
#include "core/WorkerTaskServices.h"
#include "core/ContextCoordinationService.h"
#include "core/Services.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QProcess>
#include <QTextStream>
#include <QDateTime>
#include <QLockFile>
#include <QDirIterator>
#include <QUuid>
#include <QThread>
#include <algorithm>
#include <functional>

namespace {
bool require(bool pass, const QString& message)
{
    QTextStream(stdout) << (pass ? "PASS " : "FAIL ") << message << Qt::endl;
    return pass;
}
bool write(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QString digest(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QJsonValue canonicalFixtureValue(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject result; const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) result.insert(it.key(), canonicalFixtureValue(it.value()));
        return result;
    }
    if (value.isArray()) {
        QList<QByteArray> entries;
        for (const auto& item : value.toArray()) entries.append(QJsonDocument(QJsonArray{canonicalFixtureValue(item)}).toJson(QJsonDocument::Compact));
        std::sort(entries.begin(), entries.end()); QJsonArray result;
        for (const auto& entry : entries) result.append(QJsonDocument::fromJson(entry).array().first());
        return result;
    }
    return value;
}
QByteArray readBytes(const QString& path)
{
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QString git(const QString& root, const QStringList& args)
{
    QProcess process;
    process.setWorkingDirectory(root);
    process.start("git", args);
    if (!process.waitForFinished(30000) || process.exitCode() != 0) return {};
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}
// Deliberately synthetic fixture evidence: tests the verifier, never production
// certification. No fixture evidence leaves this isolated temporary project.
QJsonObject fixtureEvidence(const QString& root, const QString& subject, const QString& revision)
{
    const QString lifecycle = subject + ".1.1.0.0";
    const QString ns = "ARAMF_WORKER/certification/evidence/" + subject.toLower() + "/" + lifecycle + "/fixture";
#ifdef Q_OS_WIN
    const QString suffix = ".exe";
#else
    const QString suffix;
#endif
    const QString core = "aramf_core_tests" + suffix + "::";
    const QString domain = subject == "F2" ? "--f2-identity-trust" : subject == "F3" ? "--f3-scope-integrity" : "--f4-lifecycle-certification";
    const QMap<QString,QString> identities{
        {"domain", core + domain}, {"independence", "aramf_" + subject.toLower() + "_independence" + suffix + "::" + QDir(root).absolutePath()},
        {"certification", core + "--foundation-certification"}, {"provenance-scope", core + "--provenance-and-scope"},
        {"namespace", core + "--foundation-namespace"}, {"migration", core + "--process-migration"},
        {"integration", core + "--foundation-integration"}, {"full-ctest", "ctest::--test-dir " + QDir(root).absoluteFilePath("build") + " --output-on-failure"}};
    QJsonArray checks;
    for (auto it = identities.begin(); it != identities.end(); ++it) {
        const QByteArray bytes = "synthetic verifier fixture, not a real command result\n";
        const QString reference = ns + "/checks/" + it.key() + ".log";
        write(root + "/" + reference, bytes);
        checks.append(QJsonObject{{"name", it.key()}, {"commandIdentity", it.value()}, {"sourceRevision", revision},
            {"lifecycle", lifecycle}, {"namespace", ns}, {"exitCode", 0}, {"status", "PASS"},
            {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"reference", reference}, {"fingerprint", digest(bytes)}});
    }
    QString source, contract;
    CertificationSourceManifestProvider::fingerprint(root, subject, &source);
    CertificationContractManifestProvider::fingerprint(subject, root, &contract);
    return {{"schemaVersion", 1}, {"verificationLevel", "HOST_TEST"},
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"provenance", QJsonObject{{"actor", "tool"}, {"tool", "FoundationCertificationCampaign"}}},
        {"subject", subject}, {"lifecycle", lifecycle}, {"namespace", ns}, {"sourceRevision", revision},
        {"sourceFingerprint", source}, {"contractFingerprint", contract}, {"status", "PASS"}, {"checks", checks}};
}

bool relocatedBindingFixture()
{
    // Synthetic issuance in an isolated repository models a historical layout.
    // It is not production certification evidence and executes no live move.
    QTemporaryDir fixture; fixture.setAutoRemove(false);
    const QString root = fixture.path(), config = "ARAMF_WORKER.aramf.json";
    const QString oldPath = "src/legacy/IdentityTrustFoundation.cpp";
    const QString newPath = "src/foundations/F2/IdentityTrustFoundation.cpp";
    const QStringList subjects{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"};
    QSet<QString> files;
    for (const auto& subject : subjects) for (const auto& path : CertificationSourceManifestProvider::manifest(subject).files) files.insert(path);
    const QString repository = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    for (const auto& path : files) if (!write(root + '/' + path, readBytes(repository + '/' + path))) return false;
    QDir().mkpath(root + "/src/legacy");
    if (!QFile::rename(root + '/' + newPath, root + '/' + oldPath)) return false;
    git(root, {"init"}); git(root, {"add", "src", "tests", "CMakeLists.txt"});
    git(root, {"-c", "user.name=Historical Test", "-c", "user.email=history@example.invalid", "-c", "commit.gpgsign=false", "commit", "-m", "historical source layout"});
    const QString historicalRevision = git(root, {"rev-parse", "HEAD"});
    QString error; bool ok = require(historicalRevision.size() == 40, "historical fixture revision exists");
    ProjectModel model; model.setProjectPath(root);
    auto state = ProcessVersionState::currentCanonicalState();
    for (int i = 0; i < 4; ++i) {
        ok &= ProcessVersionLifecycle::startNextProcess(&state, &error);
        ok &= ProcessVersionLifecycle::certifyCurrentIteration(&state, &error);
        ok &= ProcessVersionLifecycle::completeActiveProcess(&state, &error);
    }
    auto project = ProjectPersistence().toJson(model); project.insert("processVersion", processVersionStateToJson(state));
    ok &= ProjectPersistence().fromJson(&model, project, &error) && ProjectPersistence().save(model, root + '/' + config, &error)
        && ProjectMemory().initialize(root, &model, &error) && FoundationCertificationService::synchronizeProjectJson(root, model, &error);
    if (!require(ok, "historical integration fixture initializes: " + error)) return false;
    QString f2Artifact;
    QJsonObject legacySourceSnapshot;
    for (const auto& subject : subjects) {
        const bool foundation = QStringList{"F2", "F3", "F4"}.contains(subject);
        const QString lifecycle = subject + ".1.1.0.0";
        QString fingerprint, contract;
        CertificationSourceManifestProvider::fingerprint(root, subject, &fingerprint);
        CertificationContractManifestProvider::fingerprint(subject, root, &contract);
        QJsonObject evidence = foundation ? fixtureEvidence(root, subject, historicalRevision)
            : QJsonObject{{"schemaVersion", 1}, {"status", "PASS"}, {"subject", subject}, {"lifecycle", subject + ".1.1.1.1"},
                {"sourceRevision", historicalRevision}, {"sourceFingerprint", fingerprint}, {"contractFingerprint", contract}};
        if (subject == "F2") {
            QJsonArray snapshotFiles; QByteArray material;
            for (const auto& currentPath : CertificationSourceManifestProvider::manifest(subject).files) {
                const QString path = currentPath == newPath ? oldPath : currentPath;
                const auto data = readBytes(root + '/' + path);
                snapshotFiles.append(QJsonObject{{"path", path}, {"bytesBase64", QString::fromLatin1(data.toBase64())}});
                material.append(path.toUtf8()); material.append('\0'); material.append(data); material.append('\0');
            }
            evidence.insert("sourceFingerprint", digest(material));
            legacySourceSnapshot = {{"sourceRevision", historicalRevision}, {"sourceFingerprint", digest(material)}, {"files", snapshotFiles}};
        } else if (foundation) evidence.insert("sourceSnapshot", FoundationCertificationCampaign::sourceSnapshot(root, subject, historicalRevision, &error));
        const auto bindings = CertificationDependencyBindingProvider::currentBindings(subject, root, &error);
        for (auto it = bindings.begin(); it != bindings.end(); ++it) evidence.insert(it.key(), it.value());
        const QString artifact = foundation ? evidence.value("namespace").toString() + "/evidence.json"
            : "ARAMF_WORKER/certification/evidence/integration-fixture/" + subject + ".json";
        if (subject == "F2") f2Artifact = artifact;
        const auto bytes = QJsonDocument(evidence).toJson(); ok &= write(root + '/' + artifact, bytes);
        const QJsonObject context{{"sourceRevision", historicalRevision}, {"evidenceArtifact", artifact}, {"evidenceFingerprint", digest(bytes)}, {"lifecycle", lifecycle}};
        QJsonObject started, certificate; CertificationService service;
        ok &= service.start(root, subject, foundation ? "FOUNDATION" : "HOST_TEST", "project", "HOST_TEST", {"fixture"}, context, &started, &error);
        for (auto it = context.begin(); it != context.end(); ++it) started.insert(it.key(), it.value());
        ok &= service.issue(root, started, "PASS", {QJsonObject{{"reference", artifact}, {"fingerprint", digest(bytes)}, {"verified", true}, {"type", "HOST_TEST"}}}, &certificate, &error);
        if (!require(ok, subject + " synthetic historical certificate: " + error)) return false;
    }
    const auto ledger = readBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl");
    const auto originalEvidence = readBytes(root + '/' + f2Artifact);
    ok &= require(!FoundationCertificationCampaign::validateHistorical(root, "F2", {}, &error), "legacy historical validation rejects missing source witness");
    auto alteredSnapshot = legacySourceSnapshot;
    alteredSnapshot.insert("sourceRevision", QString(40, '0'));
    ok &= require(!FoundationCertificationCampaign::validateHistorical(root, "F2", alteredSnapshot, &error), "historical validation rejects incorrect witness revision");
    alteredSnapshot = legacySourceSnapshot;
    auto alteredFiles = alteredSnapshot.value("files").toArray();
    auto alteredFile = alteredFiles[0].toObject(); alteredFile.insert("bytesBase64", "dGFtcGVyZWQ="); alteredFiles[0] = alteredFile;
    alteredSnapshot.insert("files", alteredFiles);
    ok &= require(!FoundationCertificationCampaign::validateHistorical(root, "F2", alteredSnapshot, &error), "historical validation rejects replaced source content");
    ok &= require(FoundationCertificationCampaign::validateHistorical(root, "F2", legacySourceSnapshot, &error), "historical source paths validate in historical revision: " + error);
    ok &= require(QFile::rename(root + '/' + oldPath, root + '/' + newPath), "isolated byte-identical source relocation");
    git(root, {"add", "src"});
    git(root, {"-c", "user.name=Historical Test", "-c", "user.email=history@example.invalid", "-c", "commit.gpgsign=false", "commit", "-m", "isolated source relocation"});
    ok &= require(!git(root, {"ls-tree", "-r", "--name-only", historicalRevision}).split('\n').contains(newPath), "new path did not exist in historical revision");
    ok &= require(CertificationFreshnessService::evaluate("F2", root).status == CertificationFreshnessStatus::SourceStale, "relocation legitimately stales F2");
    QJsonObject integration;
    ok &= require(!FoundationCertificationCampaign::acceptIntegration(root, config, &integration, &error), "integration rejects stale relocated source");
    CertificationRevalidationResult revalidated;
    ok &= require(!CertificationRevalidationService::revalidate(root, "F2", {{"status", "PASS"}}, &revalidated, &error), "legacy revalidation rejects absent continuity evidence");
    ok &= require(!CertificationRevalidationService::validateCurrentSource(root, "F2", historicalRevision, &error), "current binding rejects obsolete implementation revision");
    ok &= require(CertificationRevalidationService::revalidate(root, "F2", {{"status", "PASS"}, {"fixture", "synthetic relocation verifier"},
        {"historicalSourceSnapshot", legacySourceSnapshot}}, &revalidated, &error), "current source revalidation after relocation: " + error);
    ok &= require(FoundationCertificationCampaign::validateHistorical(root, "F2", legacySourceSnapshot, &error), "historical certificate still validates without current old path");
    ok &= require(FoundationCertificationCampaign::acceptIntegration(root, config, &integration, &error), "integration accepts historical/current binding chain: " + error);
    ok &= require(FoundationCertificationCampaign::eligible(root, config, &error), "P6 eligibility uses current revalidated binding: " + error);
    const QString acceptancePath = root + '/' + integration.value("artifact").toString();
    const auto acceptanceBytes = readBytes(acceptancePath);
    const auto acceptance = QJsonDocument::fromJson(acceptanceBytes).object();
    const QJsonObject identity{{"projectId", model.projectId()}, {"projectRoot", QFileInfo(root).canonicalFilePath()}, {"workerIdentity", "ARAMF_WORKER"}};
    ok &= require(acceptance.value("projectBinding") == identity, "integration acceptance binds actual project identity");
    for (const auto& key : QStringList{"projectId", "projectRoot", "workerIdentity"}) {
        auto foreign = acceptance; auto binding = foreign.value("projectBinding").toObject(); binding.insert(key, "foreign"); foreign.insert("projectBinding", binding);
        write(acceptancePath, QJsonDocument(foreign).toJson());
        ok &= require(!FoundationCertificationCampaign::eligible(root, config, &error), "integration rejects foreign " + key);
    }
    auto unbound = acceptance; unbound.remove("projectBinding"); write(acceptancePath, QJsonDocument(unbound).toJson());
    ok &= require(!FoundationCertificationCampaign::eligible(root, config, &error), "integration rejects unbound legacy acceptance");
    write(acceptancePath, acceptanceBytes);
    const auto sourceBytes = readBytes(root + '/' + newPath);
    write(root + '/' + newPath, sourceBytes + "\n// unvalidated change\n");
    ok &= require(!FoundationCertificationCampaign::eligible(root, config, &error), "revalidated source tampering rejected");
    write(root + '/' + newPath, sourceBytes);
    write(root + '/' + f2Artifact, originalEvidence + " ");
    ok &= require(!FoundationCertificationCampaign::eligible(root, config, &error), "historical evidence tampering rejected after revalidation");
    write(root + '/' + f2Artifact, originalEvidence);
    const auto currentEvidence = readBytes(root + '/' + revalidated.evidenceArtifact);
    write(root + '/' + revalidated.evidenceArtifact, currentEvidence + " ");
    ok &= require(!FoundationCertificationCampaign::eligible(root, config, &error), "revalidation evidence tampering rejected");
    write(root + '/' + revalidated.evidenceArtifact, currentEvidence);
    ok &= require(FoundationCertificationCampaign::eligible(root, config, &error), "restored isolated fixture bindings validate");
    ok &= require(readBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl") == ledger
        && readBytes(root + '/' + f2Artifact) == originalEvidence, "historical certificate ledger and evidence remain byte-identical");
    return ok;
}
}

QJsonObject canonicalCertificationOperationAssertions()
{
    // Certificates and tiny-project assertion payloads are synthetic test
    // inputs. Command observations come from real isolated runCheck processes;
    // they never attest the repository's product regression suites.
    QJsonObject assertions;
    const auto expect = [&](bool pass, const QString& name) {
        assertions.insert(name, pass && assertions.value(name).toBool(true));
        if (!pass) QTextStream(stderr) << "OPERATION FIXTURE FAIL: " << name << Qt::endl;
        return pass;
    };
    QTemporaryDir fixture;
    fixture.setAutoRemove(false);
    const QString root = fixture.path(), config = root + "/ARAMF_WORKER.aramf.json";
    QTextStream(stdout) << "Operation fixture preserved: " << root << Qt::endl;
    QString error;
    const QStringList subjects{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"};
    QSet<QString> fileSet;
    for (const auto& subject : subjects)
        for (const auto& path : CertificationSourceManifestProvider::manifest(subject).files) fileSet.insert(path);
    QStringList files = fileSet.values(); std::sort(files.begin(), files.end());
    for (const auto& path : files)
        if (!write(root + '/' + path, readBytes(QDir(AramfPaths::programRoot()).filePath(path)))) return {{"operation fixture source copy", false}};
    QJsonObject syntheticAssertions;
    const auto fixtureProfile = WorkerTaskServices::acceptanceProfile("task-reauthorization-security/v1");
    for (const auto& criterion : fixtureProfile.value("criteria").toArray())
        for (const auto& assertion : criterion.toObject().value("assertions").toArray()) syntheticAssertions.insert(assertion.toString(), "PASS");
    const auto payload = QJsonDocument(QJsonObject{{"profile", fixtureProfile.value("id")}, {"assertions", syntheticAssertions},
        {"syntheticVerifierFixture", true}}).toJson(QJsonDocument::Compact);
    write(root + "/fixture-runner.cpp", QByteArray("#include <cstdio>\nint main(){puts(R\"fixture(WORKER-TASK-ACCEPTANCE ") + payload
        + ")fixture\");puts(\"Synthetic verifier fixture; completed=250 passed=250 failed=0; automatedCompleted=250 automatedPass=250 failures=0\");return 0;}\n");
    write(root + "/fixture-gui.cpp", "#include <QApplication>\n#include <QWidget>\nint main(int argc,char**argv){QApplication app(argc,argv);QWidget window;window.resize(300,200);window.show();return app.exec();}\n");
    write(root + "/CMakeLists.txt", "cmake_minimum_required(VERSION 3.21)\nproject(SyntheticOperationVerifier LANGUAGES CXX)\ninclude(CTest)\n"
        "find_package(Qt6 REQUIRED COMPONENTS Widgets)\nadd_executable(aramf fixture-gui.cpp)\ntarget_link_libraries(aramf PRIVATE Qt6::Widgets)\n"
        "foreach(name aramf_core_tests aramf_campaign_runner aramf_release_validation_runner)\nadd_executable(${name} fixture-runner.cpp)\nendforeach()\n"
        "foreach(name aramf_core_tests aramf_template_tests aramf_workflow_tests aramf_configuration_update)\nadd_test(NAME ${name} COMMAND aramf_core_tests)\nendforeach()\n");
    write(root + "/.gitignore", "build/\n");
    git(root, {"init", "-q"}); git(root, {"config", "core.autocrlf", "false"}); git(root, {"add", "."});
    git(root, {"-c", "user.name=Operation Test", "-c", "user.email=operation@example.invalid", "-c", "commit.gpgsign=false", "commit", "-q", "-m", "isolated operation sources"});
    const QString revision = git(root, {"rev-parse", "HEAD"});
    ProjectModel model; model.setProjectPath(root); model.setProjectId("isolated-certification-operation");
    auto memoryConfiguration = model.memoryConfiguration();
    memoryConfiguration.validationOptions = {"memory-consistency", "conflicting-decisions", "stale-current-state"};
    model.setMemoryConfiguration(memoryConfiguration);
    auto ai = model.aiConfiguration(); ai.permissions = {"read-project-files", "modify-files", "create-files", "run-builds", "run-tests", "run-project-tools"}; model.setAiConfiguration(ai);
    auto rules = model.ruleConfiguration(); rules.projectScopes = {"source-code", "tests"};
    for (const auto& scope : rules.projectScopes)
        rules.scopeMetadata.insert(scope, QJsonObject{{"files", QJsonArray::fromStringList(files)},
            {"tests", QJsonArray{"aramf_core_tests::--worker-tasks"}}, {"riskTraits", QJsonArray{"governance", "persistence"}},
            {"affects", QJsonArray{}}, {"generatedArtifacts", QJsonArray{}}});
    auto sourceMetadata = rules.scopeMetadata.value("source-code").toObject();
    sourceMetadata.insert("folderResourcePolicies", QJsonObject{{"operation-worker", QJsonObject{{"schemaVersion", 1},
        {"generatedPaths", QJsonArray{"context/context-index.json", "context/compressed-context.json", "context/freshness.json",
            "context/agent-adapters.json", "verification/latest-validation.json", "verification/verification-result.json",
            "verification/generation-state.json", "memory/cold-start-validation.json", "memory/memory-consistency-validation.json"}}}}});
    rules.scopeMetadata.insert("source-code", sourceMetadata);
    model.setRuleConfiguration(rules);
    ProjectResource workerResource; workerResource.id = "operation-worker"; workerResource.type = "folder";
    workerResource.location = root + "/ARAMF_WORKER"; workerResource.role = "source-of-truth";
    workerResource.scopes = {"source-code"}; model.setResources({workerResource});
    auto lifecycle = ProcessVersionState::currentCanonicalState();
    // CanonicalV2's initial state already retains completed P1-P5 history.
    for (int n = 0; n < 4; ++n) {
        if (!ProcessVersionLifecycle::startNextProcess(&lifecycle, &error)
            || !ProcessVersionLifecycle::certifyCurrentIteration(&lifecycle, &error)
            || !ProcessVersionLifecycle::completeActiveProcess(&lifecycle, &error)) return {{"operation fixture lifecycle: " + error, false}};
    }
    lifecycle.foundationIntegrationValid = true;
    auto project = ProjectPersistence().toJson(model); project.insert("processVersion", processVersionStateToJson(lifecycle));
    if (!ProjectPersistence().fromJson(&model, project, &error) || !ProjectPersistence().save(model, config, &error)
        || !ProjectPersistence().load(&model, config, &error) || !GenerationServices().generate(model, model.generationOptions()).success)
        return {{"operation fixture generation", false}};
    git(root, {"add", ".gitignore"});
    git(root, {"-c", "user.name=Operation Test", "-c", "user.email=operation@example.invalid", "-c", "commit.gpgsign=false", "commit", "-q", "-m", "canonical fixture ignore output"});
    QMap<QString, QString> historicalArtifacts;
    for (const auto& subject : subjects) {
        const bool foundation = QStringList{"F2", "F3", "F4"}.contains(subject);
        QString source, contract;
        CertificationSourceManifestProvider::fingerprint(root, subject, &source, &error);
        CertificationContractManifestProvider::fingerprint(subject, root, &contract, &error);
        auto evidence = foundation ? fixtureEvidence(root, subject, revision)
            : QJsonObject{{"schemaVersion", 1}, {"status", "PASS"}, {"subject", subject}, {"lifecycle", subject + ".1.1.1.1"},
                {"sourceRevision", revision}, {"sourceFingerprint", source}, {"contractFingerprint", contract}};
        if (foundation) evidence.insert("sourceSnapshot", FoundationCertificationCampaign::sourceSnapshot(root, subject, revision, &error));
        const auto bindings = CertificationDependencyBindingProvider::currentBindings(subject, root, &error);
        for (auto it = bindings.begin(); it != bindings.end(); ++it) evidence.insert(it.key(), it.value());
        const QString artifact = foundation ? evidence.value("namespace").toString() + "/evidence.json"
            : "ARAMF_WORKER/certification/evidence/operation-fixture/" + subject + ".json";
        historicalArtifacts.insert(subject, artifact);
        const auto bytes = QJsonDocument(evidence).toJson(); write(root + '/' + artifact, bytes);
        const QJsonObject context{{"sourceRevision", revision}, {"evidenceArtifact", artifact}, {"evidenceFingerprint", digest(bytes)}, {"lifecycle", subject + ".1.1.0.0"}};
        QJsonObject started, certificate; CertificationService service;
        if (!service.start(root, subject, foundation ? "FOUNDATION" : "HOST_TEST", "project", "HOST_TEST", {"fixture"}, context, &started, &error))
            return {{"operation fixture certification start: " + error, false}};
        for (auto it = context.begin(); it != context.end(); ++it) started.insert(it.key(), it.value());
        if (!service.issue(root, started, "PASS", {QJsonObject{{"reference", artifact}, {"fingerprint", digest(bytes)}, {"verified", true}, {"type", "HOST_TEST"}}}, &certificate, &error))
            return {{"operation fixture certification: " + error, false}};
    }
    const auto originalLedger = readBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl");
    QProcess configure; configure.setWorkingDirectory(root);
    configure.start("cmake", {"-S", root, "-B", root + "/build", "-G", "Ninja", "-DBUILD_TESTING=ON"});
    if (!configure.waitForFinished(120000) || configure.exitCode() != 0) {
        QTextStream(stderr) << configure.readAllStandardOutput() << configure.readAllStandardError();
        return {{"operation fixture real build configuration", false}};
    }
    const auto refresh = [&]() {
        VerificationServices().verify(model, model.generationOptions());
        const bool generated = ContextCoordinationService::generate(model).value("success").toBool();
        const auto verified = VerificationServices().verify(model, model.generationOptions());
        if (!generated || verified.overallStatus != VerificationStatus::Pass)
            QTextStream(stderr) << "Operation fixture refresh: " << QJsonDocument(verified.summary).toJson();
        return generated && verified.overallStatus == VerificationStatus::Pass;
    };
    const auto measuredSoftware = [&]() {
        refresh();
        WorkerTaskRequest request; request.goal = "Synthetic operation-verifier fixture"; request.type = "validation";
        request.scopes = {"source-code", "tests"}; request.files = {"tests/WorkerTaskTests.cpp"};
        request.acceptanceProfile = "task-reauthorization-security/v1";
        for (const auto& value : WorkerTaskServices::acceptanceProfile(request.acceptanceProfile).value("requirements").toArray()) request.definitionOfDone << value.toString();
        const auto contract = WorkerTaskServices::prepare(model, request);
        QSet<QString> covered;
        QStringList checks;
        for (const auto& item : contract.value("requiredEvidence").toArray())
            if (item != "diff-boundary" && item != "acceptance-criteria") checks.append(item.toString());
        checks.append("acceptance-criteria");
        for (const auto& check : checks) {
            if (covered.contains(check)) continue;
            const auto result = WorkerTaskServices::runCheck(model, contract, check, "build", 120000);
            if (result.value("status") != "PASS") {
                QTextStream(stderr) << "Fixture real producer failed " << check << ": " << QJsonDocument(result).toJson(); break;
            }
            for (const auto& entry : result.value("evidence").toArray()) covered.insert(entry.toObject().value("check").toString());
        }
        return contract;
    };
    const auto software = measuredSoftware();
    const auto priorMeasuredIndex = WorkerTaskServices::checkEvidence(model, software);
    const auto rebuilt = WorkerTaskServices::runCheck(model, software, "production-build", "build", 120000);
    const bool staleAcceptance = WorkerTaskServices::checkEvidence(model, software).value("status") == "BLOCKED";
    const auto replacementAcceptance = WorkerTaskServices::runCheck(model, software, "acceptance-criteria", "build", 120000);
    bool oldEvidenceRetained = true;
    for (const auto& value : priorMeasuredIndex.value("evidence").toArray()) {
        const auto entry = value.toObject(); oldEvidenceRetained &= digest(readBytes(root + '/' + entry.value("artifact").toString())) == entry.value("artifactFingerprint");
    }
    expect(rebuilt.value("status") == "PASS" && staleAcceptance && replacementAcceptance.value("status") == "PASS" && oldEvidenceRetained,
        "operation refreshes acceptance after measured prerequisite rerun without rewriting history");
    const auto measuredIndex = WorkerTaskServices::checkEvidence(model, software);
    if (!expect(measuredIndex.value("status") == "PASS", "operation real measured software control validates")) {
        QTextStream(stderr) << QJsonDocument(measuredIndex).toJson(); return assertions;
    }
    const auto rejectMeasurement = [&](const QString& check, const QString& name, const std::function<void(QJsonObject&)>& mutate) {
        QJsonObject entry;
        for (const auto& value : measuredIndex.value("evidence").toArray()) if (value.toObject().value("check") == check) entry = value.toObject();
        if (entry.isEmpty()) return expect(false, name);
        const QString artifact = root + '/' + entry.value("artifact").toString(), indexPath = QFileInfo(artifact).path() + "/index.json";
        const auto original = readBytes(artifact), originalIndex = readBytes(indexPath);
        auto record = QJsonDocument::fromJson(original).object(); mutate(record);
        const auto changed = QJsonDocument(record).toJson(); write(artifact, changed);
        entry.insert("artifactFingerprint", digest(changed));
        auto index = QJsonDocument::fromJson(originalIndex).object(); index.insert("evidence", QJsonArray{entry}); write(indexPath, QJsonDocument(index).toJson());
        const bool rejected = WorkerTaskServices::checkEvidence(model, software).value("status") == "BLOCKED";
        write(artifact, original); write(indexPath, originalIndex);
        return expect(rejected && WorkerTaskServices::checkEvidence(model, software).value("status") == "PASS", name);
    };
    rejectMeasurement("production-build", "operation rejects label-only producer PASS", [](QJsonObject& record) {
        record.insert("observations", QJsonArray{QJsonObject{{"syntheticVerifierFixture", true}, {"status", "PASS"}}});
    });
    rejectMeasurement("memory-consistency", "operation rejects omitted enabled memory validation", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto observed = observations.first().toObject(); auto report = observed.value("report").toObject();
        QJsonArray checks; for (const auto& checked : report.value("checks").toArray()) if (checked.toObject().value("name") != "conflicting-decisions") checks.append(checked);
        report.insert("checks", checks); observed.insert("report", report); observations[0] = observed; record.insert("observations", observations);
    });
    rejectMeasurement("cold-start", "operation rejects omitted mandatory cold-start path", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto observed = observations.first().toObject(); auto report = observed.value("report").toObject();
        QJsonArray checks; for (const auto& checked : report.value("checks").toArray()) if (checked.toObject().value("name") != AramfPaths::MemoryConfiguration) checks.append(checked);
        report.insert("checks", checks); observed.insert("report", report); observations[0] = observed; record.insert("observations", observations);
    });
    rejectMeasurement("memory-consistency", "operation rejects enabled memory validation marked inapplicable", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto observed = observations.first().toObject(); auto report = observed.value("report").toObject();
        QJsonArray checks; for (const auto& value : report.value("checks").toArray()) { auto checked = value.toObject(); if (checked.value("name") == "conflicting-decisions") checked.insert("status", "NOT_APPLICABLE"); checks.append(checked); }
        report.insert("checks", checks); observed.insert("report", report); observations[0] = observed; record.insert("observations", observations);
    });
    expect(!model.memoryConfiguration().validationOptions.contains("sequence-continuity")
        && WorkerTaskServices::checkEvidence(model, software).value("status") == "PASS", "operation accepts disabled optional memory validation");
    rejectMeasurement("production-build", "operation rejects failed command under PASS label", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto command = observations.first().toObject(); command.insert("exitCode", 9); observations[0] = command; record.insert("observations", observations);
    });
    rejectMeasurement("ctest", "operation rejects mismatched CTest execution count", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto command = observations.last().toObject();
        command.insert("stdoutBase64", QString::fromLatin1(QByteArray("100% tests passed, 0 tests failed out of 999").toBase64()));
        observations[observations.size()-1] = command; record.insert("observations", observations);
    });
    rejectMeasurement("aramf_core_tests::--worker-tasks", "operation rejects missing measured acceptance assertion", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto command = observations.last().toObject();
        command.insert("stdoutBase64", QString::fromLatin1(QByteArray("Synthetic fixture: no assertion report").toBase64()));
        observations[observations.size()-1] = command; record.insert("observations", observations);
    });
    rejectMeasurement("acceptance-criteria", "operation rejects missing acceptance prerequisite", [](QJsonObject& record) {
        auto observations = record.value("observations").toArray(); auto observed = observations.first().toObject(); observed.insert("evidence", QJsonArray{});
        observations[0] = observed; record.insert("observations", observations);
    });
    const QStringList selected{"F1", "P1", "P2", "P3", "P4", "P5"};
    QJsonObject semantics; for (const auto& subject : selected) semantics.insert(subject, false);
    const QJsonObject intent{{"kind", "revalidate-current/v1"}, {"subjects", QJsonArray::fromStringList(selected)},
        {"semanticChanges", semantics}, {"reason", "Synthetic coordinator-verifier fixture only"}};
    const auto tree = [&]() { QMap<QString,QByteArray> result; QDirIterator it(root, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) { const QString path = it.next(); result.insert(path, readBytes(path)); } return result; };
    const auto beforePrepare = tree();
    const auto plan = WorkerTaskServices::prepareCertificationOperation(model, intent, software);
    expect(tree() == beforePrepare, "operation preparation is read-only");
    const bool ready = plan.value("preflight").toObject().value("status").toString().startsWith("READY");
    if (!expect(ready, "operation valid software control prepares READY")) {
        QTextStream(stderr) << QJsonDocument(plan.value("preflight").toObject()).toJson(); return assertions;
    }
    bool exact = plan.value("permittedFiles").toArray().size() == 22;
    for (const auto& permission : plan.value("filePermissions").toArray()) {
        const auto value = permission.toObject(); exact &= value.value("owner") == "canonical-service"
            && value.value("action") != "MODIFY" && !value.value("path").toString().contains('*');
    }
    expect(exact, "operation paths are exact and service-owned");
    bool rejectsAuthority = true;
    for (const auto& key : QStringList{"outputs", "status", "sourceFingerprint", "baseline", "authority"}) {
        auto forged = intent; forged.insert(key, "caller");
        rejectsAuthority &= WorkerTaskServices::prepareCertificationOperation(model, forged, software).value("preflight").toObject().value("status") == "BLOCKED";
    }
    expect(rejectsAuthority, "operation rejects caller output authority");
    auto forged = plan; auto allowed = forged.value("permittedFiles").toArray(); allowed.append("CMakeLists.txt"); forged.insert("permittedFiles", allowed);
    forged.remove("contractId"); forged.insert("contractId", digest(QJsonDocument(canonicalFixtureValue(forged).toObject()).toJson(QJsonDocument::Compact)));
    expect(WorkerTaskServices::applyCertificationOperation(model, forged).value("status") == "BLOCKED" && tree() == beforePrepare,
        "operation rejects forged expanded permissions");
    const QString sourcePath = root + "/src/core/WorkerTaskServices.cpp"; const auto sourceBytes = readBytes(sourcePath);
    write(sourcePath, sourceBytes + "\n// synthetic stale source\n");
    expect(WorkerTaskServices::applyCertificationOperation(model, plan).value("status") == "BLOCKED", "operation rejects stale source before writes");
    write(sourcePath, sourceBytes);
    write(root + "/unrelated.txt", "unrelated fixture mutation");
    expect(WorkerTaskServices::applyCertificationOperation(model, plan).value("status") == "BLOCKED", "operation rejects changed baseline before writes");
    QFile::remove(root + "/unrelated.txt"); // Removes only this synthetic temporary fixture input.
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest(root.toUtf8()) + ".lock"); lock.setStaleLockTime(0);
    const bool locked = lock.tryLock(0);
    expect(locked && WorkerTaskServices::applyCertificationOperation(model, plan).value("status") == "BLOCKED", "operation lock prevents concurrent apply"); lock.unlock();
    const auto applied = WorkerTaskServices::applyCertificationOperation(model, plan);
    if (!expect(applied.value("status") == "PASS" && applied.value("postflight").toObject().value("completionState") == "VERIFIED", "operation revalidation completes verified")) {
        QTextStream(stderr) << QJsonDocument(applied).toJson(); return assertions;
    }
    const auto afterApply = tree();
    bool contextTamperingRejected = true;
    for (const auto& file : QStringList{"context-index.json", "compressed-context.json", "freshness.json", "agent-adapters.json"}) {
        const QString path = root + "/ARAMF_WORKER/context/" + file; const auto bytes = readBytes(path);
        auto tampered = QJsonDocument::fromJson(bytes).object(); tampered.insert("forged", true);
        if (file == "context-index.json") tampered.insert("entries", QJsonArray{});
        write(path, QJsonDocument(tampered).toJson());
        contextTamperingRejected &= WorkerTaskServices::postflight(model, plan).value("status") == "FAIL";
        write(path, bytes);
    }
    expect(contextTamperingRejected, "operation rejects each tampered canonical context output");
    expect(WorkerTaskServices::applyCertificationOperation(model, plan).value("status") == "BLOCKED" && tree() == afterApply,
        "operation replay preserves original artifacts");
    const QString receiptPath = root + '/' + applied.value("receipt").toString(); const auto receiptBytes = readBytes(receiptPath);
    write(receiptPath, receiptBytes + " ");
    // Whitespace is still tampering because the Worker manifest and receipt
    // are byte-bound, even though the parsed JSON is unchanged.
    expect(WorkerTaskServices::postflight(model, plan).value("status") == "FAIL", "operation postflight rejects changed receipt");
    write(receiptPath, receiptBytes);
    const QJsonObject integrationIntent{{"kind", "accept-current-integration/v1"}, {"subjects", QJsonArray{}},
        {"semanticChanges", QJsonObject{}}, {"reason", "Synthetic sequential integration verifier"}};
    const auto integrationPlan = WorkerTaskServices::prepareCertificationOperation(model, integrationIntent, plan);
    const auto integration = WorkerTaskServices::applyCertificationOperation(model, integrationPlan);
    if (!expect(integration.value("status") == "PASS" && integration.value("postflight").toObject().value("completionState") == "VERIFIED",
        "operation integration follows verified lineage")) { QTextStream(stderr) << QJsonDocument(integrationPlan.value("preflight").toObject()).toJson() << QJsonDocument(integration).toJson(); return assertions; }
    const auto beforeInspection = tree();
    QThread::msleep(1100);
    expect(WorkerTaskServices::postflight(model, integrationPlan).value("completionState") == "VERIFIED" && tree() == beforeInspection,
        "operation completed integration postflight is read-only");
    const QString acceptancePath = root + "/ARAMF_WORKER/certification/integration/" + integrationPlan.value("operationId").toString() + "/acceptance.json";
    const auto acceptanceBytes = readBytes(acceptancePath); auto acceptance = QJsonDocument::fromJson(acceptanceBytes).object();
    auto foreign = acceptance.value("projectBinding").toObject(); foreign.insert("projectId", "foreign"); acceptance.insert("projectBinding", foreign);
    write(acceptancePath, QJsonDocument(acceptance).toJson());
    expect(!FoundationCertificationCampaign::eligible(root, config, &error), "operation rejects foreign acceptance identity"); write(acceptancePath, acceptanceBytes);
    const auto savedProject = ProjectPersistence().toJson(model);
    auto activeState = model.processVersionState();
    bool progressValid = ProcessVersionLifecycle::startNextProcess(&activeState, &error);
    auto activeProject = savedProject; activeProject.insert("processVersion", processVersionStateToJson(activeState));
    progressValid &= ProjectPersistence().fromJson(&model, activeProject, &error) && ProjectPersistence().save(model, config, &error)
        && FoundationCertificationService::synchronizeProjectJson(root, model, &error) && FoundationCertificationCampaign::eligible(root, config, &error);
    expect(progressValid, "operation acceptance remains valid through legitimate P6 lifecycle start");
    ProjectPersistence().fromJson(&model, savedProject, &error); ProjectPersistence().save(model, config, &error);
    FoundationCertificationService::synchronizeProjectJson(root, model, &error);
    const QString corruptPath = root + '/' + historicalArtifacts.value("P1"); const auto original = readBytes(corruptPath);
    write(corruptPath, original + " ");
    const auto partialSoftware = measuredSoftware();
    auto partialIntent = intent; partialIntent.insert("subjects", QJsonArray{"F1", "P1"}); partialIntent.insert("semanticChanges", QJsonObject{{"F1", false}, {"P1", false}});
    const auto partialPlan = WorkerTaskServices::prepareCertificationOperation(model, partialIntent, partialSoftware);
    const auto partial = WorkerTaskServices::applyCertificationOperation(model, partialPlan);
    const auto failureReceipt = QJsonDocument::fromJson(readBytes(root + '/' + partial.value("receipt").toString())).object();
    if (!expect(partial.value("status") == "FAIL" && failureReceipt.value("outcome") == "FAILED"
        && !failureReceipt.value("results").toArray().isEmpty() && ContextCoordinationService::validate(model).value("valid").toBool(),
        "operation failure retains partial evidence"))
        QTextStream(stderr) << QJsonDocument(partialPlan.value("preflight").toObject()).toJson() << QJsonDocument(partial).toJson();
    write(corruptPath, original);
    expect(readBytes(root + "/ARAMF_WORKER/certification/certificates.jsonl") == originalLedger, "operation preserves original certificate history");
    const auto recoverySoftware = measuredSoftware();
    auto recoveryIntent = intent; recoveryIntent.insert("subjects", QJsonArray{"F1"}); recoveryIntent.insert("semanticChanges", QJsonObject{{"F1", false}});
    const auto recoveryPlan = WorkerTaskServices::prepareCertificationOperation(model, recoveryIntent, recoverySoftware);
    if (!expect(recoveryPlan.value("preflight").toObject().value("status").toString().startsWith("READY"), "operation recovery fixture prepares real plan")) return assertions;
    const QString operationId = recoveryPlan.value("operationId").toString();
    const QString interruptedId = "reval-" + QUuid::createUuidV5(QUuid(operationId), "revalidate-current/v1/F1").toString(QUuid::WithoutBraces);
    const QString interruptedPrefix = "ARAMF_WORKER/certification/revalidation/F1/" + interruptedId;
    const QString operationPrefix = "ARAMF_WORKER/certification/operations/" + operationId;
    // Deliberately synthesize an interrupted file pair to test recovery parsing;
    // the production service never accepts this as successful evidence.
    write(root + '/' + interruptedPrefix + "/evidence.json", "synthetic interrupted evidence bytes\n");
    QJsonObject latest;
    expect(!CertificationRevalidationService::latest(root, "F1", &latest, &error), "operation rejects unexplained missing immutable record");
    write(root + '/' + interruptedPrefix + "/revalidation.json", "{\"subject\":\"F1\"}");
    expect(!CertificationRevalidationService::latest(root, "F1", &latest, &error), "operation rejects structurally incomplete immutable record");
    write(root + '/' + interruptedPrefix + "/revalidation.json", "{ truncated synthetic record");
    expect(!CertificationRevalidationService::latest(root, "F1", &latest, &error), "operation rejects unexplained truncated immutable record");
    const auto proofBytes = QJsonDocument(QJsonObject{{"producer", "WorkerTaskServices/certification-operation/v1"}, {"operationContract", recoveryPlan}}).toJson();
    write(root + '/' + operationPrefix + "/software-evidence.json", proofBytes);
    const auto recovery = WorkerTaskServices::recoverCertificationOperation(model, recoveryPlan);
    expect(recovery.value("status") == "RECOVERED_FAILURE" && recovery.value("completionState") == "BLOCKED"
        && WorkerTaskServices::postflight(model, recoveryPlan).value("status") == "FAIL",
        "operation supported recovery seals interrupted outputs only as failed");
    const auto retainedRecovery = readBytes(root + '/' + operationPrefix + "/recovery.json");
    QFile::remove(root + '/' + operationPrefix + "/recovery-seal.json"); // Isolated fixture: emulate interruption before sealing.
    const auto resumedRecovery = WorkerTaskServices::recoverCertificationOperation(model, recoveryPlan);
    expect(resumedRecovery.value("status") == "RECOVERED_FAILURE"
        && readBytes(root + '/' + operationPrefix + "/recovery.json") == retainedRecovery,
        "operation resumes intact unsealed recovery without replacing retained evidence");
    const auto retained = tree();
    expect(CertificationRevalidationService::latest(root, "F1", &latest, &error) && latest.value("revalidationId") != interruptedId && tree() == retained,
        "operation classifies only sealed exact failed pair without discarding bytes");
    write(root + '/' + interruptedPrefix + "/revalidation.json", "{ changed after sealed failure");
    expect(!CertificationRevalidationService::latest(root, "F1", &latest, &error), "operation rejects altered retained failed pair");
    write(root + '/' + interruptedPrefix + "/revalidation.json", "{ truncated synthetic record");
    CertificationRevalidationResult recovered;
    expect(CertificationRevalidationService::revalidate(root, "F1", {{"status", "PASS"}, {"syntheticRecoveryFixture", true}}, &recovered, &error)
        && CertificationRevalidationService::latest(root, "F1", &latest, &error) && latest.value("revalidationId") == recovered.revalidationId
        && readBytes(root + '/' + interruptedPrefix + "/revalidation.json") == "{ truncated synthetic record",
        "operation new revalidation supersedes classified interrupted attempt");
    return assertions;
}

bool runFoundationCertificationTests()
{
    QTemporaryDir fixture;
    fixture.setAutoRemove(false);
    const QString root = fixture.path();
    const QString file = "ARAMF_WORKER.aramf.json";
    QString error;
    bool ok = true;
    QSet<QString> sources;
    for (const auto& subject : QStringList{"F2", "F3", "F4"})
        for (const auto& path : CertificationSourceManifestProvider::manifest(subject).files) sources.insert(path);
    for (const auto& path : sources) {
        QFile source(QDir(AramfPaths::programRoot()).filePath(path));
        if (!source.open(QIODevice::ReadOnly) || !write(root + "/" + path, source.readAll()))
            return require(false, "fixture source copy: " + path);
    }
    git(root, {"init"});
    git(root, {"add", "src", "tests", "CMakeLists.txt"});
    git(root, {"-c", "user.name=Foundation Test", "-c", "user.email=foundation-test@example.invalid",
               "-c", "commit.gpgsign=false", "commit", "-m", "Isolated verifier fixture"});
    const QString revision = git(root, {"rev-parse", "HEAD"});
    if (!require(revision.size() == 40, "fixture Git revision")) return false;
    ProjectModel model;
    model.setProjectPath(root);
    auto state = ProcessVersionState::currentCanonicalState();
    ProcessVersionLifecycle::startNextProcess(&state);
    ProcessVersionLifecycle::certifyCurrentIteration(&state);
    ProcessVersionLifecycle::completeActiveProcess(&state);
    auto project = ProjectPersistence().toJson(model);
    project.insert("processVersion", processVersionStateToJson(state));
    if (!ProjectPersistence().fromJson(&model, project, &error)
        || !ProjectPersistence().save(model, root + "/" + file, &error)
        || !ProjectMemory().initialize(root, &model, &error)
        || !FoundationCertificationService::synchronizeProjectJson(root, model, &error))
        return require(false, "fixture initialize: " + error);
    ok &= require(!FoundationCertificationCampaign::start(root, file, "F3", &error), "canonical order rejects F3 before F2");
    ok &= require(!FoundationCertificationCampaign::start(root, file, "P6", &error), "Foundation route rejects P6");
    for (const auto& subject : QStringList{"F2", "F3", "F4"}) {
        ok &= require(FoundationCertificationCampaign::start(root, file, subject, &error), subject + " starts: " + error);
        ok &= require(!FoundationCertificationCampaign::complete(root, file, subject, &error), subject + " cannot complete uncertified");
        auto evidence = fixtureEvidence(root, subject, revision);
        const QString artifact = evidence.value("namespace").toString() + "/evidence.json";
        auto persist = [&](const QJsonObject& value){return write(root + "/" + artifact, QJsonDocument(value).toJson());};
        auto valid = [&](){return FoundationCertificationCampaign::validateEvidence(root, subject, subject + ".1.1.0.0", revision, artifact, nullptr, &error);};
        persist(evidence);
        ok &= require(valid(), subject + " accepts correctly bound fixture evidence: " + error);
        for (const auto& field : QStringList{"subject","lifecycle","sourceRevision","sourceFingerprint","contractFingerprint","namespace","status","schemaVersion","verificationLevel","timestamp","provenance"}) {
            auto altered = evidence;
            altered.insert(field, "incorrect");
            persist(altered);
            ok &= require(!valid(), subject + " rejects altered " + field);
        }
        auto altered = evidence;
        auto checks = evidence.value("checks").toArray();
        checks[0] = checks[1];
        altered.insert("checks", checks);
        persist(altered);
        ok &= require(!valid(), subject + " rejects duplicate checks");
        checks = evidence.value("checks").toArray();
        checks.removeLast();
        altered.insert("checks", checks);
        persist(altered);
        ok &= require(!valid(), subject + " rejects missing check");
        for (const auto& field : QStringList{"commandIdentity","reference","fingerprint","status","sourceRevision","lifecycle","namespace","exitCode","timestamp"}) {
            altered = evidence;
            checks = evidence.value("checks").toArray();
            auto check = checks[0].toObject();
            check.insert(field, "incorrect");
            checks[0] = check;
            altered.insert("checks", checks);
            persist(altered);
            ok &= require(!valid(), subject + " rejects altered check " + field);
        }
        persist(evidence);
        const QString log = evidence.value("checks").toArray()[0].toObject().value("reference").toString();
        write(root + "/" + log, "tampered");
        ok &= require(!valid(), subject + " rejects altered physical evidence bytes");
        evidence = fixtureEvidence(root, subject, revision);
        persist(evidence);
        write(root + "/outside.json", QJsonDocument(evidence).toJson());
        ok &= require(!FoundationCertificationCampaign::validateEvidence(root, subject, subject + ".1.1.0.0", revision, "outside.json", nullptr, &error),
                      subject + " rejects evidence outside namespace");
        QJsonObject certificate;
        ok &= require(FoundationCertificationCampaign::certify(root, file, subject, revision, artifact, &certificate, &error),
                      subject + " synthetic fixture certification: " + error);
        ok &= require(!certificate.value("certificateId").toString().isEmpty(), subject + " durable certificate ID");
        write(root + "/" + log, "post-issuance corruption");
        ok &= require(!FoundationCertificationCampaign::complete(root, file, subject, &error), subject + " refuses completion after physical log corruption");
        // Restore only the isolated test log; the signed artifact is unchanged.
        write(root + "/" + log, "synthetic verifier fixture, not a real command result\n");
        ok &= require(FoundationCertificationCampaign::complete(root, file, subject, &error), subject + " completes: " + error);
        ProjectModel reloaded;
        ok &= require(ProjectPersistence().load(&reloaded, root + "/" + file, &error)
                      && !reloaded.processVersionState().hasActiveProcess, subject + " cold reload");
        // Freshness evaluation resolves Git from process cwd; evaluate against fixture.
        const QString previous = QDir::currentPath();
        QDir::setCurrent(root);
        ok &= require(CertificationFreshnessService::evaluate(subject, root).status == CertificationFreshnessStatus::Fresh,
                      subject + " persisted certificate is fresh");
        QDir::setCurrent(previous);
    }
    QJsonObject integration;
    ok &= require(!FoundationCertificationCampaign::acceptIntegration(root, file, &integration, &error),
                  "integration rejects missing F1/P1-P5 evidence despite completed lifecycle");
    ok &= require(!FoundationCertificationCampaign::eligible(root, file, &error), "P6 remains blocked in incomplete fixture");
    QTextStream(stdout) << "Fixture preserved: " << root << Qt::endl;
    return relocatedBindingFixture() && ok;
}
