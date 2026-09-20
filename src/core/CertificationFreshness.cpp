#include "CertificationFreshness.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QStack>
#include <QTextStream>
#include <QProcess>

namespace {

QString hashBytes(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QString hashJson(const QJsonObject& value)
{
    return hashBytes(QJsonDocument(value).toJson(QJsonDocument::Compact));
}

QJsonObject sortedObject(const QJsonObject& input)
{
    QJsonObject result;
    const auto keys = input.keys();
    for (const auto& key : keys) result.insert(key, input.value(key));
    return result;
}

QString evidencePathFromCertificate(const QJsonObject& certificate)
{
    const auto references = certificate.value(QStringLiteral("evidenceReferences")).toArray();
    for (const auto& value : references) {
        const auto reference = value.toObject().value(QStringLiteral("reference")).toString();
        if (!reference.trimmed().isEmpty()) return reference;
    }
    return certificate.value(QStringLiteral("evidenceArtifact")).toString();
}

QJsonObject readJson(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return {};
    }
    return document.object();
}

QJsonObject latestCertificate(const QString& root, const QString& subject, QString* error)
{
    QFile file(QDir(root).filePath(QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl")));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    QJsonObject latest;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()
            && document.object().value(QStringLiteral("subject")).toString() == subject) latest = document.object();
    }
    return latest;
}

QString currentGitRevision(QString* error)
{
    QProcess git;
    git.start(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (!git.waitForFinished(10000) || git.exitCode() != 0) {
        if (error) *error = QStringLiteral("Unable to resolve current Git revision.");
        return {};
    }
    return QString::fromUtf8(git.readAllStandardOutput()).trimmed();
}

QString currentContractFingerprint(const QString& subject, const QString& root, QString* error)
{
    const QString projectFile = QDir(root).filePath(QStringLiteral("ARAMF_WORKER.aramf.json"));
    const auto project = readJson(projectFile, error);
    if (project.isEmpty() && error && !error->isEmpty()) return {};

    const auto structure = project.value(QStringLiteral("structure")).toObject();
    static const QMap<QString, QString> keys = {
        {QStringLiteral("S1"), QStringLiteral("s1ResponsibilityOwnership")},
        {QStringLiteral("S2"), QStringLiteral("s2PhysicalStructure")},
        {QStringLiteral("S3"), QStringLiteral("s3DependencyInterfaces")},
        {QStringLiteral("S4"), QStringLiteral("s4CompositionEncapsulation")},
        {QStringLiteral("S5"), QStringLiteral("s5DecompositionModularity")},
        {QStringLiteral("S6"), QStringLiteral("s6StructuralEvolutionEnforcement")}
    };
    if (keys.contains(subject)) {
        if (!structure.contains(keys.value(subject))) return {};
        return hashJson(structure.value(keys.value(subject)).toObject());
    }

    // F/P have no project-owned JSON contract section. Their compact,
    // versioned dependency contract is intentionally separate from source
    // and evidence fingerprints.
    return hashJson(QJsonObject{{QStringLiteral("subject"), subject},
                                {QStringLiteral("contractVersion"), 1},
                                {QStringLiteral("dependencies"), QJsonArray::fromStringList(
                                    CertificationFreshnessService::dependencyManifest(subject).toJson(false)
                                        .value(QStringLiteral("directDependencies")).toArray().isEmpty()
                                    ? QStringList{} : QStringList{subject})}});
}

QStringList filesFor(const QString& subject)
{
    const QStringList shared = {
        QStringLiteral("CMakeLists.txt"),
        QStringLiteral("src/core/ProcessVersion.h"),
        QStringLiteral("src/core/ProcessVersion.cpp"),
        QStringLiteral("src/core/ProjectModel.h"),
        QStringLiteral("src/core/ProjectModel.cpp"),
        QStringLiteral("src/core/ProjectPersistence.cpp"),
        QStringLiteral("src/core/Services.cpp"),
        QStringLiteral("tests/ProjectMemoryTests.cpp")
    };
    if (subject == QStringLiteral("S1")) return shared + QStringList{
        QStringLiteral("src/core/ProjectSchema.h"), QStringLiteral("src/structure/S1/ResponsibilityOwnership.h"),
        QStringLiteral("src/structure/S1/ResponsibilityOwnership.cpp"), QStringLiteral("src/structure/S1/S1CertificationService.h"),
        QStringLiteral("src/structure/S1/S1CertificationService.cpp"), QStringLiteral("tests/FoundationNamespaceTests.cpp"),
        QStringLiteral("tests/S1ResponsibilityTests.cpp"), QStringLiteral("tests/S1CertificationTests.cpp")};
    if (subject == QStringLiteral("S2")) return shared + QStringList{
        QStringLiteral("src/core/ProjectSchema.h"), QStringLiteral("src/structure/S1/ResponsibilityOwnership.h"),
        QStringLiteral("src/structure/S2/PhysicalStructure.h"), QStringLiteral("src/structure/S2/PhysicalStructure.cpp"),
        QStringLiteral("src/structure/S2/S2CertificationService.h"), QStringLiteral("src/structure/S2/S2CertificationService.cpp"),
        QStringLiteral("tests/S2PhysicalStructureTests.cpp"), QStringLiteral("tests/S2CertificationTests.cpp")};
    if (subject == QStringLiteral("S3")) return shared + QStringList{
        QStringLiteral("src/structure/S1/ResponsibilityOwnership.h"), QStringLiteral("src/structure/S2/PhysicalStructure.h"),
        QStringLiteral("src/structure/S3/DependencyInterfaces.h"), QStringLiteral("src/structure/S3/DependencyInterfaces.cpp"),
        QStringLiteral("src/structure/S3/S3CertificationService.h"), QStringLiteral("src/structure/S3/S3CertificationService.cpp"),
        QStringLiteral("tests/S3DependencyInterfacesTests.cpp"), QStringLiteral("tests/S3CertificationTests.cpp")};
    if (subject == QStringLiteral("S4")) return shared + QStringList{
        QStringLiteral("src/structure/S1/ResponsibilityOwnership.h"), QStringLiteral("src/structure/S2/PhysicalStructure.h"),
        QStringLiteral("src/structure/S3/DependencyInterfaces.h"), QStringLiteral("src/structure/S4/CompositionEncapsulation.h"),
        QStringLiteral("src/structure/S4/CompositionEncapsulation.cpp"), QStringLiteral("src/structure/S4/S4CertificationService.h"),
        QStringLiteral("src/structure/S4/S4CertificationService.cpp"), QStringLiteral("tests/S4CompositionEncapsulationTests.cpp"),
        QStringLiteral("tests/S4CertificationTests.cpp")};
    if (subject == QStringLiteral("S5")) return shared + QStringList{
        QStringLiteral("src/structure/S1/ResponsibilityOwnership.h"), QStringLiteral("src/structure/S2/PhysicalStructure.h"),
        QStringLiteral("src/structure/S3/DependencyInterfaces.h"), QStringLiteral("src/structure/S4/CompositionEncapsulation.h"),
        QStringLiteral("src/structure/S5/DecompositionModularity.h"), QStringLiteral("src/structure/S5/DecompositionModularity.cpp"),
        QStringLiteral("src/structure/S5/S5CertificationService.h"), QStringLiteral("src/structure/S5/S5CertificationService.cpp"),
        QStringLiteral("tests/S5DecompositionModularityTests.cpp"), QStringLiteral("tests/S5CertificationTests.cpp")};
    if (subject == QStringLiteral("S6")) return shared + QStringList{
        QStringLiteral("src/structure/S1/ResponsibilityOwnership.h"), QStringLiteral("src/structure/S2/PhysicalStructure.h"),
        QStringLiteral("src/structure/S3/DependencyInterfaces.h"), QStringLiteral("src/structure/S4/CompositionEncapsulation.h"),
        QStringLiteral("src/structure/S5/DecompositionModularity.h"), QStringLiteral("src/structure/S6/StructuralEvolutionEnforcement.h"),
        QStringLiteral("src/structure/S6/StructuralEvolutionEnforcement.cpp"), QStringLiteral("src/structure/S6/S6CertificationService.h"),
        QStringLiteral("src/structure/S6/S6CertificationService.cpp"), QStringLiteral("tests/S6StructuralEvolutionEnforcementTests.cpp"),
        QStringLiteral("tests/S6CertificationTests.cpp")};
    if (subject == QStringLiteral("F1")) return {
        QStringLiteral("src/core/MemoryEvidenceFoundation.h"), QStringLiteral("src/core/MemoryEvidenceFoundation.cpp"),
        QStringLiteral("src/core/FoundationServices.h"), QStringLiteral("src/core/FoundationServices.cpp"),
        QStringLiteral("src/core/CertificationService.h"), QStringLiteral("src/core/CertificationService.cpp"),
        QStringLiteral("src/core/ProjectMemory.h"), QStringLiteral("src/core/ProjectMemory.cpp"),
        QStringLiteral("tests/FoundationTests.cpp"), QStringLiteral("tests/ProjectMemoryTests.cpp")};
    if (subject == QStringLiteral("P1")) return {QStringLiteral("src/core/WorkerTaskServices.h"), QStringLiteral("src/core/WorkerTaskServices.cpp"), QStringLiteral("tests/WorkerTaskTests.cpp"), QStringLiteral("tests/P2ExecutionTests.cpp")};
    if (subject == QStringLiteral("P2")) return {QStringLiteral("src/core/ContextCoordinationService.h"), QStringLiteral("src/core/ContextCoordinationService.cpp"), QStringLiteral("src/core/WorkerContextResolver.h"), QStringLiteral("src/core/WorkerContextResolver.cpp"), QStringLiteral("tests/ContextCoordinationTests.cpp")};
    if (subject == QStringLiteral("P3")) return {QStringLiteral("src/core/ExecutionOrchestrator.h"), QStringLiteral("src/core/ExecutionOrchestrator.cpp"), QStringLiteral("tests/P2ExecutionTests.cpp")};
    if (subject == QStringLiteral("P4")) return {QStringLiteral("src/core/PredictiveOptimizationService.h"), QStringLiteral("src/core/PredictiveOptimizationService.cpp"), QStringLiteral("tests/P3PredictiveTests.cpp")};
    if (subject == QStringLiteral("P5")) return {QStringLiteral("src/core/AdaptiveRoutingService.h"), QStringLiteral("src/core/AdaptiveRoutingService.cpp"), QStringLiteral("tests/P4RoutingTests.cpp")};
    return {};
}

}

QJsonObject CertificationSourceManifest::toJson() const
{
    return {{QStringLiteral("manifestVersion"), manifestVersion}, {QStringLiteral("subject"), subject},
            {QStringLiteral("files"), QJsonArray::fromStringList(files)}};
}

QString CertificationSourceManifest::fingerprint() const
{
    return hashJson(toJson());
}

QJsonObject CertificationDependencyBinding::toJson() const
{
    return {{QStringLiteral("subject"), subject}, {QStringLiteral("requiredLifecycle"), requiredLifecycle},
            {QStringLiteral("certificateId"), certificateId}, {QStringLiteral("sourceRevision"), sourceRevision},
            {QStringLiteral("contractFingerprint"), contractFingerprint}, {QStringLiteral("evidenceFingerprint"), evidenceFingerprint},
            {QStringLiteral("dependencyManifestFingerprint"), dependencyManifestFingerprint}};
}

QJsonObject CertificationDependencyManifest::toJson(bool includeFingerprint) const
{
    QJsonArray dependencies;
    for (const auto& dependency : directDependencies) dependencies.append(dependency.toJson());
    QJsonObject result{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("subject"), subject},
                       {QStringLiteral("lifecycle"), lifecycle}, {QStringLiteral("directDependencies"), dependencies}};
    if (includeFingerprint) result.insert(QStringLiteral("manifestFingerprint"), manifestFingerprint.isEmpty() ? computedFingerprint() : manifestFingerprint);
    return result;
}

