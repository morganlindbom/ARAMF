#include "S1CertificationService.h"
#include "core/CertificationService.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace S1 {
namespace {
bool writeEvidence(const QString& root, const Configuration& configuration, const QString& sourceRevision,
                  const QJsonObject& testResult, int iteration, QString* relativePath, QString* fingerprint, QString* error)
{
    const auto auditResult = audit(configuration);
    if (!auditResult.valid) { if (error) *error = QStringLiteral("S1 audit must pass before certification."); return false; }
    const QString relative = QStringLiteral("ARAMF_WORKER/certification/evidence/s1.1.%1/iteration-%1/s1-evidence.json").arg(iteration);
    const QString absolute = QDir(root).filePath(relative);
    QDir().mkpath(QFileInfo(absolute).absolutePath());
    const QJsonObject evidence{{"schemaVersion",1},{"authority","S1"},{"subject","S1"},{"lifecycle",QStringLiteral("S1.1.%1.0.0").arg(iteration)},{"sourceRevision",sourceRevision},{"sourceFingerprint",auditResult.modelFingerprint},{"generatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"testResult",testResult},{"audit",auditResult.toJson()}};
    const QByteArray bytes = QJsonDocument(evidence).toJson(QJsonDocument::Indented);
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    QSaveFile file(absolute); if (!file.open(QIODevice::WriteOnly|QIODevice::Text) || file.write(bytes) != bytes.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    if (relativePath) *relativePath = relative; if (fingerprint) *fingerprint = hash; return true;
}
}

bool CertificationServiceAdapter::certify(const QString& projectRoot, const Configuration& configuration,
                                          const QString& sourceRevision, const QJsonObject& testResult,
                                          int iteration,
                                          QJsonObject* issuedCertificate, QString* error)
{
    QString evidencePath, evidenceFingerprint;
    if (!writeEvidence(projectRoot, configuration, sourceRevision, testResult, iteration, &evidencePath, &evidenceFingerprint, error)) return false;
    CertificationService service; QJsonObject started;
    if (!service.start(projectRoot, QStringLiteral("S1"), QStringLiteral("STRUCTURE"), QStringLiteral("project"), QStringLiteral("HOST_TEST"), QJsonArray{QStringLiteral("s1-evidence-artifact")}, QJsonObject{{"structureVersion",QStringLiteral("S1.1.%1").arg(iteration)},{"sourceRevision",sourceRevision},{"evidenceArtifact",evidencePath},{"evidenceFingerprint",evidenceFingerprint}}, &started, error)) return false;
    return service.issue(projectRoot, started, QStringLiteral("PASS"), QJsonArray{QJsonObject{{"reference",evidencePath},{"fingerprint",evidenceFingerprint},{"type","HOST_TEST"},{"verified",true}}}, issuedCertificate, error);
}
}
