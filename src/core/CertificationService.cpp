#include "CertificationService.h"

#include "AramfPaths.h"
#include "ProjectMemory.h"
#include "ProjectModel.h"

#include <QDateTime>
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
QString path(const QString& root, const QString& relative) { return QDir(root).filePath(relative); }

QJsonObject readObject(const QString& filePath, QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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

bool writeObject(const QString& filePath, QJsonObject value, QString* error)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    value.insert(QStringLiteral("_file"), QFileInfo(filePath).fileName());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(QJsonDocument(value).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QList<QJsonObject> readCertificates(const QString& filePath, QString* error)
{
    QList<QJsonObject> result;
    QFile file(filePath);
    if (!file.exists()) return result;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return {};
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) *error = QStringLiteral("Malformed certification JSONL: %1").arg(parseError.errorString());
            return {};
        }
        const auto certificate = document.object();
        const QString certificateId = certificate.value(QStringLiteral("certificateId")).toString();
        if (certificateId.isEmpty() || std::any_of(result.cbegin(), result.cend(), [&certificateId](const QJsonObject& prior) {
                return prior.value(QStringLiteral("certificateId")).toString() == certificateId;
            })) {
            if (error) *error = QStringLiteral("Certification history contains a missing or duplicate certificate ID.");
            return {};
        }
        result.append(certificate);
    }
    return result;
}

bool isPass(const QString& status) { return status == QStringLiteral("PASS"); }
bool isKnownLevel(const QString& level)
{
    return QStringList{QStringLiteral("BUILD_ONLY"), QStringLiteral("HOST_TEST"), QStringLiteral("SIMULATED"),
            QStringLiteral("RUNTIME"), QStringLiteral("ON_TARGET"), QStringLiteral("PHYSICAL"),
            QStringLiteral("GUI_END_TO_END"), QStringLiteral("HARDWARE_CERTIFIED")}.contains(level);
}
}

bool CertificationService::initialize(const QString& projectRoot, const ProjectModel* model, QString* error) const
{
    if (!model || !model->certificationConfiguration().enabled) return true;
    if (!QDir(projectRoot).mkpath(AramfPaths::CertificationEvidenceDirectory)
        || !QFileInfo::exists(path(projectRoot, AramfPaths::CertificationDirectory))) {
        if (!QDir(projectRoot).mkpath(AramfPaths::CertificationDirectory)
            || !QDir(projectRoot).mkpath(AramfPaths::CertificationEvidenceDirectory)) {
            if (error) *error = QStringLiteral("Could not create certification directories.");
            return false;
        }
    }
    if (!QFileInfo::exists(path(projectRoot, AramfPaths::CertificationContract))) {
        const QJsonObject contractObject{
            {QStringLiteral("version"), 1},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("history"), QStringLiteral("certificates.jsonl is append-only; a retest always creates a new certificate ID and never rewrites prior certificates.")},
            {QStringLiteral("levels"), QJsonArray{QStringLiteral("BUILD_ONLY"), QStringLiteral("HOST_TEST"), QStringLiteral("SIMULATED"), QStringLiteral("RUNTIME"), QStringLiteral("ON_TARGET"), QStringLiteral("PHYSICAL"), QStringLiteral("GUI_END_TO_END"), QStringLiteral("HARDWARE_CERTIFIED")}},
            {QStringLiteral("passRule"), QStringLiteral("PASS requires all applicable required evidence to be present and verified. Missing physical or on-target evidence cannot produce HARDWARE_CERTIFIED PASS.")},
            {QStringLiteral("currentState"), QStringLiteral("current-certification-state.json resolves the latest certificate per subject; it is derived state, not the certificate history." )},
            {QStringLiteral("projectMemory"), QStringLiteral("CERTIFICATION_STARTED, CERTIFICATE_ISSUED, and CERTIFICATE_FAILED are also recorded in the Project Memory event log." )},
            {QStringLiteral("status"), QStringLiteral("PROJECT_STATUS.md may summarize current certification, while certificates.jsonl remains the durable evidence source." )},
            {QStringLiteral("evidence"), QStringLiteral("Evidence references must identify real persisted evidence. Agents must not fabricate test, build, runtime, physical, or hardware results." )},
            {QStringLiteral("certificateFields"), QJsonArray{
                QStringLiteral("certificateId"), QStringLiteral("certificateType"), QStringLiteral("subject"), QStringLiteral("scope"),
                QStringLiteral("requirements"), QStringLiteral("testMethod"), QStringLiteral("result"), QStringLiteral("verificationLevel"),
                QStringLiteral("environment"), QStringLiteral("targetPlatform"), QStringLiteral("hardwareConfiguration"),
                QStringLiteral("softwareBuildConfiguration"), QStringLiteral("sourceRevision"), QStringLiteral("buildResult"),
                QStringLiteral("testResult"), QStringLiteral("validationResult"), QStringLiteral("evidenceReferences"),
                QStringLiteral("limitations"), QStringLiteral("knownExclusions"), QStringLiteral("relatedProjectMemoryEvents"),
                QStringLiteral("previousCertificateId"), QStringLiteral("supersedesCertificateId")}}
        };
        if (!writeObject(path(projectRoot, AramfPaths::CertificationContract), contractObject, error)) return false;
    }
    if (!QFileInfo::exists(path(projectRoot, AramfPaths::Certificates))) {
        QFile file(path(projectRoot, AramfPaths::Certificates));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (error) *error = file.errorString();
            return false;
        }
    }
    if (!QFileInfo::exists(path(projectRoot, AramfPaths::CurrentCertificationState))
        && !writeObject(path(projectRoot, AramfPaths::CurrentCertificationState),
                        QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("subjects"), QJsonObject{}}}, error)) return false;
    if (!refreshCurrentState(projectRoot, error)) return false;
    const QString statusPath = path(projectRoot, AramfPaths::ProjectStatus);
    QFile statusFile(statusPath);
    if (statusFile.exists() && statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray status = statusFile.readAll();
        statusFile.close();
        if (!status.contains("## Test Certification")) {
            if (!statusFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                if (error) *error = statusFile.errorString();
                return false;
            }
            statusFile.write("\n## Test Certification\n\n- Current durable evidence: `certification/certificates.jsonl`\n- Current resolved state: `certification/current-certification-state.json`\n- No certificate is issued without applicable evidence.\n");
        }
    }
    return true;
}