QString CertificationDependencyManifest::computedFingerprint() const
{
    return hashJson(toJson(false));
}

QString certificationFreshnessStatusName(CertificationFreshnessStatus status)
{
    switch (status) {
    case CertificationFreshnessStatus::Fresh: return QStringLiteral("FRESH");
    case CertificationFreshnessStatus::SourceStale: return QStringLiteral("SOURCE_STALE");
    case CertificationFreshnessStatus::DependencyStale: return QStringLiteral("DEPENDENCY_STALE");
    case CertificationFreshnessStatus::TransitiveDependencyStale: return QStringLiteral("TRANSITIVE_DEPENDENCY_STALE");
    case CertificationFreshnessStatus::DependencyBindingIncomplete: return QStringLiteral("DEPENDENCY_BINDING_INCOMPLETE");
    case CertificationFreshnessStatus::EvidenceStale: return QStringLiteral("EVIDENCE_STALE");
    case CertificationFreshnessStatus::FreshnessIndeterminate: return QStringLiteral("FRESHNESS_INDETERMINATE");
    case CertificationFreshnessStatus::NotCertified: return QStringLiteral("NOT_CERTIFIED");
    case CertificationFreshnessStatus::NotStarted: return QStringLiteral("NOT_STARTED");
    }
    return QStringLiteral("FRESHNESS_INDETERMINATE");
}

