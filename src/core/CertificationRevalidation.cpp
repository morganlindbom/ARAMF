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
#include <QSet>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

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

QJsonValue canonicalOperationValue(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject result; const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) result.insert(it.key(), canonicalOperationValue(it.value()));
        return result;
    }
    if (value.isArray()) {
        QList<QByteArray> values;
        for (const auto& item : value.toArray()) values.append(QJsonDocument(QJsonArray{canonicalOperationValue(item)}).toJson(QJsonDocument::Compact));
        std::sort(values.begin(), values.end()); QJsonArray result;
        for (const auto& item : values) result.append(QJsonDocument::fromJson(item).array().first());
        return result;
    }
    return value;
}

// Only an exact sealed FAILED coordinator attempt may classify its own
// interrupted immutable pair. Unexplained corruption remains a hard failure.
bool retainedFailedAttempt(const QString& root, const QString& subject, const QString& revalidationId)
{
    const QDir operations(workerPath(root, "ARAMF_WORKER/certification/operations"));
    const auto hash = [&](const QString& relative) {
        QFile file(QDir(root).filePath(relative));
        if (!file.exists()) return QString("MISSING");
        return file.open(QIODevice::ReadOnly) ? hashBytes(file.readAll()) : QString("UNREADABLE");
    };
    const QString prefix = "ARAMF_WORKER/certification/revalidation/" + subject + '/' + revalidationId;
    for (const auto& operationId : operations.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!QRegularExpression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(operationId).hasMatch()) continue;
        if (revalidationId != "reval-" + QUuid::createUuidV5(QUuid(operationId), ("revalidate-current/v1/" + subject).toUtf8()).toString(QUuid::WithoutBraces)) continue;
        const QString operationPrefix = "ARAMF_WORKER/certification/operations/" + operationId;
        const bool recovered = QFileInfo::exists(operations.filePath(operationId + "/recovery.json"));
        const QString receiptName = recovered ? "recovery.json" : "receipt.json";
        const QString sealName = recovered ? "recovery-seal.json" : "seal.json";
        const auto receipt = readObject(operations.filePath(operationId + '/' + receiptName), nullptr);
        const auto seal = readObject(operations.filePath(operationId + '/' + sealName), nullptr);
        const auto proof = readObject(operations.filePath(operationId + "/software-evidence.json"), nullptr);
        const auto contract = proof.value("operationContract").toObject();
        auto unsignedContract = contract; unsignedContract.remove("contractId");
        const auto intent = contract.value("intent").toObject();
        const QString contractId = contract.value("contractId").toString();
        if (contract.value("contractKind") != "canonical-certification-operation/v1" || contractId.isEmpty()
            || hashBytes(QJsonDocument(canonicalOperationValue(unsignedContract).toObject()).toJson(QJsonDocument::Compact)) != contractId
            || !contract.value("preflight").toObject().value("status").toString().startsWith("READY")
            || contract.value("operationId") != operationId || intent.value("kind") != "revalidate-current/v1"
            || !intent.value("subjects").toArray().contains(subject)
            || contract.value("projectBinding").toObject().value("projectRoot") != QFileInfo(root).canonicalFilePath()
            || receipt.value("contractId") != contractId || seal.value("contractId") != contractId
            || receipt.value("operationId") != operationId || seal.value("operationId") != operationId
            || receipt.value("outcome") != "FAILED" || receipt.value("error").toString().isEmpty()
            || receipt.value("producer") != "WorkerTaskServices/certification-operation/v1"
            || seal.value("producer") != receipt.value("producer") || proof.value("producer") != receipt.value("producer")
            || seal.value("receiptFingerprint") != hash(operationPrefix + '/' + receiptName)
            || receipt.value("outputs").toObject().value(operationPrefix + "/software-evidence.json") != hash(operationPrefix + "/software-evidence.json")) continue;
        if (recovered && (!contract.value("permittedFiles").toArray().contains(operationPrefix + '/' + receiptName)
            || !contract.value("permittedFiles").toArray().contains(operationPrefix + '/' + sealName)
            || receipt.value("outputs").toObject().value(operationPrefix + "/receipt.json") != hash(operationPrefix + "/receipt.json")
            || receipt.value("outputs").toObject().value(operationPrefix + "/seal.json") != hash(operationPrefix + "/seal.json"))) continue;
        bool exact = true;
        for (const auto& name : QStringList{"evidence.json", "revalidation.json"}) {
            const QString path = prefix + '/' + name;
            exact &= contract.value("permittedFiles").toArray().contains(path)
                && contract.value("baseline").toObject().value(path).toString("MISSING") == "MISSING"
                && receipt.value("outputs").toObject().value(path) == hash(path);
        }
        if (exact) return true;
    }
    return false;
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
    const QString path = QDir(root).filePath(reference);
    const QString canonical = QFileInfo(path).canonicalFilePath(), canonicalRoot = QFileInfo(root).canonicalFilePath();
    QFile file(path);
    if (canonical.isEmpty() || !canonical.startsWith(canonicalRoot + '/', Qt::CaseInsensitive) || !file.open(QIODevice::ReadOnly)) {
        if (error) *error = "Historical evidence is missing or outside this project.";
        return {};
    }
    const auto bytes = file.readAll();
    const QString actual = hashBytes(bytes);
    bool bound = false;
    for (const auto& value : certificate.value("evidenceReferences").toArray()) {
        const auto entry = value.toObject();
        if (entry.value("reference") != reference) continue;
        if (entry.value("fingerprint") != actual) {
            if (error) *error = "Historical evidence bytes differ from their certificate reference.";
            return {};
        }
        bound = true;
    }
    if (certificate.value("evidenceArtifact") == reference && certificate.contains("evidenceFingerprint")) {
        if (certificate.value("evidenceFingerprint") != actual) {
            if (error) *error = "Historical evidence bytes differ from the sealed certificate.";
            return {};
        }
        bound = true;
    }
    if (!bound) { if (error) *error = "Historical evidence has no exact certificate fingerprint."; return {}; }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = "Historical evidence JSON is malformed.";
        return {};
    }
    return document.object();
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
    if (QStringList{"F1", "F2", "F3", "F4"}.contains(subject)) {
        const auto history = project.value(QStringLiteral("processVersion")).toObject()
            .value(QStringLiteral("completedHistory")).toArray();
        int bestIteration = 0;
        int bestLoop = 1;
        for (const auto& value : history) {
            const auto entry = value.toObject();
            if (entry.value(QStringLiteral("foundation")).toInt() == subject.mid(1).toInt()
                && entry.value(QStringLiteral("certification")).toInt() == 1
                && entry.value(QStringLiteral("done")).toInt() == 1
                && entry.value(QStringLiteral("iteration")).toInt() >= bestIteration) {
                bestIteration = entry.value(QStringLiteral("iteration")).toInt();
                bestLoop = entry.value(QStringLiteral("loop")).toInt(1);
            }
        }
        if (bestIteration > 0) return QStringLiteral("%1.%2.%3.1.1").arg(subject).arg(bestLoop).arg(bestIteration);
        const QString foundationVersion = historicalEvidence.value(QStringLiteral("foundationVersion")).toString();
        const int iteration = historicalEvidence.value(QStringLiteral("iteration")).toInt();
        if (subject == QStringLiteral("F1") && !foundationVersion.isEmpty() && iteration > 0)
            return QStringLiteral("F1.1.%1.1.1").arg(iteration);
    }
    if (subject.startsWith(QLatin1Char('P'))) {
        bool ok = false;
        const int process = subject.mid(1).toInt(&ok);
        if (ok && process > 0) {
            const auto history = project.value(QStringLiteral("processVersion")).toObject()
                .value(QStringLiteral("completedHistory")).toArray();
            int bestIteration = 0;
            int bestLoop = 1;
            for (const auto& value : history) {
                const auto entry = value.toObject();
                if (entry.value(QStringLiteral("process")).toInt() == process
                    && entry.value(QStringLiteral("certification")).toInt() == 1
                    && entry.value(QStringLiteral("done")).toInt() == 1
                    && entry.value(QStringLiteral("iteration")).toInt() >= bestIteration) {
                    bestIteration = entry.value(QStringLiteral("iteration")).toInt();
                    bestLoop = entry.value(QStringLiteral("loop")).toInt(1);
                }
            }
            if (bestIteration > 0) return QStringLiteral("%1.%2.%3.1.1").arg(subject).arg(bestLoop).arg(bestIteration);
        }
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
    return subject == QStringLiteral("F1") || subject == QStringLiteral("F2")
        || subject == QStringLiteral("F3") || subject == QStringLiteral("F4") || subject == QStringLiteral("S1")
        || subject == QStringLiteral("S2") || subject == QStringLiteral("S3")
        || subject == QStringLiteral("S4") || subject == QStringLiteral("S5")
        || subject == QStringLiteral("S6") || subject == QStringLiteral("P1")
        || subject == QStringLiteral("P2") || subject == QStringLiteral("P3")
        || subject == QStringLiteral("P4") || subject == QStringLiteral("P5");
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
        const bool valid = record.value("schemaVersion").toInt() == 1 && record.value("subject") == subject
            && record.value("revalidationId") == entry && record.value("revalidationType") == "FRESHNESS_REVALIDATION"
            && record.value("status") == "FRESH" && !record.value("revalidationOfCertificateId").toString().isEmpty()
            && QDateTime::fromString(record.value("timestamp").toString(), Qt::ISODateWithMs).isValid()
            && record.value("evidenceArtifact") == "ARAMF_WORKER/certification/revalidation/" + subject + '/' + entry + "/evidence.json"
            && QRegularExpression("^[a-f0-9]{64}$").match(record.value("evidenceFingerprint").toString()).hasMatch();
        if (!valid) {
            if (!retainedFailedAttempt(projectRoot, subject, entry)) {
                if (error && error->isEmpty()) *error = "Malformed or unclassified revalidation history: " + subject + '/' + entry;
                return false;
            }
            if (error) error->clear();
            continue;
        }
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

bool CertificationRevalidationService::validateCurrentSource(const QString& root, const QString& subject,
                                                             const QString& revision, QString* error)
{
    const auto run = [&](const QStringList& arguments, QByteArray* output = nullptr) {
        QProcess process; process.setWorkingDirectory(root); process.start("git", arguments);
        if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode()) return false;
        if (output) *output = process.readAllStandardOutput(); return true;
    };
    QByteArray resolved, tracked;
    const auto files = CertificationSourceManifestProvider::manifest(subject).files;
    bool valid = revision.size() == 40 && !files.isEmpty() && run({"rev-parse", revision + "^{commit}"}, &resolved)
        && QString::fromUtf8(resolved).trimmed() == revision
        && run(QStringList{"ls-tree", "-r", "--name-only", revision, "--"} + files, &tracked);
    for (const auto& file : files) valid &= QString::fromUtf8(tracked).split('\n').contains(file);
    valid &= run(QStringList{"diff", "--quiet", revision, "--"} + files);
    if (!valid && error) *error = "Current source manifest is not bound to an unchanged committed implementation.";
    return valid;
}

