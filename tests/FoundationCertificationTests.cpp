#include "core/FoundationCertificationCampaign.h"
#include "core/FoundationServices.h"
#include "core/CertificationFreshness.h"
#include "core/CertificationService.h"
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
    return ok;
}
