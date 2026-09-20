#include "S4CertificationService.h"
#include "core/CertificationService.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
namespace S4 {
namespace {
QString hash(const QJsonObject& value){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(value).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
bool writeEvidence(const QString& root,const Configuration& c,const S1::Configuration& s1,const S2::Configuration& s2,const S3::Configuration& s3,const QString& source,const QJsonObject& tests,int iteration,QString* path,QString* evidenceHash,QString* error){const auto result=audit(c,s1,s2,s3);if(!result.valid){if(error)*error="S4 audit must pass before certification.";return false;}const QString relative=QStringLiteral("ARAMF_WORKER/certification/evidence/s4.1.%1/iteration-%1/s4-evidence.json").arg(iteration);const QString absolute=QDir(root).filePath(relative);QDir().mkpath(QFileInfo(absolute).absolutePath());const QJsonObject evidence{{"schemaVersion",1},{"authority","S4"},{"subject","S4"},{"lifecycle",QStringLiteral("S4.1.%1.0.0").arg(iteration)},{"sourceRevision",source},{"sourceFingerprint",result.modelFingerprint},{"generatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"testResult",tests},{"audit",result.toJson()},{"s1ContractFingerprint",hash(S1::toJson(s1))},{"s2ContractFingerprint",hash(S2::toJson(s2))},{"s3ContractFingerprint",hash(S3::toJson(s3))}};const QByteArray bytes=QJsonDocument(evidence).toJson(QJsonDocument::Indented);const QString h=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());QSaveFile file(absolute);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){if(error)*error=file.errorString();return false;}if(path)*path=relative;if(evidenceHash)*evidenceHash=h;return true;}
}
bool CertificationServiceAdapter::certify(const QString& root,const Configuration& c,const S1::Configuration& s1,const S2::Configuration& s2,const S3::Configuration& s3,const QString& source,const QJsonObject& tests,int iteration,QJsonObject* issued,QString* error){QString path,evidenceHash;if(!writeEvidence(root,c,s1,s2,s3,source,tests,iteration,&path,&evidenceHash,error))return false;CertificationService service;QJsonObject started;if(!service.start(root,"S4","STRUCTURE","project","HOST_TEST",QJsonArray{"s4-evidence-artifact"},{{"structureVersion",QStringLiteral("S4.1.%1").arg(iteration)},{"sourceRevision",source},{"evidenceArtifact",path},{"evidenceFingerprint",evidenceHash}},&started,error))return false;return service.issue(root,started,"PASS",QJsonArray{QJsonObject{{"reference",path},{"fingerprint",evidenceHash},{"type","HOST_TEST"},{"verified",true}}},issued,error);}
}
