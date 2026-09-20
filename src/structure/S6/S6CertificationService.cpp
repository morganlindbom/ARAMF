#include "S6CertificationService.h"
#include "core/CertificationService.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
namespace S6 {
namespace {
bool writeEvidence(const QString& root, const Configuration& configuration, const QString& source, const QJsonObject& tests, int iteration, QString* path, QString* fingerprint, QString* error)
{
    const auto result=audit(configuration); if(!result.valid){if(error)*error="S6 audit must pass before certification.";return false;}
    const QString relative=QStringLiteral("ARAMF_WORKER/certification/evidence/s6.1.%1/iteration-%1/s6-evidence.json").arg(iteration);const QString absolute=QDir(root).filePath(relative);QDir().mkpath(QFileInfo(absolute).absolutePath());
    const QJsonObject evidence{{"schemaVersion",1},{"authority","S6"},{"subject","S6"},{"lifecycle",QStringLiteral("S6.1.%1.0.0").arg(iteration)},{"sourceRevision",source},{"sourceFingerprint",result.modelFingerprint},{"generatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"testResult",tests},{"audit",result.toJson()}};
    const QByteArray bytes=QJsonDocument(evidence).toJson(QJsonDocument::Indented);const QString hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());QSaveFile file(absolute);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){if(error)*error=file.errorString();return false;}if(path)*path=relative;if(fingerprint)*fingerprint=hash;return true;
}
}
bool CertificationServiceAdapter::certify(const QString& root,const Configuration& configuration,const QString& sourceRevision,const QJsonObject& testResult,int iteration,QJsonObject* issuedCertificate,QString* error)
{QString path,fingerprint;if(!writeEvidence(root,configuration,sourceRevision,testResult,iteration,&path,&fingerprint,error))return false;CertificationService service;QJsonObject started;if(!service.start(root,"S6","STRUCTURE","project","HOST_TEST",QJsonArray{"s6-evidence-artifact"},{{"structureVersion",QStringLiteral("S6.1.%1").arg(iteration)},{"sourceRevision",sourceRevision},{"evidenceArtifact",path},{"evidenceFingerprint",fingerprint}},&started,error))return false;return service.issue(root,started,"PASS",QJsonArray{QJsonObject{{"reference",path},{"fingerprint",fingerprint},{"type","HOST_TEST"},{"verified",true}}},issuedCertificate,error);}
}
