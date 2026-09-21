#include "CertificationRevalidation.h"

#include "AramfPaths.h"
#include "CertificationFreshness.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QSaveFile>
#include <QUuid>

namespace {
QString workerPath(const QString& root, const QString& relative)
{
    return QDir(root).filePath(AramfPaths::resolveWorkerRelativePath(relative));
}

QString hashBytes(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QJsonObject readObject(const QString& filePath, QString* error)
{
    QFile file(filePath);
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

bool writeBytes(const QString& filePath, const QByteArray& bytes, QString* error)
{
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) {
        if (error) *error = QStringLiteral("Could not create revalidation evidence directory.");
        return false;
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool gitRevision(const QString& root, QString* revision, QString* error)
{
    QProcess git;
    git.setWorkingDirectory(root);
    git.start(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    if (!git.waitForFinished(10000) || git.exitCode() != 0) {
        if (error) *error = QStringLiteral("Unable to resolve the committed implementation revision for revalidation.");
        return false;
    }
    if (revision) *revision = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    return !revision || !revision->isEmpty();
}

QJsonObject latestHistoricalCertificate(const QString& root, const QString& subject, QString* error)
{
    QFile file(workerPath(root, AramfPaths::Certificates));
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
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) *error = QStringLiteral("Malformed certification history: %1").arg(parseError.errorString());
            return {};
        }
        if (document.object().value(QStringLiteral("subject")).toString() == subject) latest = document.object();
    }
    if (latest.isEmpty() && error) *error = QStringLiteral("No historical certificate exists for %1.").arg(subject);
    return latest;
}

QString evidenceReference(const QJsonObject& certificate)
{
    for (const auto& value : certificate.value(QStringLiteral("evidenceReferences")).toArray()) {
        const QString reference = value.toObject().value(QStringLiteral("reference")).toString();
        if (!reference.trimmed().isEmpty()) return reference;
    }
    return certificate.value(QStringLiteral("evidenceArtifact")).toString();
}

QJsonObject readHistoricalEvidence(const QString& root, const QJsonObject& certificate, QString* error)
{
    const QString reference = evidenceReference(certificate);
    if (reference.isEmpty()) {
        if (error) *error = QStringLiteral("Historical certificate has no evidence reference.");
        return {};
    }
    return readObject(QDir(root).filePath(reference), error);
}

QString completedLifecycleFromProject(const QString& root, const QString& subject, const QJsonObject& historicalEvidence)
{
    QString readError;
    const auto project = readObject(QDir(root).filePath(QStringLiteral("ARAMF_WORKER.aramf.json")), &readError);
    if (subject.startsWith(QLatin1Char('S'))) {
        static const QMap<QString, QString> keys = {
            {QStringLiteral("S1"), QStringLiteral("s1ResponsibilityOwnership")},
            {QStringLiteral("S2"), QStringLiteral("s2PhysicalStructure")},
            {QStringLiteral("S3"), QStringLiteral("s3DependencyInterfaces")},
            {QStringLiteral("S4"), QStringLiteral("s4CompositionEncapsulation")},
            {QStringLiteral("S5"), QStringLiteral("s5DecompositionModularity")},
            {QStringLiteral("S6"), QStringLiteral("s6StructuralEvolutionEnforcement")}};
        const auto lifecycle = project.value(QStringLiteral("structure")).toObject()
            .value(keys.value(subject)).toObject().value(QStringLiteral("lifecycle")).toObject();
        if (!lifecycle.isEmpty()) {
            return QStringLiteral("%1.%2.%3.%4.%5").arg(subject)
                .arg(lifecycle.value(QStringLiteral("loop")).toInt())
                .arg(lifecycle.value(QStringLiteral("iteration")).toInt())
                .arg(lifecycle.value(QStringLiteral("certification")).toInt())
                .arg(lifecycle.value(QStringLiteral("done")).toInt());
        }
    }
    if (subject == QStringLiteral("F1")) {
        const auto history = project.value(QStringLiteral("processVersion")).toObject()
            .value(QStringLiteral("completedHistory")).toArray();
        int bestIteration = 0;
        int bestLoop = 1;
        for (const auto& value : history) {
            const auto entry = value.toObject();
            if (entry.value(QStringLiteral("foundation")).toInt() == 1
                && entry.value(QStringLiteral("certification")).toInt() == 1
                && entry.value(QStringLiteral("done")).toInt() == 1
                && entry.value(QStringLiteral("iteration")).toInt() >= bestIteration) {
                bestIteration = entry.value(QStringLiteral("iteration")).toInt();
                bestLoop = entry.value(QStringLiteral("loop")).toInt(1);
            }
        }
        if (bestIteration > 0) return QStringLiteral("F1.%1.%2.1.1").arg(bestLoop).arg(bestIteration);
        const QString foundationVersion = historicalEvidence.value(QStringLiteral("foundationVersion")).toString();
        const int iteration = historicalEvidence.value(QStringLiteral("iteration")).toInt();
        if (!foundationVersion.isEmpty() && iteration > 0) return QStringLiteral("F1.1.%1.1.1").arg(iteration);
    }
    return historicalEvidence.value(QStringLiteral("lifecycle")).toString();
}

QJsonObject bindingsForEvidence(const QJsonObject& bindings)
{
    QJsonObject normalized = bindings;
    normalized.insert(QStringLiteral("dependencyBindings"), bindings);
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        if (it.key() != QStringLiteral("dependencyManifestFingerprint")) normalized.insert(it.key(), it.value());
    }
    return normalized;
}

bool isAllowedSubject(const QString& subject)
{
    return subject == QStringLiteral("F1") || subject == QStringLiteral("S1")
        || subject == QStringLiteral("S2") || subject == QStringLiteral("S3")
        || subject == QStringLiteral("S4") || subject == QStringLiteral("S5")
        || subject == QStringLiteral("S6");
}
}