QJsonObject CertificationFreshnessResult::toJson() const
{
    QJsonArray dependencies;
    for (const auto& result : directDependencyResults) dependencies.append(result.toJson());
    return {{QStringLiteral("subject"), subject}, {QStringLiteral("historicalCertificateId"), historicalCertificateId},
            {QStringLiteral("historicalLifecycle"), historicalLifecycle}, {QStringLiteral("certifiedSourceRevision"), certifiedSourceRevision},
            {QStringLiteral("currentSourceRevision"), currentSourceRevision}, {QStringLiteral("certifiedSourceFingerprint"), certifiedSourceFingerprint},
            {QStringLiteral("currentSourceFingerprint"), currentSourceFingerprint}, {QStringLiteral("certifiedContractFingerprint"), certifiedContractFingerprint},
            {QStringLiteral("currentContractFingerprint"), currentContractFingerprint}, {QStringLiteral("certifiedEvidenceFingerprint"), certifiedEvidenceFingerprint},
            {QStringLiteral("currentEvidenceFingerprint"), currentEvidenceFingerprint}, {QStringLiteral("dependencyManifestFingerprint"), dependencyManifestFingerprint},
            {QStringLiteral("sourceFresh"), sourceFresh}, {QStringLiteral("contractFresh"), contractFresh},
            {QStringLiteral("directDependencyFresh"), directDependencyFresh}, {QStringLiteral("transitiveDependencyFresh"), transitiveDependencyFresh},
            {QStringLiteral("evidenceFresh"), evidenceFresh}, {QStringLiteral("bindingComplete"), bindingComplete},
            {QStringLiteral("status"), certificationFreshnessStatusName(status)}, {QStringLiteral("reasons"), QJsonArray::fromStringList(reasons)},
            {QStringLiteral("directDependencyResults"), dependencies}};
}