bool CertificationService::appendCertificate(const QString& projectRoot, const QJsonObject& certificate, QString* error) const
{
    QFile file(path(projectRoot, AramfPaths::Certificates));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray line = QJsonDocument(certificate).toJson(QJsonDocument::Compact) + '\n';
    if (file.write(line) != line.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool CertificationService::start(const QString& projectRoot, const QString& subject,
                                  const QString& certificateType, const QString& scope,
                                  const QString& verificationLevel, const QJsonArray& requirements,
                                  const QJsonObject& context, QJsonObject* result, QString* error) const
{
    if (error) error->clear();
    if (subject.trimmed().isEmpty() || certificateType.trimmed().isEmpty() || scope.trimmed().isEmpty()
        || !isKnownLevel(verificationLevel)) {
        if (error) *error = QStringLiteral("Certification subject, type, scope, and known verification level are required.");
        return false;
    }
    QJsonObject certificate{
        {QStringLiteral("certificateId"), QStringLiteral("cert-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))},
        {QStringLiteral("certificateType"), certificateType},
        {QStringLiteral("subject"), subject},
        {QStringLiteral("scope"), scope},
        {QStringLiteral("verificationLevel"), verificationLevel},
        {QStringLiteral("requirements"), requirements},
        {QStringLiteral("context"), context},
        {QStringLiteral("status"), QStringLiteral("STARTED")},
        {QStringLiteral("certificationStatus"), QStringLiteral("PENDING")},
        {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("evidenceReferences"), QJsonArray{}},
        {QStringLiteral("relatedProjectMemoryEvents"), QJsonArray{}}
    };
    ProjectMemory memory;
    QString memoryError;
    if (!QFileInfo::exists(path(projectRoot, AramfPaths::EventLog))) {
        if (!memory.initializeMemory(projectRoot, nullptr, &memoryError)) {
            if (error) *error = memoryError;
            return false;
        }
    }
    QJsonObject eventFields{{QStringLiteral("certificateId"), certificate.value(QStringLiteral("certificateId"))},
                            {QStringLiteral("subject"), subject},
                            {QStringLiteral("verificationLevel"), verificationLevel}};
    if (!memory.appendEvent(projectRoot, QStringLiteral("CERTIFICATION_STARTED"), subject, eventFields, error)) return false;
    certificate.insert(QStringLiteral("relatedProjectMemoryEvents"), QJsonArray{QStringLiteral("latest:CERTIFICATION_STARTED")});
    if (result) *result = certificate;
    return true;
}

bool CertificationService::issue(const QString& projectRoot, const QJsonObject& input,
                                 const QString& status, const QJsonArray& evidence,
                                 QJsonObject* result, QString* error) const
{
    if (error) error->clear();
    if (input.value(QStringLiteral("certificateId")).toString().isEmpty()
        || input.value(QStringLiteral("subject")).toString().isEmpty()) {
        if (error) *error = QStringLiteral("A started certificate with certificateId and subject is required.");
        return false;
    }
    const QString level = input.value(QStringLiteral("verificationLevel")).toString();
    const QJsonArray requirements = input.value(QStringLiteral("requirements")).toArray();
    const bool evidenceValid = std::all_of(evidence.cbegin(), evidence.cend(), [](const QJsonValue& value) {
        const auto object = value.toObject();
        return !object.value(QStringLiteral("reference")).toString().trimmed().isEmpty()
            && object.value(QStringLiteral("verified")).toBool(false);
    });
    const bool evidenceComplete = evidenceValid && evidence.size() >= requirements.size();
    const bool physicalBlocked = level == QStringLiteral("HARDWARE_CERTIFIED")
        && std::none_of(evidence.cbegin(), evidence.cend(), [](const QJsonValue& value) {
            const auto object = value.toObject();
            return object.value(QStringLiteral("type")).toString().compare(QStringLiteral("physical"), Qt::CaseInsensitive) == 0
                || object.value(QStringLiteral("type")).toString().compare(QStringLiteral("on-target"), Qt::CaseInsensitive) == 0;
        });
    if (isPass(status) && (!evidenceComplete || physicalBlocked)) {
        if (error) *error = QStringLiteral("PASS certification requires complete applicable evidence; physical/on-target evidence is required for HARDWARE_CERTIFIED.");
        return false;
    }
    QJsonObject certificate = input;
    certificate.insert(QStringLiteral("status"), status);
    certificate.insert(QStringLiteral("certificationStatus"), isPass(status) ? QStringLiteral("CERTIFIED") : QStringLiteral("NOT_CERTIFIED"));
    certificate.insert(QStringLiteral("result"), status);
    certificate.insert(QStringLiteral("evidenceReferences"), evidence);
    certificate.insert(QStringLiteral("evidenceComplete"), evidenceComplete && !physicalBlocked);
    certificate.insert(QStringLiteral("issuedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!appendCertificate(projectRoot, certificate, error)) return false;
    ProjectMemory memory;
    const QString eventType = isPass(status) ? QStringLiteral("CERTIFICATE_ISSUED") : QStringLiteral("CERTIFICATE_FAILED");
    if (!memory.appendEvent(projectRoot, eventType, certificate.value(QStringLiteral("subject")).toString(),
                            QJsonObject{{QStringLiteral("certificateId"), certificate.value(QStringLiteral("certificateId"))},
                                        {QStringLiteral("status"), status},
                                        {QStringLiteral("verificationLevel"), level},
                                        {QStringLiteral("evidenceComplete"), certificate.value(QStringLiteral("evidenceComplete"))}}, error)) return false;
    if (!refreshCurrentState(projectRoot, error)) return false;
    if (result) *result = certificate;
    return true;
}

QList<QJsonObject> CertificationService::certificates(const QString& projectRoot, QString* error) const
{
    return readCertificates(path(projectRoot, AramfPaths::Certificates), error);
}

QList<QJsonObject> CertificationService::certificatesForSubject(const QString& projectRoot, const QString& subject, QString* error) const
{
    QList<QJsonObject> result;
    for (const auto& certificate : certificates(projectRoot, error))
        if (certificate.value(QStringLiteral("subject")).toString() == subject) result.append(certificate);
    return result;
}

bool CertificationService::latestForSubject(const QString& projectRoot, const QString& subject, QJsonObject* result, QString* error) const
{
    const auto values = certificatesForSubject(projectRoot, subject, error);
    if (values.isEmpty()) {
        if (error && error->isEmpty()) *error = QStringLiteral("No certificate exists for subject: %1").arg(subject);
        return false;
    }
    if (result) *result = values.last();
    return true;
}

QJsonObject CertificationService::currentState(const QString& projectRoot, QString* error) const
{
    return readObject(path(projectRoot, AramfPaths::CurrentCertificationState), error);
}

QJsonObject CertificationService::contract(const QString& projectRoot, QString* error) const
{
    return readObject(path(projectRoot, AramfPaths::CertificationContract), error);
}

bool CertificationService::refreshCurrentState(const QString& projectRoot, QString* error) const
{
    const auto values = certificates(projectRoot, error);
    if (error && !error->isEmpty()) return false;
    QJsonObject subjects;
    for (const auto& certificate : values) {
        const QString subject = certificate.value(QStringLiteral("subject")).toString();
        if (!subject.isEmpty()) subjects.insert(subject, certificate);
    }
    return writeObject(path(projectRoot, AramfPaths::CurrentCertificationState),
                       QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("subjects"), subjects}}, error);
}