QJsonObject CertificationRevalidationResult::toJson() const
{
    return {{QStringLiteral("subject"), subject},
            {QStringLiteral("revalidationId"), revalidationId},
            {QStringLiteral("revalidationOfCertificateId"), revalidationOfCertificateId},
            {QStringLiteral("historicalLifecycle"), historicalLifecycle},
            {QStringLiteral("evidenceArtifact"), evidenceArtifact},
            {QStringLiteral("evidenceFingerprint"), evidenceFingerprint},
            {QStringLiteral("sourceRevision"), sourceRevision},
            {QStringLiteral("sourceFingerprint"), sourceFingerprint},
            {QStringLiteral("contractFingerprint"), contractFingerprint},
            {QStringLiteral("dependencyManifestFingerprint"), dependencyManifestFingerprint},
            {QStringLiteral("status"), status}};
}

bool CertificationRevalidationService::latest(const QString& projectRoot, const QString& subject,
                                              QJsonObject* result, QString* error)
{
    if (error) error->clear();
    const QString subjectDirectory = QDir(projectRoot).filePath(
        AramfPaths::resolveWorkerRelativePath(AramfPaths::CertificationRevalidationDirectory + QLatin1Char('/') + subject));
    QDir directory(subjectDirectory);
    const auto directories = directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QJsonObject latestRecord;
    QString latestTimestamp;
    QString latestId;
    for (const auto& entry : directories) {
        const auto record = readObject(directory.filePath(entry + QStringLiteral("/revalidation.json")), error);
        if (record.isEmpty()) return false;
        const QString timestamp = record.value(QStringLiteral("timestamp")).toString();
        const QString id = record.value(QStringLiteral("revalidationId")).toString();
        if (latestRecord.isEmpty() || timestamp > latestTimestamp || (timestamp == latestTimestamp && id > latestId)) {
            latestRecord = record;
            latestTimestamp = timestamp;
            latestId = id;
        }
    }
    if (result) *result = latestRecord;
    return !latestRecord.isEmpty();
}