bool CertificationRevalidationService::validateHistoricalSource(const QString& root, const QJsonObject& certificate,
                                                                const QJsonObject& suppliedSnapshot, QString* error)
{
    const auto fail = [&](const QString& message) { if (error) *error = message; return false; };
    const auto read = [&](const QString& path) { QFile file(QDir(root).filePath(path)); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{}; };
    const auto git = [&](const QStringList& arguments, QByteArray* output) {
        QProcess process; process.setWorkingDirectory(root); process.start("git", arguments);
        if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode()) return false;
        *output = process.readAllStandardOutput(); return true;
    };
    const QString artifact = certificate.value("evidenceArtifact").toString(), revision = certificate.value("sourceRevision").toString();
    const auto bytes = read(artifact); const auto evidence = QJsonDocument::fromJson(bytes).object();
    QByteArray resolved, tree;
    if (artifact.isEmpty() || certificate.value("result") != "PASS" || certificate.value("certificationStatus") != "CERTIFIED"
        || !certificate.value("evidenceComplete").toBool() || hashBytes(bytes) != certificate.value("evidenceFingerprint")
        || revision.size() != 40 || !git({"rev-parse", revision + "^{commit}"}, &resolved)
        || QString::fromUtf8(resolved).trimmed() != revision || evidence.value("sourceRevision") != revision
        || !git({"ls-tree", "-r", "--name-only", revision}, &tree)) return fail("Historical certificate/evidence/revision binding is invalid.");
    const auto snapshot = evidence.contains("sourceSnapshot") ? evidence.value("sourceSnapshot").toObject() : suppliedSnapshot;
    if (snapshot.value("sourceRevision") != revision || snapshot.value("sourceFingerprint") != evidence.value("sourceFingerprint")
        || snapshot.value("files").toArray().isEmpty()) return fail("Missing historical source witness.");
    const auto tracked = QString::fromUtf8(tree).split('\n'); QSet<QString> paths; QByteArray material;
    for (const auto& value : snapshot.value("files").toArray()) {
        const auto file = value.toObject(); const QString path = file.value("path").toString();
        const QByteArray encoded = file.value("bytesBase64").toString().toLatin1(), data = QByteArray::fromBase64(encoded);
        if (path.isEmpty() || path.contains(':') || path.contains('\\') || path.contains(QChar(0)) || QDir::isAbsolutePath(path)
            || QDir::cleanPath(path) != path || path.startsWith("../") || !tracked.contains(path) || paths.contains(path)
            || data.toBase64() != encoded) return fail("Invalid or duplicate historical source path.");
        QByteArray committed; if (!git({"show", revision + ':' + path}, &committed)) return fail("Missing historical source blob.");
        auto normalized = data; normalized.replace("\r\n", "\n"); committed.replace("\r\n", "\n");
        if (normalized != committed) return fail("Historical source differs from its Git revision: " + path);
        paths.insert(path); material.append(path.toUtf8()); material.append('\0'); material.append(data); material.append('\0');
    }
    if (hashBytes(material) != evidence.value("sourceFingerprint")) return fail("Historical witness differs from sealed source fingerprint.");
    for (const auto& value : evidence.value("checks").toArray()) {
        const auto check = value.toObject();
        const QString reference = check.value("reference").toString();
        const QString canonical = QFileInfo(QDir(root).filePath(reference)).canonicalFilePath();
        const QString boundary = QFileInfo(QDir(root).filePath(QFileInfo(artifact).path())).canonicalFilePath();
        if (boundary.isEmpty() || canonical.isEmpty() || !canonical.startsWith(boundary + '/')
            || check.value("sourceRevision") != revision || check.value("lifecycle") != evidence.value("lifecycle")
            || check.value("status") != "PASS" || check.value("exitCode").toInt(-1) != 0
            || hashBytes(read(reference)) != check.value("fingerprint")) return fail("Historical check evidence is missing or altered.");
    }
    return true;
}

