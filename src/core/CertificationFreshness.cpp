#include "CertificationFreshness.h"
#include "CertificationRevalidation.h"

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

QJsonObject semanticMetadata(const QString& subject, const QString& context, const QJsonObject& input)
{
    QJsonObject result;
    QStringList keys;
    if (subject == QStringLiteral("S3") && context == QStringLiteral("interfaces"))
        keys = {QStringLiteral("authorizedResponsibilityIds")};
    else if (subject == QStringLiteral("S3") && context == QStringLiteral("dependencies"))
        keys = {QStringLiteral("allowSelf")};
    else if (subject == QStringLiteral("S4") && context == QStringLiteral("compositionContracts"))
        keys = {QStringLiteral("multiplicityReason")};
    else if (subject == QStringLiteral("S4") && context == QStringLiteral("observations"))
        keys = {QStringLiteral("directPrivateAccess"), QStringLiteral("privateAccess")};
    else if (subject == QStringLiteral("S5") && context == QStringLiteral("assessments"))
        keys = {QStringLiteral("unrelatedConcerns"), QStringLiteral("unrelatedStateDomains"),
                QStringLiteral("unrelatedTestDomains"), QStringLiteral("relatedChangeReasons"),
                QStringLiteral("fragmentationSignal"), QStringLiteral("independentMeaning"),
                QStringLiteral("physicalBoundaryIds"), QStringLiteral("dependencyIds"),
                QStringLiteral("interfaceIds"), QStringLiteral("compositionIds")};
    else if (subject == QStringLiteral("S5") && context == QStringLiteral("candidateModules"))
        keys = {QStringLiteral("authoritativeS1ResponsibilityId")};
    for (const auto& key : keys) if (input.contains(key)) result.insert(key, input.value(key));
    return result;
}

QJsonValue semanticValue(const QJsonValue& value, const QString& subject, const QString& context = {})
{
    if (value.isObject()) {
        QJsonObject result;
        for (const auto& key : value.toObject().keys()) {
            const QString normalized = key.toLower();
            if (normalized == QStringLiteral("lifecycle")
                || normalized == QStringLiteral("lifecyclehistory")
                || normalized == QStringLiteral("certificate")
                || normalized == QStringLiteral("certificateid")
                || normalized == QStringLiteral("evidence")
                || normalized == QStringLiteral("evidencereferences")
                || normalized == QStringLiteral("generatedat")
                || normalized == QStringLiteral("issuedat")
                || normalized == QStringLiteral("timestamp")
                || normalized == QStringLiteral("freshness")
                || normalized == QStringLiteral("certification")
                || normalized == QStringLiteral("certification")) continue;
            if (normalized == QStringLiteral("metadata")) {
                const auto metadata = semanticMetadata(subject, context, value.toObject().value(key).toObject());
                if (!metadata.isEmpty()) result.insert(key, metadata);
                continue;
            }
            result.insert(key, semanticValue(value.toObject().value(key), subject, key));
        }
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        for (const auto& item : value.toArray()) result.append(semanticValue(item, subject, context));
        return result;
    }
    return value;
}

