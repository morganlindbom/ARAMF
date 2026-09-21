#include "core/CertificationFreshness.h"
#include "core/CertificationRevalidation.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <iostream>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::cerr << "FAIL: " << message << '\n';
    return value;
}

QString hashBytes(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool runGit(const QString& root, const QStringList& arguments)
{
    QProcess process;
    process.setWorkingDirectory(root);
    process.start(QStringLiteral("git"), arguments);
    return process.waitForFinished(10000) && process.exitCode() == 0;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);
    if (app.arguments().contains(QStringLiteral("--phase-b1"))) {
        const QString projectRoot = QDir::currentPath();
        const QJsonObject regressionEvidence{
            {QStringLiteral("status"), QStringLiteral("PASS")},
            {QStringLiteral("campaign"), QStringLiteral("Phase B.1 exact committed baseline")},
            {QStringLiteral("implementationRevision"), QStringLiteral("60ee7d14ec7675b9a5ad9f1484f747bbdf4846fc")},
            {QStringLiteral("fullCTest"), QStringLiteral("13/13 PASS")},
            {QStringLiteral("updateCampaign"), QStringLiteral("PASS")},
            {QStringLiteral("configurationUpdate"), QStringLiteral("PASS")},
            {QStringLiteral("memoryColdStart"), QStringLiteral("PASS")}};
        bool chainOk = true;
        for (const auto& subject : QStringList{QStringLiteral("F1"), QStringLiteral("S1"), QStringLiteral("S2"),
                                               QStringLiteral("S3"), QStringLiteral("S4"), QStringLiteral("S5"), QStringLiteral("S6")}) {
            CertificationRevalidationResult revalidation;
            QString error;
            if (!CertificationRevalidationService::revalidate(projectRoot, subject, regressionEvidence, &revalidation, &error)) {
                std::cerr << "FAIL: " << subject.toStdString() << " revalidation: " << error.toStdString() << '\n';
                chainOk = false;
                break;
            }
            const auto evaluated = CertificationFreshnessService::evaluate(subject, projectRoot, &error);
            if (!error.isEmpty() || evaluated.status != CertificationFreshnessStatus::Fresh) {
                std::cerr << "FAIL: " << subject.toStdString() << " checkpoint: " << error.toStdString() << '\n';
                chainOk = false;
                break;
            }
            std::cout << subject.toStdString() << " FRESH " << revalidation.revalidationId.toStdString() << '\n';
        }
        return chainOk ? 0 : 1;
    }
    bool ok = true;
    QTemporaryDir fixture;
    ok &= check(fixture.isValid(), "temporary revalidation fixture created");
    if (!ok) return 1;

    const QString repositoryRoot = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    const QString projectRoot = QDir(fixture.path()).absolutePath();
    for (const auto& relative : CertificationSourceManifestProvider::manifest(QStringLiteral("F1")).files) {
        const QString source = QDir(repositoryRoot).filePath(relative);
        const QString destination = QDir(projectRoot).filePath(relative);
        QDir().mkpath(QFileInfo(destination).absolutePath());
        ok &= check(QFile::copy(source, destination), "fixture source-manifest file copied");
    }
    ok &= check(writeBytes(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json")),
                           QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 2}}).toJson(QJsonDocument::Compact) + '\n'),
                "fixture project configuration written");
    ok &= check(runGit(projectRoot, {QStringLiteral("init"), QStringLiteral("-q")}), "fixture git repository initialized");
    ok &= check(runGit(projectRoot, {QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("ARAMF Test")}), "fixture git identity configured");
    ok &= check(runGit(projectRoot, {QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("aramf-test@example.invalid")}), "fixture git email configured");
    ok &= check(runGit(projectRoot, {QStringLiteral("add"), QStringLiteral(".")}), "fixture files staged");
    ok &= check(runGit(projectRoot, {QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("fixture baseline")}), "fixture source baseline committed");

    QString sourceFingerprint;
    QString contractFingerprint;
    ok &= check(CertificationSourceManifestProvider::fingerprint(projectRoot, QStringLiteral("F1"), &sourceFingerprint),
                "fixture source fingerprint calculated");
    ok &= check(CertificationContractManifestProvider::fingerprint(QStringLiteral("F1"), projectRoot, &contractFingerprint),
                "fixture contract fingerprint calculated");
    const QByteArray historicalBytes = QJsonDocument(QJsonObject{
        {QStringLiteral("lifecycle"), QStringLiteral("F1.1.4.1.1")},
        {QStringLiteral("sourceRevision"), QStringLiteral("fixture")},
        {QStringLiteral("sourceFingerprint"), sourceFingerprint},
        {QStringLiteral("contractFingerprint"), contractFingerprint}}).toJson(QJsonDocument::Compact) + '\n';
    const QString historicalReference = QStringLiteral("ARAMF_WORKER/certification/evidence/historical.json");
    ok &= check(writeBytes(QDir(projectRoot).filePath(historicalReference), historicalBytes), "historical fixture evidence written");
    const QString historicalFingerprint = hashBytes(historicalBytes);
    const QJsonObject certificate{
        {QStringLiteral("certificateId"), QStringLiteral("cert-fixture-f1")},
        {QStringLiteral("subject"), QStringLiteral("F1")},
        {QStringLiteral("evidenceArtifact"), historicalReference},
        {QStringLiteral("evidenceFingerprint"), historicalFingerprint},
        {QStringLiteral("evidenceReferences"), QJsonArray{QJsonObject{{QStringLiteral("reference"), historicalReference}, {QStringLiteral("fingerprint"), historicalFingerprint}}}}};
    ok &= check(writeBytes(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl")),
                           QJsonDocument(certificate).toJson(QJsonDocument::Compact) + '\n'), "historical fixture certificate written");

    CertificationRevalidationResult revalidation;
    QString error;
    ok &= check(CertificationRevalidationService::revalidate(projectRoot, QStringLiteral("F1"),
            QJsonObject{{QStringLiteral("status"), QStringLiteral("PASS")}, {QStringLiteral("suite"), QStringLiteral("fixture")}},
            &revalidation, &error), "F1 freshness revalidation succeeds on isolated fixture");
    ok &= check(error.isEmpty(), "fixture revalidation has no error");
    ok &= check(revalidation.status == QStringLiteral("FRESH"), "fixture revalidation status is FRESH");
    ok &= check(revalidation.historicalLifecycle == QStringLiteral("F1.1.4.1.1"), "historical lifecycle is referenced, not changed");
    const auto evaluated = CertificationFreshnessService::evaluate(QStringLiteral("F1"), projectRoot, &error);
    ok &= check(evaluated.status == CertificationFreshnessStatus::Fresh, "revalidated fixture evaluates FRESH");
    ok &= check(evaluated.historicalLifecycle == QStringLiteral("F1.1.4.1.1"), "evaluator preserves completed lifecycle identity");
    QFile evidence(QDir(projectRoot).filePath(revalidation.evidenceArtifact));
    ok &= check(evidence.open(QIODevice::ReadOnly), "revalidation evidence can be read");
    ok &= check(hashBytes(evidence.readAll()) == revalidation.evidenceFingerprint, "revalidation evidence fingerprint matches exact bytes");
    return ok ? 0 : 1;
}