bool CertificationRevalidationService::revalidate(const QString& projectRoot, const QString& subject,
                                                  const QJsonObject& regressionEvidence,
                                                  CertificationRevalidationResult* result, QString* error,
                                                  const QString& plannedId, const QJsonObject& operationBinding)
{
    if (error) error->clear();
    if ((!plannedId.isEmpty() && (!QRegularExpression("^reval-[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(plannedId).hasMatch()
            || operationBinding.value("subject") != subject
            || !QRegularExpression("^[a-f0-9]{64}$").match(operationBinding.value("contractId").toString()).hasMatch()))
        || (plannedId.isEmpty() && !operationBinding.isEmpty())) {
        if (error) *error = "Invalid planned revalidation identity or operation binding.";
        return false;
    }
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
    QJsonObject historicalSourceSnapshot;
    if (QStringList{"F2", "F3", "F4"}.contains(subject)) {
        historicalSourceSnapshot = historicalEvidence.value("sourceSnapshot").toObject();
        if (historicalSourceSnapshot.isEmpty()) historicalSourceSnapshot = regressionEvidence.value("historicalSourceSnapshot").toObject();
        if (historicalSourceSnapshot.isEmpty()) {
            QJsonObject previous;
            if (latest(projectRoot, subject, &previous, error)) {
                const QString reference = previous.value("evidenceArtifact").toString();
                QFile previousBytes(QDir(projectRoot).filePath(reference));
                if (!previousBytes.open(QIODevice::ReadOnly)
                    || hashBytes(previousBytes.readAll()) != previous.value("evidenceFingerprint").toString()) {
                    if (error) *error = "Previous revalidation evidence is missing or altered.";
                    return false;
                }
                historicalSourceSnapshot = readObject(QDir(projectRoot).filePath(reference), error).value("historicalSourceSnapshot").toObject();
            }
        }
        if (!validateHistoricalSource(projectRoot, certificate, historicalSourceSnapshot, error)
            || !validateCurrentSource(projectRoot, subject, sourceRevision, error)) return false;
    }
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
    normalizedRegressionEvidence.remove(QStringLiteral("historicalSourceSnapshot"));
    normalizedRegressionEvidence.insert(QStringLiteral("implementationRevision"), sourceRevision);
    const QString revalidationId = plannedId.isEmpty()
        ? QStringLiteral("reval-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)) : plannedId;
    const QString plannedDirectory = workerPath(projectRoot, AramfPaths::CertificationRevalidationDirectory + '/' + subject + '/' + revalidationId);
    if (QFileInfo::exists(plannedDirectory)
        || (!plannedId.isEmpty() && (operationBinding.value("sourceFingerprint") != sourceFingerprint
            || operationBinding.value("contractFingerprint") != contractFingerprint))) {
        if (error) *error = "Planned revalidation already exists or its source/contract binding changed.";
        return false;
    }
    const auto writeImmutable = [&](const QString& path, const QByteArray& bytes) {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(bytes) != bytes.size()) {
            if (error) *error = file.errorString();
            return false;
        }
        file.close();
        return file.open(QIODevice::ReadOnly) && file.readAll() == bytes;
    };
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
        {QStringLiteral("semanticChange"), regressionEvidence.value(QStringLiteral("semanticChange")).toBool()},
        {QStringLiteral("status"), QStringLiteral("FRESH")},
        {QStringLiteral("regressionEvidence"), normalizedRegressionEvidence},
        {QStringLiteral("provenance"), QJsonObject{{QStringLiteral("sourceBinding"), QStringLiteral("CertificationSourceManifestProvider")},
                                                    {QStringLiteral("contractBinding"), QStringLiteral("CertificationContractManifestProvider")},
                                                    {QStringLiteral("dependencyBinding"), QStringLiteral("CertificationDependencyBindingProvider")}}},
        {QStringLiteral("timestamp"), timestamp}};
    if (!historicalSourceSnapshot.isEmpty()) evidence.insert("historicalSourceSnapshot", historicalSourceSnapshot);
    if (!operationBinding.isEmpty()) evidence.insert("operationBinding", operationBinding);
    const auto normalizedBindings = bindingsForEvidence(dependencyBindings);
    for (auto it = normalizedBindings.begin(); it != normalizedBindings.end(); ++it) evidence.insert(it.key(), it.value());
    const QByteArray evidenceBytes = QJsonDocument(evidence).toJson(QJsonDocument::Compact) + '\n';
    const QString evidenceRelative = AramfPaths::CertificationRevalidationDirectory + QLatin1Char('/') + subject
        + QLatin1Char('/') + revalidationId + QStringLiteral("/evidence.json");
    const QString evidencePath = QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(evidenceRelative));
    if (!writeImmutable(evidencePath, evidenceBytes)) return false;
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
        {QStringLiteral("semanticChange"), regressionEvidence.value(QStringLiteral("semanticChange")).toBool()},
        {QStringLiteral("status"), QStringLiteral("FRESH")},
        {QStringLiteral("timestamp"), timestamp},
        {QStringLiteral("provenance"), QStringLiteral("agent-direct")}};
    const QString recordRelative = AramfPaths::CertificationRevalidationDirectory + QLatin1Char('/') + subject
        + QLatin1Char('/') + revalidationId + QStringLiteral("/revalidation.json");
    if (!operationBinding.isEmpty()) record.insert("operationBinding", operationBinding);
    if (!writeImmutable(QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(recordRelative)),
                    QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n')) return false;

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