CertificationDependencyManifest CertificationFreshnessService::dependencyManifest(const QString& subject)
{
    CertificationDependencyManifest manifest;
    manifest.subject = subject;
    manifest.lifecycle = subject.startsWith(QLatin1Char('S')) ? QStringLiteral("%1.1").arg(subject) : subject;
    const QStringList dependencies =
        subject == QStringLiteral("S2") ? QStringList{QStringLiteral("S1")} :
        subject == QStringLiteral("S3") ? QStringList{QStringLiteral("S1"), QStringLiteral("S2")} :
        subject == QStringLiteral("S4") ? QStringList{QStringLiteral("S1"), QStringLiteral("S2"), QStringLiteral("S3")} :
        subject == QStringLiteral("S5") ? QStringList{QStringLiteral("S1"), QStringLiteral("S2"), QStringLiteral("S3"), QStringLiteral("S4")} :
        subject == QStringLiteral("S6") ? QStringList{QStringLiteral("S1"), QStringLiteral("S2"), QStringLiteral("S3"), QStringLiteral("S4"), QStringLiteral("S5")} :
        subject == QStringLiteral("P1") ? QStringList{QStringLiteral("F1")} :
        subject == QStringLiteral("P2") ? QStringList{QStringLiteral("P1")} :
        subject == QStringLiteral("P3") ? QStringList{QStringLiteral("P1"), QStringLiteral("P2")} :
        subject == QStringLiteral("P4") ? QStringList{QStringLiteral("P1"), QStringLiteral("P2"), QStringLiteral("P3")} :
        subject == QStringLiteral("P5") ? QStringList{QStringLiteral("P1"), QStringLiteral("P2"), QStringLiteral("P3"), QStringLiteral("P4")} : QStringList{};
    for (const auto& dependency : dependencies) {
        CertificationDependencyBinding binding;
        binding.subject = dependency;
        manifest.directDependencies.append(binding);
    }
    manifest.manifestFingerprint = manifest.computedFingerprint();
    return manifest;
}