QJsonObject contractDescriptor(const QString& subject)
{
    const QJsonArray direct = QJsonArray::fromStringList(
        subject == QStringLiteral("P1") ? QStringList{QStringLiteral("F1")} :
        subject == QStringLiteral("P2") ? QStringList{QStringLiteral("P1")} :
        subject == QStringLiteral("P3") ? QStringList{QStringLiteral("P1"), QStringLiteral("P2")} :
        subject == QStringLiteral("P4") ? QStringList{QStringLiteral("P1")} :
        subject == QStringLiteral("P5") ? QStringList{QStringLiteral("P1"), QStringLiteral("P4")} : QStringList{});
    static const QMap<QString, QStringList> capabilities = {
        {QStringLiteral("F1"), {QStringLiteral("memory-evidence-foundation"), QStringLiteral("append-only-evidence"), QStringLiteral("project-memory")}},
        {QStringLiteral("P1"), {QStringLiteral("task-preflight"), QStringLiteral("task-contract"), QStringLiteral("postflight-evidence")}},
        {QStringLiteral("P2"), {QStringLiteral("context-indexing"), QStringLiteral("scoped-routing"), QStringLiteral("handoff-coordination")}},
        {QStringLiteral("P3"), {QStringLiteral("execution-orchestration"), QStringLiteral("task-state-transitions"), QStringLiteral("worker-coordination")}},
        {QStringLiteral("P4"), {QStringLiteral("predictive-optimization"), QStringLiteral("evidence-ranking"), QStringLiteral("risk-prediction")}},
        {QStringLiteral("P5"), {QStringLiteral("adaptive-routing"), QStringLiteral("route-governance"), QStringLiteral("route-health")}}
    };
    QJsonObject result{{QStringLiteral("contractVersion"), 2}, {QStringLiteral("subject"), subject},
                       {QStringLiteral("directDependencies"), direct}};
    result.insert(QStringLiteral("capabilities"), QJsonArray::fromStringList(capabilities.value(subject)));
    return result;
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
    QString value;
    if (!CertificationContractManifestProvider::fingerprint(subject, root, &value, error)) return {};
    return value;
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
            {QStringLiteral("sourceFresh"), sourceFresh}, {QStringLiteral("sourceBindingComplete"), sourceBindingComplete},
            {QStringLiteral("contractFresh"), contractFresh},
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
        subject == QStringLiteral("P4") ? QStringList{QStringLiteral("P1")} :
        subject == QStringLiteral("P5") ? QStringList{QStringLiteral("P1"), QStringLiteral("P4")} : QStringList{};
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

bool CertificationContractManifestProvider::fingerprint(const QString& subject, const QString& projectRoot,
                                                         QString* result, QString* error)
{
    if (error) error->clear();
    const auto project = readJson(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER.aramf.json")), error);
    if (project.isEmpty() && error && !error->isEmpty()) return false;
    return fingerprintFromProjectJson(subject, project, result, error);
}

bool CertificationContractManifestProvider::fingerprintFromProjectJson(const QString& subject, const QJsonObject& project,
                                                                        QString* result, QString* error)
{
    if (error) error->clear();
    QJsonObject projection;
    if (subject.startsWith(QLatin1Char('S'))) {
        static const QMap<QString, QString> keys = {
            {QStringLiteral("S1"), QStringLiteral("s1ResponsibilityOwnership")},
            {QStringLiteral("S2"), QStringLiteral("s2PhysicalStructure")},
            {QStringLiteral("S3"), QStringLiteral("s3DependencyInterfaces")},
            {QStringLiteral("S4"), QStringLiteral("s4CompositionEncapsulation")},
            {QStringLiteral("S5"), QStringLiteral("s5DecompositionModularity")},
            {QStringLiteral("S6"), QStringLiteral("s6StructuralEvolutionEnforcement")}
        };
        if (!keys.contains(subject)) {
            if (error) *error = QStringLiteral("Unknown Structure contract subject %1.").arg(subject);
            return false;
        }
        const auto structure = project.value(QStringLiteral("structure")).toObject();
        if (!structure.contains(keys.value(subject))) {
            if (error) *error = QStringLiteral("Project has no semantic contract for %1.").arg(subject);
            return false;
        }
        projection = semanticValue(structure.value(keys.value(subject)), subject).toObject();
        projection.insert(QStringLiteral("contractVersion"), 2);
        projection.insert(QStringLiteral("subject"), subject);
    } else if (subject == QStringLiteral("F1") || subject.startsWith(QLatin1Char('P'))) {
        projection = contractDescriptor(subject);
    } else {
        if (error) *error = QStringLiteral("Unknown certification contract subject %1.").arg(subject);
        return false;
    }
    if (result) *result = hashJson(projection);
    return true;
}

QJsonObject CertificationDependencyBindingProvider::currentBindings(const QString& subject, const QString& projectRoot,
                                                                     QString* error)
{
    if (error) error->clear();
    const auto manifest = CertificationFreshnessService::dependencyManifest(subject);
    QJsonObject result;
    for (const auto& dependency : manifest.directDependencies) {
        QString fingerprint;
        if (!CertificationContractManifestProvider::fingerprint(dependency.subject, projectRoot, &fingerprint, error)
            || fingerprint.isEmpty()) return {};
        result.insert(dependency.subject.toLower() + QStringLiteral("ContractFingerprint"), fingerprint);
    }
    result.insert(QStringLiteral("dependencyManifestFingerprint"), manifest.computedFingerprint());
    return result;
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
    result.sourceBindingComplete = !result.certifiedSourceFingerprint.isEmpty()
        && !result.currentSourceFingerprint.isEmpty();
    result.sourceFresh = result.sourceBindingComplete
        && result.certifiedSourceFingerprint == result.currentSourceFingerprint;
    result.contractFresh = !result.certifiedContractFingerprint.isEmpty()
        ? result.certifiedContractFingerprint == result.currentContractFingerprint : true;
    result.evidenceFresh = !result.certifiedEvidenceFingerprint.isEmpty()
        ? result.certifiedEvidenceFingerprint == result.currentEvidenceFingerprint : false;
    if (!result.sourceBindingComplete)
        result.reasons.append(QStringLiteral("Comparable canonical source-manifest binding is unavailable; repository revision is provenance only."));
    else if (!result.sourceFresh)
        result.reasons.append(QStringLiteral("Certified source manifest differs from the current source manifest."));
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
            if (historicalBinding.isEmpty() || historicalBinding != dependencyResult.currentContractFingerprint) {
                directFresh = false;
                result.reasons.append(QStringLiteral("Direct dependency %1 is stale or its contract binding differs.").arg(dependency.subject));
            } else {
                transitiveFresh = false;
                result.reasons.append(QStringLiteral("Direct dependency %1 is stale through its own dependency or source state.").arg(dependency.subject));
            }
        }
    }
    result.directDependencyFresh = directFresh;
    result.transitiveDependencyFresh = transitiveFresh;
    if (!result.bindingComplete) result.status = CertificationFreshnessStatus::DependencyBindingIncomplete;
    else if (!transitiveFresh) result.status = CertificationFreshnessStatus::TransitiveDependencyStale;
    else if (!directFresh) result.status = CertificationFreshnessStatus::DependencyStale;
    else if (!result.evidenceFresh) result.status = CertificationFreshnessStatus::EvidenceStale;
    else if (!result.sourceBindingComplete) result.status = CertificationFreshnessStatus::FreshnessIndeterminate;
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
    QJsonObject revalidation;
    QString revalidationError;
    const bool hasRevalidation = CertificationRevalidationService::latest(projectRoot, subject, &revalidation, &revalidationError);
    const QJsonObject certificate = hasRevalidation ? QJsonObject{} : latestCertificate(projectRoot, subject, error);
    if (!hasRevalidation && certificate.isEmpty()) {
        result.status = CertificationFreshnessStatus::NotCertified;
        result.reasons.append(QStringLiteral("No historical certificate exists for %1.").arg(subject));
        visiting.remove(subject);
        return result;
    }
    const QString evidenceReference = hasRevalidation
        ? revalidation.value(QStringLiteral("evidenceArtifact")).toString()
        : evidencePathFromCertificate(certificate);
    const QString evidenceFile = QDir(projectRoot).filePath(evidenceReference);
    const auto evidence = readJson(evidenceFile, error);
    if (evidence.isEmpty()) { visiting.remove(subject); return result; }
    QString sourceFingerprint;
    QString sourceError;
    CertificationSourceManifestProvider::fingerprint(projectRoot, subject, &sourceFingerprint, &sourceError);
    QString contractError;
    const QString contractFingerprint = currentContractFingerprint(subject, projectRoot, &contractError);
    if (error && error->isEmpty() && !contractError.isEmpty() && subject.startsWith(QLatin1Char('S'))) *error = contractError;
    QString currentRevision = currentGitRevision(error);
    const QString historicalSourceRevision = evidence.value(QStringLiteral("sourceRevision")).toString();
    auto historical = hasRevalidation
        ? QJsonObject{{QStringLiteral("certificateId"), revalidation.value(QStringLiteral("revalidationOfCertificateId"))},
                      {QStringLiteral("lifecycle"), revalidation.value(QStringLiteral("historicalLifecycle"))},
                      {QStringLiteral("sourceRevision"), revalidation.value(QStringLiteral("sourceRevision"))},
                      {QStringLiteral("sourceFingerprint"), revalidation.value(QStringLiteral("sourceFingerprint"))},
                      {QStringLiteral("contractFingerprint"), revalidation.value(QStringLiteral("contractFingerprint"))},
                      {QStringLiteral("evidenceFingerprint"), revalidation.value(QStringLiteral("evidenceFingerprint"))}}
        : QJsonObject{{QStringLiteral("certificateId"), certificate.value(QStringLiteral("certificateId"))},
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
