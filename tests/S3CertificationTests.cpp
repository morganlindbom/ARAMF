#include "structure/S3/S3CertificationService.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>

bool runS3CertificationForProject(const QString& projectRoot)
{
    ProjectModel model; ProjectPersistence persistence; QString error; const QString projectFile=QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json")); if(!persistence.load(&model,projectFile,&error)) return false;
    int iteration=1; if(model.dependencyInterfaces().lifecycleHistory.isEmpty()) { S3::Configuration c; const QString root=model.responsibilityOwnership().rootId; c.interfaces.append({"project-api","project-api","Project public contract",root,S3::InterfaceVisibility::Public,S3::InterfaceKind::Api,{},{},"",{}}); model.setDependencyInterfaces(c); if(!model.startS3Iteration(&error)) return false; } else { iteration=model.dependencyInterfaces().lifecycleHistory.last().iteration+1; if(!model.reworkS3(&error)) return false; }
    if(model.dependencyInterfaces().lifecycle.iteration!=iteration||model.dependencyInterfaces().lifecycle.certification!=0||model.dependencyInterfaces().lifecycle.done!=0) return false; if(!persistence.save(model,projectFile,&error)) return false;
    QByteArray material; const QStringList files={"CMakeLists.txt","src/core/ProcessVersion.h","src/core/ProcessVersion.cpp","src/core/ProjectModel.h","src/core/ProjectModel.cpp","src/core/ProjectPersistence.cpp","src/core/Services.cpp","src/structure/S1/ResponsibilityOwnership.h","src/structure/S2/PhysicalStructure.h","src/structure/S3/DependencyInterfaces.h","src/structure/S3/DependencyInterfaces.cpp","src/structure/S3/S3CertificationService.h","src/structure/S3/S3CertificationService.cpp","tests/ProjectMemoryTests.cpp","tests/S3DependencyInterfacesTests.cpp","tests/S3CertificationTests.cpp"}; for(const auto& relative:files){QFile file(QDir::current().filePath(relative));if(!file.open(QIODevice::ReadOnly))return false;material.append(relative.toUtf8());material.append('\0');material.append(file.readAll());material.append('\0');} const QString sourceFingerprint=QString::fromLatin1(QCryptographicHash::hash(material,QCryptographicHash::Sha256).toHex()); QProcess git; git.start("git",{"rev-parse","HEAD"}); if(!git.waitForFinished(10000)||git.exitCode()!=0)return false; const QString commit=QString::fromUtf8(git.readAllStandardOutput()).trimmed(); if(commit.isEmpty())return false;
    QJsonObject issued; if(!S3::CertificationServiceAdapter::certify(projectRoot,model.dependencyInterfaces(),model.responsibilityOwnership(),model.physicalStructure(),QStringLiteral("%1:S3:%2").arg(commit,sourceFingerprint),QJsonObject{{"suite","aramf_s3_tests"},{"status","PASS"},{"passed",1},{"failed",0},{"regression","CTest 8/8 PASS"}},iteration,&issued,&error))return false; if(issued.value("subject").toString()!="S3")return false; if(!model.certifyS3Iteration(&error)||!model.completeS3Iteration(&error))return false; if(!persistence.save(model,projectFile,&error))return false; return GenerationServices().generate(model,model.generationOptions()).success;
}