bool CertificationSourceManifestProvider::fingerprint(const QString& projectRoot, const QString& subject, QString* result, QString* error)
{
    if (error) error->clear();
    const auto sourceManifest = manifest(subject);
    if (sourceManifest.files.isEmpty()) {
        if (error) *error = QStringLiteral("No canonical source manifest exists for %1.").arg(subject);
        return false;
    }
    QByteArray material;
    for (const auto& relative : sourceManifest.files) {
        QFile file(QDir(projectRoot).filePath(relative));
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("Cannot read source manifest file %1: %2").arg(relative, file.errorString());
            return false;
        }
        material.append(relative.toUtf8());
        material.append('\0');
        material.append(file.readAll());
        material.append('\0');
    }
    if (result) *result = hashBytes(material);
    return true;
}

CertificationSourceManifest CertificationSourceManifestProvider::manifest(const QString& subject)
{
    return {subject, QStringLiteral("1"), filesFor(subject)};
}

CertificationFreshnessResult CertificationFreshnessService::evaluateSnapshot(
    const QString& subject, const QJsonObject& historical, const QString& currentSourceRevision,
    const QString& currentSourceFingerprint, const QString& currentContractFingerprint,
    const QString& currentEvidenceFingerprint, const QHash<QString, CertificationFreshnessResult>& dependencyResults)
{
    CertificationFreshnessResult result;
    result.subject = subject;
    result.historicalCertificateId = historical.value(QStringLiteral("certificateId")).toString();
    result.historicalLifecycle = historical.value(QStringLiteral("lifecycle")).toString();
    result.certifiedSourceRevision = historical.value(QStringLiteral("sourceRevision")).toString();
    result.certifiedSourceFingerprint = historical.value(QStringLiteral("sourceFingerprint")).toString();
    result.certifiedContractFingerprint = historical.value(QStringLiteral("contractFingerprint")).toString();
    result.certifiedEvidenceFingerprint = historical.value(QStringLiteral("evidenceFingerprint")).toString();
    result.currentSourceRevision = currentSourceRevision;
    result.currentSourceFingerprint = currentSourceFingerprint;
    result.currentContractFingerprint = currentContractFingerprint;
    result.currentEvidenceFingerprint = currentEvidenceFingerprint;
    result.dependencyManifestFingerprint = dependencyManifest(subject).manifestFingerprint;
    if (result.historicalCertificateId.isEmpty()) {
        result.status = historical.isEmpty() ? CertificationFreshnessStatus::NotCertified : CertificationFreshnessStatus::FreshnessIndeterminate;
        result.reasons.append(QStringLiteral("No historical certificate binding was supplied."));
        return result;
    }
    result.sourceFresh = !result.certifiedSourceFingerprint.isEmpty()
        ? result.certifiedSourceFingerprint == result.currentSourceFingerprint
        : (!result.certifiedSourceRevision.isEmpty() && result.certifiedSourceRevision == result.currentSourceRevision);
    result.contractFresh = !result.certifiedContractFingerprint.isEmpty()
        ? result.certifiedContractFingerprint == result.currentContractFingerprint : true;
    result.evidenceFresh = !result.certifiedEvidenceFingerprint.isEmpty()
        ? result.certifiedEvidenceFingerprint == result.currentEvidenceFingerprint : false;
    if (!result.sourceFresh) result.reasons.append(QStringLiteral("Certified source binding differs from the current source state."));
    if (!result.contractFresh) result.reasons.append(QStringLiteral("Certified contract fingerprint differs from the current contract state."));
    if (!result.evidenceFresh) result.reasons.append(QStringLiteral("Persisted evidence bytes are not proven equal to the certified evidence fingerprint."));

    const auto manifest = dependencyManifest(subject);
    result.bindingComplete = true;
    bool directFresh = true;
    bool transitiveFresh = true;
    for (const auto& dependency : manifest.directDependencies) {
        if (!dependencyResults.contains(dependency.subject)) {
            result.bindingComplete = false;
            directFresh = false;
            result.reasons.append(QStringLiteral("Missing evaluated direct dependency %1.").arg(dependency.subject));
            continue;
        }
        const auto dependencyResult = dependencyResults.value(dependency.subject);
        result.directDependencyResults.append(dependencyResult);
        const QString bindingKey = dependency.subject.toLower() + QStringLiteral("ContractFingerprint");
        const QString historicalBinding = historical.value(bindingKey).toString();
        if (historicalBinding.isEmpty()) {
            result.bindingComplete = false;
            result.reasons.append(QStringLiteral("Historical direct dependency binding for %1 is absent.").arg(dependency.subject));
        } else if (historicalBinding != dependencyResult.currentContractFingerprint) {
            directFresh = false;
            result.reasons.append(QStringLiteral("Direct dependency contract for %1 differs from its certified binding.").arg(dependency.subject));
        }
        if (dependencyResult.status != CertificationFreshnessStatus::Fresh) {
            directFresh = false;
            if (dependencyResult.status == CertificationFreshnessStatus::TransitiveDependencyStale) {
                transitiveFresh = false;
                result.reasons.append(QStringLiteral("Direct dependency %1 is transitively stale.").arg(dependency.subject));
            } else {
                result.reasons.append(QStringLiteral("Direct dependency %1 is stale or indeterminate.").arg(dependency.subject));
            }
        }
    }
    result.directDependencyFresh = directFresh;
    result.transitiveDependencyFresh = transitiveFresh;
    if (!result.bindingComplete) result.status = CertificationFreshnessStatus::DependencyBindingIncomplete;
    else if (!transitiveFresh) result.status = CertificationFreshnessStatus::TransitiveDependencyStale;
    else if (!directFresh) result.status = CertificationFreshnessStatus::DependencyStale;
    else if (!result.evidenceFresh) result.status = CertificationFreshnessStatus::EvidenceStale;
    else if (!result.sourceFresh) result.status = CertificationFreshnessStatus::SourceStale;
    else if (!result.contractFresh) result.status = CertificationFreshnessStatus::SourceStale;
    else result.status = CertificationFreshnessStatus::Fresh;
    return result;
}

