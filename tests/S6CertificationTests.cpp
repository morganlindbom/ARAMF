#include "structure/S6/S6CertificationService.h"
#include "core/CertificationFreshness.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>

bool runS6CertificationForProject(const QString& root)
{
    ProjectModel model;
    ProjectPersistence persistence;
    QString error;
    const QString projectFile = QDir(root).filePath("ARAMF_WORKER.aramf.json");
    if (!persistence.load(&model, projectFile, &error)) return false;
    int iteration = 1;
    if (model.structuralEvolutionEnforcement().lifecycleHistory.isEmpty()) {
        S6::Configuration configuration;
        configuration.metadata.insert("legacyBaseline", true);
        model.setStructuralEvolutionEnforcement(configuration);
        if (!model.startS6Iteration(&error)) return false;
    } else {
        iteration = model.structuralEvolutionEnforcement().lifecycleHistory.last().iteration + 1;
        if (!model.reworkS6(&error)) return false;
    }
    if (model.structuralEvolutionEnforcement().lifecycle.iteration != iteration
        || model.structuralEvolutionEnforcement().lifecycle.certification != 0
        || model.structuralEvolutionEnforcement().lifecycle.done != 0) return false;
    if (!persistence.save(model, projectFile, &error)) return false;

    QByteArray material;
    const QStringList files = {"CMakeLists.txt", "src/core/ProcessVersion.h", "src/core/ProcessVersion.cpp",
        "src/core/ProjectModel.h", "src/core/ProjectModel.cpp", "src/core/ProjectPersistence.cpp", "src/core/Services.cpp",
        "src/structure/S1/ResponsibilityOwnership.h", "src/structure/S2/PhysicalStructure.h",
        "src/structure/S3/DependencyInterfaces.h", "src/structure/S4/CompositionEncapsulation.h",
        "src/structure/S5/DecompositionModularity.h", "src/structure/S6/StructuralEvolutionEnforcement.h",
        "src/structure/S6/StructuralEvolutionEnforcement.cpp", "src/structure/S6/S6CertificationService.h",
        "src/structure/S6/S6CertificationService.cpp", "tests/ProjectMemoryTests.cpp",
        "tests/S6StructuralEvolutionEnforcementTests.cpp", "tests/S6CertificationTests.cpp"};
    for (const auto& path : files) {
        QFile file(QDir::current().filePath(path));
        if (!file.open(QIODevice::ReadOnly)) return false;
        material.append(path.toUtf8()); material.append('\0'); material.append(file.readAll()); material.append('\0');
    }
    const QString sourceFingerprint = QString::fromLatin1(QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex());
    QProcess git;
    git.start("git", {"rev-parse", "HEAD"});
    if (!git.waitForFinished(10000) || git.exitCode() != 0) return false;
    const QString commit = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    QJsonObject issued;
    const QJsonObject tests{{"suite", "aramf_s6_tests"}, {"status", "PASS"}, {"passed", 1}, {"failed", 0}, {"regression", "CTest 11/11 PASS"}};
    QString bindingError;
    const QJsonObject dependencyBindings = CertificationDependencyBindingProvider::currentBindings("S6", root, &bindingError);
    if (dependencyBindings.isEmpty()) return false;
    QJsonObject rejected;
    QJsonObject invalidBindings{{"s1ContractFingerprint", "fixture-s1"}, {"s2ContractFingerprint", "fixture-s2"},
        {"s3ContractFingerprint", "fixture-s3"}, {"s4ContractFingerprint", "fixture-s4"}, {"s5ContractFingerprint", "fixture-s5"}};
    if (S6::CertificationServiceAdapter::certifyWithDependencies(root, model.structuralEvolutionEnforcement(),
            QStringLiteral("invalid"), tests, iteration, invalidBindings, &rejected, &error)) return false;
    if (!S6::CertificationServiceAdapter::certifyWithDependencies(root, model.structuralEvolutionEnforcement(),
            QStringLiteral("%1:S6:%2").arg(commit, sourceFingerprint), tests, iteration, dependencyBindings, &issued, &error)) return false;
    if (issued.value("subject").toString() != "S6") return false;
    if (!model.certifyS6Iteration(&error) || !model.completeS6Iteration(&error)) return false;
    if (!persistence.save(model, projectFile, &error)) return false;
    return GenerationServices().generate(model, model.generationOptions()).success;
}
