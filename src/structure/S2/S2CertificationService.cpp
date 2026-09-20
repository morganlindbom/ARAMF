#include "S2CertificationService.h"
#include "core/CertificationService.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace S2 {
namespace {
bool writeEvidence(const QString& root, const Configuration& configuration, const S1::Configuration& ownership, const QString& sourceRevision, const QJsonObject& testResult, int iteration, QString* relativePath, QString* fingerprint, QString* error) {
    const auto result = audit(configuration, ownership, root);
    if (!result.valid) { if (error) *error = QStringLiteral("S2 audit must pass before certification."); return false; }
    const QString relative = QStringLiteral("ARAMF_WORKER/certification/evidence/s2.1.%1/iteration-%1/s2-evidence.json").arg(iteration);
    const QString absolute = QDir(root).filePath(relative); QDir().mkpath(QFileInfo(absolute).absolutePath());
    const QJsonObject evidence{{"schemaVersion", 1}, {"authority", "S2"}, {"subject", "S2"}, {"lifecycle", QStringLiteral("S2.1.%1.0.0").arg(iteration)}, {"sourceRevision", sourceRevision}, {"sourceFingerprint", result.modelFingerprint}, {"generatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"testResult", testResult}, {"audit", result.toJson()}, {"s1ContractFingerprint", QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(S1::toJson(ownership)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex())}};
    const QByteArray bytes = QJsonDocument(evidence).toJson(QJsonDocument::Indented); const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()); QSaveFile file(absolute); if (!file.open(QIODevice::WriteOnly | QIODevice::Text) || file.write(bytes) != bytes.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    if (relativePath) *relativePath = relative; if (fingerprint) *fingerprint = hash; return true;
}
}

bool CertificationServiceAdapter::certify(const QString& projectRoot, const Configuration& configuration, const S1::Configuration& ownership, const QString& sourceRevision, const QJsonObject& testResult, int iteration, QJsonObject* issuedCertificate, QString* error) {
    QString evidencePath, evidenceFingerprint; if (!writeEvidence(projectRoot, configuration, ownership, sourceRevision, testResult, iteration, &evidencePath, &evidenceFingerprint, error)) return false;
    CertificationService service; QJsonObject started; if (!service.start(projectRoot, QStringLiteral("S2"), QStringLiteral("STRUCTURE"), QStringLiteral("project"), QStringLiteral("HOST_TEST"), QJsonArray{QStringLiteral("s2-evidence-artifact")}, QJsonObject{{"structureVersion", QStringLiteral("S2.1.%1").arg(iteration)}, {"sourceRevision", sourceRevision}, {"evidenceArtifact", evidencePath}, {"evidenceFingerprint", evidenceFingerprint}}, &started, error)) return false;
    return service.issue(projectRoot, started, QStringLiteral("PASS"), QJsonArray{QJsonObject{{"reference", evidencePath}, {"fingerprint", evidenceFingerprint}, {"type", "HOST_TEST"}, {"verified", true}}}, issuedCertificate, error);
}
}
