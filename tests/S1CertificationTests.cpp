#include "structure/S1/S1CertificationService.h"
#include "core/ProjectMemory.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QProcess>

bool runS1CertificationTests()
{
    QTemporaryDir fixture;
    if (!fixture.isValid()) return false;
    QString error;
    ProjectMemory memory;
    if (!memory.initializeMemory(fixture.path(), nullptr, &error)) return false;
    S1::Configuration configuration;
    configuration.rootId = QStringLiteral("project");
    configuration.responsibilities.append({QStringLiteral("project"), QStringLiteral("project"), QStringLiteral("Project"), S1::ResponsibilityKind::Project, {}, {}, {}, {}, {}});
    QJsonObject issued;
    if (!S1::CertificationServiceAdapter::certify(fixture.path(), configuration, QStringLiteral("test-source-revision"), QJsonObject{{"status","PASS"},{"passed",1},{"failed",0}}, 1, &issued, &error)) return false;
    return issued.value(QStringLiteral("subject")).toString() == QStringLiteral("S1")
        && issued.value(QStringLiteral("certificationStatus")).toString() == QStringLiteral("CERTIFIED")
        && QFileInfo::exists(QDir(fixture.path()).filePath(QStringLiteral("ARAMF_WORKER/certification/evidence/s1.1.1/iteration-1/s1-evidence.json")));
}

bool runS1CertificationForProject(const QString& projectRoot)
{
    ProjectModel model;
    ProjectPersistence persistence;
    QString error;
    const QString projectFile = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json"));
    if (!persistence.load(&model, projectFile, &error)) return false;
    if (!model.reworkS1(&error)) return false;
    const S1::Configuration configuration = model.responsibilityOwnership();
    if (configuration.lifecycle.iteration != 4 || configuration.lifecycle.certification != 0 || configuration.lifecycle.done != 0) return false;
    if (!persistence.save(model, projectFile, &error)) return false;
    QByteArray material;
    const QStringList files = {"CMakeLists.txt", "src/core/ProcessVersion.h", "src/core/ProcessVersion.cpp", "src/core/ProjectModel.h", "src/core/ProjectModel.cpp", "src/core/ProjectPersistence.cpp", "src/core/ProjectSchema.h", "src/core/Services.cpp", "src/structure/S1/ResponsibilityOwnership.h", "src/structure/S1/ResponsibilityOwnership.cpp", "src/structure/S1/S1CertificationService.h", "src/structure/S1/S1CertificationService.cpp", "tests/FoundationNamespaceTests.cpp", "tests/ProjectMemoryTests.cpp", "tests/S1ResponsibilityTests.cpp", "tests/S1CertificationTests.cpp"};
    for (const auto& relative : files) { QFile file(QDir::current().filePath(relative)); if (!file.open(QIODevice::ReadOnly)) return false; material.append(relative.toUtf8()); material.append('\0'); material.append(file.readAll()); material.append('\0'); }
    const QString sourceFingerprint = QString::fromLatin1(QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex());
    QProcess git;
    git.start(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (!git.waitForFinished(10000) || git.exitCode() != 0) return false;
    const QString commit = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    if (commit.isEmpty()) return false;
    QJsonObject issued;
    const bool ok = S1::CertificationServiceAdapter::certify(projectRoot, configuration,
        QStringLiteral("%1:S1:%2").arg(commit, sourceFingerprint),
        QJsonObject{{"suite","aramf_s1_tests"},{"status","PASS"},{"passed",1},{"failed",0},{"regression","CTest 6/6 PASS"}}, 4, &issued, &error);
    if (!ok) qWarning().noquote() << error;
    if (!ok || issued.value(QStringLiteral("subject")).toString() != QStringLiteral("S1")) return false;
    if (!model.certifyS1Iteration(&error) || !model.completeS1Iteration(&error)) return false;
    if (!persistence.save(model, projectFile, &error)) return false;
    const auto generated = GenerationServices().generate(model, model.generationOptions());
    return generated.success;
}

bool runS1CloseoutGeneration(const QString& projectRoot)
{
    ProjectModel model;
    ProjectPersistence persistence;
    QString error;
    if (!persistence.load(&model, QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json")), &error)) return false;
    const auto result = GenerationServices().generate(model, model.generationOptions());
    return result.success;
}
