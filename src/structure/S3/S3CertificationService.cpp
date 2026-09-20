#include "S3CertificationService.h"
#include "core/CertificationService.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace S3 {
namespace {
QString fingerprint(const QJsonObject& value) { return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(value).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex()); }
bool writeEvidence(const QString& root, const Configuration& configuration, const S1::Configuration& ownership, const S2::Configuration& physical, const QString& sourceRevision, const QJsonObject& testResult, int iteration, QString* relativePath, QString* evidenceFingerprint, QString* error) {
    const auto result=audit(configuration,ownership,physical); if(!result.valid){if(error)*error="S3 audit must pass before certification.";return false;}
    const QString relative=QStringLiteral("ARAMF_WORKER/certification/evidence/s3.1.%1/iteration-%1/s3-evidence.json").arg(iteration); const QString absolute=QDir(root).filePath(relative); QDir().mkpath(QFileInfo(absolute).absolutePath());
    const QJsonObject evidence{{"schemaVersion",1},{"authority","S3"},{"subject","S3"},{"lifecycle",QStringLiteral("S3.1.%1.0.0").arg(iteration)},{"sourceRevision",sourceRevision},{"sourceFingerprint",result.modelFingerprint},{"generatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"testResult",testResult},{"audit",result.toJson()},{"s1ContractFingerprint",fingerprint(S1::toJson(ownership))},{"s2ContractFingerprint",fingerprint(S2::toJson(physical))}};
    const QByteArray bytes=QJsonDocument(evidence).toJson(QJsonDocument::Indented); const QString hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); QSaveFile file(absolute); if(!file.open(QIODevice::WriteOnly|QIODevice::Text)||file.write(bytes)!=bytes.size()||!file.commit()){if(error)*error=file.errorString();return false;} if(relativePath)*relativePath=relative; if(evidenceFingerprint)*evidenceFingerprint=hash; return true;
}
}
bool CertificationServiceAdapter::certify(const QString& root, const Configuration& configuration, const S1::Configuration& ownership, const S2::Configuration& physical, const QString& sourceRevision, const QJsonObject& testResult, int iteration, QJsonObject* issuedCertificate, QString* error) {
    QString path, hash; if(!writeEvidence(root,configuration,ownership,physical,sourceRevision,testResult,iteration,&path,&hash,error)) return false; CertificationService service; QJsonObject started; if(!service.start(root,"S3","STRUCTURE","project","HOST_TEST",QJsonArray{"s3-evidence-artifact"},{{"structureVersion",QStringLiteral("S3.1.%1").arg(iteration)},{"sourceRevision",sourceRevision},{"evidenceArtifact",path},{"evidenceFingerprint",hash}},&started,error)) return false; return service.issue(root,started,"PASS",QJsonArray{QJsonObject{{"reference",path},{"fingerprint",hash},{"type","HOST_TEST"},{"verified",true}}},issuedCertificate,error);
}
}