namespace {
CertificationFreshnessResult evaluateSubject(const QString& subject, const QString& projectRoot,
                                              QSet<QString>& visiting, QString* error)
{
    CertificationFreshnessResult result;
    result.subject = subject;
    if (visiting.contains(subject)) {
        result.status = CertificationFreshnessStatus::FreshnessIndeterminate;
        result.reasons.append(QStringLiteral("Dependency graph cycle detected at %1.").arg(subject));
        return result;
    }
    visiting.insert(subject);
    const QJsonObject certificate = latestCertificate(projectRoot, subject, error);
    if (certificate.isEmpty()) {
        result.status = CertificationFreshnessStatus::NotCertified;
        result.reasons.append(QStringLiteral("No historical certificate exists for %1.").arg(subject));
        visiting.remove(subject);
        return result;
    }
    const QString evidenceReference = evidencePathFromCertificate(certificate);
    const QString evidenceFile = QDir(projectRoot).filePath(evidenceReference);
    const auto evidence = readJson(evidenceFile, error);
    if (evidence.isEmpty()) { visiting.remove(subject); return result; }
    QString sourceFingerprint;
    QString sourceError;
    CertificationSourceManifestProvider::fingerprint(QDir::currentPath(), subject, &sourceFingerprint, &sourceError);
    QString contractError;
    const QString contractFingerprint = currentContractFingerprint(subject, projectRoot, &contractError);
    if (error && error->isEmpty() && !contractError.isEmpty() && subject.startsWith(QLatin1Char('S'))) *error = contractError;
    QString currentRevision = currentGitRevision(error);
    const QString historicalSourceRevision = evidence.value(QStringLiteral("sourceRevision")).toString();
    auto historical = QJsonObject{
        {QStringLiteral("certificateId"), certificate.value(QStringLiteral("certificateId"))},
        {QStringLiteral("lifecycle"), evidence.value(QStringLiteral("lifecycle"))},
        {QStringLiteral("sourceRevision"), historicalSourceRevision},
        {QStringLiteral("sourceFingerprint"), evidence.value(QStringLiteral("sourceFingerprint"))},
        {QStringLiteral("contractFingerprint"), evidence.value(QStringLiteral("contractFingerprint"))},
        {QStringLiteral("evidenceFingerprint"), certificate.value(QStringLiteral("evidenceFingerprint"))}};
    for (const auto& dependency : CertificationFreshnessService::dependencyManifest(subject).directDependencies) {
        const QString key = dependency.subject.toLower() + QStringLiteral("ContractFingerprint");
        if (evidence.contains(key)) historical.insert(key, evidence.value(key));
        else if (evidence.value(QStringLiteral("dependencyBindings")).toObject().contains(key))
            historical.insert(key, evidence.value(QStringLiteral("dependencyBindings")).toObject().value(key));
    }
    QHash<QString, CertificationFreshnessResult> dependencies;
    for (const auto& dependency : CertificationFreshnessService::dependencyManifest(subject).directDependencies)
        dependencies.insert(dependency.subject, evaluateSubject(dependency.subject, projectRoot, visiting, error));
    // Evidence byte verification is intentionally performed without changing
    // the file. The current bytes are compared to the certificate reference.
    QString currentEvidenceFingerprint;
    QFile evidenceBytes(evidenceFile);
    if (evidenceBytes.open(QIODevice::ReadOnly)) currentEvidenceFingerprint = hashBytes(evidenceBytes.readAll());
    result = CertificationFreshnessService::evaluateSnapshot(subject, historical, currentRevision, sourceFingerprint, contractFingerprint, currentEvidenceFingerprint, dependencies);
    if (subject == QStringLiteral("S6") && evidence.value(QStringLiteral("s1ContractFingerprint")).toString().isEmpty()) {
        result.bindingComplete = false;
        result.status = CertificationFreshnessStatus::DependencyBindingIncomplete;
        result.reasons.append(QStringLiteral("Historical S6 evidence has no S1-S5 dependency bindings."));
    }
    visiting.remove(subject);
    return result;
}
}

CertificationFreshnessResult CertificationFreshnessService::evaluate(const QString& subject, const QString& projectRoot, QString* error)
{
    if (error) error->clear();
    QSet<QString> visiting;
    return evaluateSubject(subject, projectRoot, visiting, error);
}
