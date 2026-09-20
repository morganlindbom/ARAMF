#include "S5CertificationService.h"
#include "core/CertificationService.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
namespace S5 {
namespace {
QString hash(const QJsonObject& value) { return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(value).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex()); }
bool writeEvidence(const QString& root, const Configuration& configuration, const S1::Configuration& ownership,
                   const S2::Configuration& physical, const S3::Configuration& dependencies,
                   const S4::Configuration& composition, const QString& source, const QJsonObject& tests,
                   int iteration, QString* path, QString* evidenceHash, QString* error)
{
    const auto result = audit(configuration, ownership, physical, dependencies, composition);
    if (!result.valid) { if (error) *error = QStringLiteral("S5 audit must pass before certification."); return false; }
    const QString relative = QStringLiteral("ARAMF_WORKER/certification/evidence/s5.1.%1/iteration-%1/s5-evidence.json").arg(iteration);
    const QString absolute = QDir(root).filePath(relative);
    QDir().mkpath(QFileInfo(absolute).absolutePath());
    const QJsonObject evidence{{"schemaVersion",1},{"authority","S5"},{"subject","S5"},
        {"lifecycle",QStringLiteral("S5.1.%1.0.0").arg(iteration)}, {"sourceRevision",source},
        {"sourceFingerprint",result.modelFingerprint}, {"generatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"testResult",tests}, {"audit",result.toJson()}, {"s1ContractFingerprint",hash(S1::toJson(ownership))},
        {"s2ContractFingerprint",hash(S2::toJson(physical))}, {"s3ContractFingerprint",hash(S3::toJson(dependencies))},
        {"s4ContractFingerprint",hash(S4::toJson(composition))}};
    const QByteArray bytes = QJsonDocument(evidence).toJson(QJsonDocument::Indented);
    const QString fingerprint = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    QSaveFile file(absolute);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    if (path) *path = relative; if (evidenceHash) *evidenceHash = fingerprint; return true;
}
}
bool CertificationServiceAdapter::certify(const QString& root, const Configuration& configuration,
    const S1::Configuration& ownership, const S2::Configuration& physical, const S3::Configuration& dependencies,
    const S4::Configuration& composition, const QString& sourceRevision, const QJsonObject& testResult, int iteration,
    QJsonObject* issuedCertificate, QString* error)
{
    QString path, evidenceHash;
    if (!writeEvidence(root, configuration, ownership, physical, dependencies, composition, sourceRevision, testResult, iteration, &path, &evidenceHash, error)) return false;
    CertificationService service; QJsonObject started;
    if (!service.start(root, "S5", "STRUCTURE", "project", "HOST_TEST", QJsonArray{"s5-evidence-artifact"},
        {{"structureVersion",QStringLiteral("S5.1.%1").arg(iteration)}, {"sourceRevision",sourceRevision},
         {"evidenceArtifact",path}, {"evidenceFingerprint",evidenceHash}}, &started, error)) return false;
    return service.issue(root, started, "PASS", QJsonArray{QJsonObject{{"reference",path},{"fingerprint",evidenceHash},{"type","HOST_TEST"},{"verified",true}}}, issuedCertificate, error);
}
}