bool CertificationRevalidationService::revalidate(const QString& projectRoot, const QString& subject,
                                                  const QJsonObject& regressionEvidence,
                                                  CertificationRevalidationResult* result, QString* error)
{
    if (error) error->clear();
    if (!isAllowedSubject(subject)) {
        if (error) *error = QStringLiteral("Freshness revalidation is not enabled for subject %1.").arg(subject);
        return false;
    }
    if (regressionEvidence.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")) {
        if (error) *error = QStringLiteral("Freshness revalidation requires PASS regression evidence.");
        return false;
    }

    QJsonObject certificate = latestHistoricalCertificate(projectRoot, subject, error);
    if (certificate.isEmpty()) return false;
    QJsonObject historicalEvidence = readHistoricalEvidence(projectRoot, certificate, error);
    if (historicalEvidence.isEmpty()) return false;

    for (const auto& dependency : CertificationFreshnessService::dependencyManifest(subject).directDependencies) {
        QString dependencyError;
        const auto dependencyResult = CertificationFreshnessService::evaluate(dependency.subject, projectRoot, &dependencyError);
        if (dependencyResult.status != CertificationFreshnessStatus::Fresh) {
            if (error) *error = QStringLiteral("Cannot revalidate %1 before direct dependency %2 is FRESH: %3")
                .arg(subject, dependency.subject, dependencyResult.reasons.join(QStringLiteral("; ")));
            return false;
        }
    }

    QString sourceRevision;
    if (!gitRevision(projectRoot, &sourceRevision, error)) return false;
    QString sourceFingerprint;
    if (!CertificationSourceManifestProvider::fingerprint(projectRoot, subject, &sourceFingerprint, error)) return false;
    QString contractFingerprint;
    if (!CertificationContractManifestProvider::fingerprint(subject, projectRoot, &contractFingerprint, error)) return false;
    const auto manifest = CertificationFreshnessService::dependencyManifest(subject);
    const auto dependencyBindings = CertificationDependencyBindingProvider::currentBindings(subject, projectRoot, error);
    if (!manifest.directDependencies.isEmpty() && dependencyBindings.isEmpty()) return false;

    QJsonObject normalizedRegressionEvidence = regressionEvidence;
    normalizedRegressionEvidence.remove(QStringLiteral("implementationRevision"));
    normalizedRegressionEvidence.remove(QStringLiteral("sourceRevision"));
    normalizedRegressionEvidence.insert(QStringLiteral("implementationRevision"), sourceRevision);
    const QString revalidationId = QStringLiteral("reval-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    const QString historicalLifecycle = completedLifecycleFromProject(projectRoot, subject, historicalEvidence);
    QJsonObject previousRevalidation;
    CertificationRevalidationService::latest(projectRoot, subject, &previousRevalidation, nullptr);
    QJsonObject evidence{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("revalidationType"), QStringLiteral("FRESHNESS_REVALIDATION")},
        {QStringLiteral("revalidationId"), revalidationId},
        {QStringLiteral("subject"), subject},
        {QStringLiteral("revalidationOfCertificateId"), certificate.value(QStringLiteral("certificateId"))},
        {QStringLiteral("supersedesRevalidationId"), previousRevalidation.value(QStringLiteral("revalidationId"))},
        {QStringLiteral("historicalLifecycle"), historicalLifecycle},
        {QStringLiteral("sourceRevision"), sourceRevision},
        {QStringLiteral("sourceFingerprint"), sourceFingerprint},
        {QStringLiteral("contractFingerprint"), contractFingerprint},
        {QStringLiteral("contractProjectionVersion"), 2},
        {QStringLiteral("dependencyManifestFingerprint"), manifest.computedFingerprint()},
        {QStringLiteral("semanticChange"), false},
        {QStringLiteral("status"), QStringLiteral("FRESH")},
        {QStringLiteral("regressionEvidence"), normalizedRegressionEvidence},
        {QStringLiteral("provenance"), QJsonObject{{QStringLiteral("sourceBinding"), QStringLiteral("CertificationSourceManifestProvider")},
                                                    {QStringLiteral("contractBinding"), QStringLiteral("CertificationContractManifestProvider")},
                                                    {QStringLiteral("dependencyBinding"), QStringLiteral("CertificationDependencyBindingProvider")}}},
        {QStringLiteral("timestamp"), timestamp}};
    const auto normalizedBindings = bindingsForEvidence(dependencyBindings);
    for (auto it = normalizedBindings.begin(); it != normalizedBindings.end(); ++it) evidence.insert(it.key(), it.value());
    const QByteArray evidenceBytes = QJsonDocument(evidence).toJson(QJsonDocument::Compact) + '\n';
    const QString evidenceRelative = AramfPaths::CertificationRevalidationDirectory + QLatin1Char('/') + subject
        + QLatin1Char('/') + revalidationId + QStringLiteral("/evidence.json");
    const QString evidencePath = QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(evidenceRelative));
    if (!writeBytes(evidencePath, evidenceBytes, error)) return false;
    QFile persistedEvidence(evidencePath);
    if (!persistedEvidence.open(QIODevice::ReadOnly)) {
        if (error) *error = persistedEvidence.errorString();
        return false;
    }
    const QString evidenceFingerprint = hashBytes(persistedEvidence.readAll());

    QJsonObject record{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("revalidationType"), QStringLiteral("FRESHNESS_REVALIDATION")},
        {QStringLiteral("revalidationId"), revalidationId},
        {QStringLiteral("subject"), subject},
        {QStringLiteral("revalidationOfCertificateId"), certificate.value(QStringLiteral("certificateId"))},
        {QStringLiteral("supersedesRevalidationId"), previousRevalidation.value(QStringLiteral("revalidationId"))},
        {QStringLiteral("historicalLifecycle"), historicalLifecycle},
        {QStringLiteral("evidenceArtifact"), evidenceRelative},
        {QStringLiteral("evidenceFingerprint"), evidenceFingerprint},
        {QStringLiteral("sourceRevision"), sourceRevision},
        {QStringLiteral("sourceFingerprint"), sourceFingerprint},
        {QStringLiteral("contractFingerprint"), contractFingerprint},
        {QStringLiteral("contractProjectionVersion"), 2},
        {QStringLiteral("dependencyManifestFingerprint"), manifest.computedFingerprint()},
        {QStringLiteral("dependencyBindings"), dependencyBindings},
        {QStringLiteral("semanticChange"), false},
        {QStringLiteral("status"), QStringLiteral("FRESH")},
        {QStringLiteral("timestamp"), timestamp},
        {QStringLiteral("provenance"), QStringLiteral("agent-direct")}};
    const QString recordRelative = AramfPaths::CertificationRevalidationDirectory + QLatin1Char('/') + subject
        + QLatin1Char('/') + revalidationId + QStringLiteral("/revalidation.json");
    if (!writeBytes(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(recordRelative)),
                    QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n', error)) return false;

    QJsonObject currentState;
    const QString currentStatePath = workerPath(projectRoot, AramfPaths::CurrentFreshnessState);
    if (QFileInfo::exists(currentStatePath)) currentState = readObject(currentStatePath, error);
    QJsonObject subjects = currentState.value(QStringLiteral("subjects")).toObject();
    subjects.insert(subject, record);
    currentState = QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("subjects"), subjects}};
    if (!writeBytes(currentStatePath, QJsonDocument(currentState).toJson(QJsonDocument::Indented), error)) return false;

    if (result) {
        result->subject = subject;
        result->revalidationId = revalidationId;
        result->revalidationOfCertificateId = certificate.value(QStringLiteral("certificateId")).toString();
        result->historicalLifecycle = historicalLifecycle;
        result->evidenceArtifact = evidenceRelative;
        result->evidenceFingerprint = evidenceFingerprint;
        result->sourceRevision = sourceRevision;
        result->sourceFingerprint = sourceFingerprint;
        result->contractFingerprint = contractFingerprint;
        result->dependencyManifestFingerprint = manifest.computedFingerprint();
        result->status = QStringLiteral("FRESH");
    }
    return true;
}
