#include "core/FoundationCertificationCampaign.h"
#include "core/FoundationServices.h"
#include "core/CertificationFreshness.h"
#include "core/CertificationService.h"
#include "core/CertificationRevalidation.h"
#include "core/ProjectMemory.h"
#include "core/ProjectPersistence.h"
#include "core/ProjectModel.h"
#include "core/AramfPaths.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QProcess>
#include <QTextStream>
#include <QDateTime>

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
