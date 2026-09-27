#include "FoundationCertificationCampaign.h"
#include "FoundationServices.h"
#include "CertificationService.h"
#include "CertificationFreshness.h"
#include "CertificationRevalidation.h"
#include "ProjectModel.h"
#include "ProjectPersistence.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QUuid>
#include <QSet>
#include <algorithm>

namespace foundation_campaign {
bool fail(QString* error, const QString& message)
{
    if (error) *error = message;
    return false;
}
QString sha(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QByteArray read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QJsonObject object(const QString& path)
{
    return QJsonDocument::fromJson(read(path)).object();
}
bool writeNew(const QString& path, const QByteArray& bytes, QString* error)
{
    if (QFileInfo::exists(path)) return fail(error, "Immutable evidence already exists: " + path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return fail(error, "Cannot create evidence directory");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, file.errorString());
    return read(path) == bytes || fail(error, "Evidence readback mismatch");
}
QString resolve(const QString& root, const QString& file)
{
    return QFileInfo(file).isAbsolute() ? file : QDir(root).filePath(file);
}
bool supported(const QString& subject)
{
    return QStringList{"F2", "F3", "F4"}.contains(subject);
}
bool load(const QString& root, const QString& file, ProjectModel* model, QString* error)
{
    if (error) error->clear();
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    const QString canonicalFile = QFileInfo(resolve(root, file)).canonicalFilePath();
    if (canonicalRoot.isEmpty() || !canonicalFile.startsWith(canonicalRoot + "/"))
        return fail(error, "Project model is outside the requested project namespace");
    if (!ProjectPersistence().load(model, canonicalFile, error)) return false;
    return QFileInfo(model->projectPath()).canonicalFilePath() == canonicalRoot
        || fail(error, "Project model belongs to a different root");
}
bool save(const QString& root, const QString& file, const ProjectModel& model, QString* error)
{
    ProjectPersistence persistence;
    if (!persistence.save(model, resolve(root, file), error)
        || !FoundationCertificationService::synchronizeProjectJson(root, model, error)) return false;
    ProjectModel restored;
    if (!persistence.load(&restored, resolve(root, file), error)) return false;
    return processVersionStateToJson(restored.processVersionState()) == processVersionStateToJson(model.processVersionState())
        || fail(error, "Lifecycle persistence readback mismatch");
}
bool active(const ProjectModel& model, const QString& subject, QString* error)
{
    const auto state = model.processVersionState();
    return (supported(subject) && state.hasActiveProcess && state.activeProcess.isFoundation()
            && state.activeProcess.foundationNumber() == subject.mid(1).toInt()
            && state.activeProcess.done == 0)
        || fail(error, "Expected an active " + subject + " certification iteration");
}
QString git(const QString& root, const QStringList& args, QByteArray* bytes = nullptr)
{
    QProcess process;
    process.setWorkingDirectory(root);
    process.start("git", args);
    if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode()) return {};
    const auto output = process.readAllStandardOutput();
    if (bytes) *bytes = output;
    return QString::fromUtf8(output).trimmed();
}
bool source(const QString& root, const QString& subject, const QString& revision, QString* error)
{
    if (revision.size() != 40 || git(root, {"rev-parse", revision + "^{commit}"}) != revision)
        return fail(error, "Source revision must identify a real full Git commit");
    const auto manifest = CertificationSourceManifestProvider::manifest(subject);
    if (manifest.files.isEmpty()) return fail(error, "Missing source manifest");
    // Scope cleanliness to certified sources. Unrelated user files (e.g. LICENSE)
    // and append-only governed outputs neither bypass nor block source binding.
    const auto tracked = git(root, QStringList{"ls-tree", "-r", "--name-only", revision, "--"} + manifest.files).split('\n');
    for (const auto& relative : manifest.files)
        if (!tracked.contains(relative)) return fail(error, "Uncommitted certification source: " + relative);
    QProcess diff;
    diff.setWorkingDirectory(root);
    diff.start("git", QStringList{"diff", "--quiet", revision, "--"} + manifest.files);
    return (diff.waitForFinished(30000) && diff.exitStatus() == QProcess::NormalExit && diff.exitCode() == 0)
        || fail(error, "Certification source differs from the specified Git revision");
}
struct Command { QString name; QString program; QStringList arguments; };
QList<Command> commands(const QString& root, const QString& subject)
{
    const QString bin = QDir(root).absoluteFilePath("build");
#ifdef Q_OS_WIN
    const QString suffix = ".exe";
#else
    const QString suffix;
#endif
    const QString core = bin + "/aramf_core_tests" + suffix;
    const QString domain = subject == "F2" ? "--f2-identity-trust" : subject == "F3"
        ? "--f3-scope-integrity" : "--f4-lifecycle-certification";
    return {
        {"domain", core, {domain}},
        {"independence", bin + "/aramf_" + subject.toLower() + "_independence" + suffix, {QDir(root).absolutePath()}},
        {"certification", core, {"--foundation-certification"}},
        {"provenance-scope", core, {"--provenance-and-scope"}},
        {"namespace", core, {"--foundation-namespace"}},
        {"migration", core, {"--process-migration"}},
        {"integration", core, {"--foundation-integration"}},
        {"full-ctest", "ctest", {"--test-dir", bin, "--output-on-failure"}}
    };
}
QString identity(const Command& command)
{
    return QFileInfo(command.program).fileName() + "::" + command.arguments.join(" ");
}
bool domainValid(const QString& root, const QString& subject, QString* error)
{
    if (subject == "F2") return FoundationProjectValidation::validateF2(root, error).valid;
    if (subject == "F3") return FoundationProjectValidation::validateF3(root, nullptr, error).valid;
    if (subject == "F4") return LifecycleCertificationFoundation::validate(root, error).valid;
    return fail(error, "Unsupported Foundation");
}
bool certificateValid(const QString& root, const QString& subject, QJsonObject* certificate, QString* error)
{
    CertificationService service;
    QJsonObject value;
    if (!service.latestForSubject(root, subject, &value, error)) return false;
    if (value.value("result") != "PASS" || value.value("certificationStatus") != "CERTIFIED"
        || !value.value("evidenceComplete").toBool())
        return fail(error, subject + " has no complete PASS certificate");
    if (CertificationFreshnessService::evaluate(subject, root).status != CertificationFreshnessStatus::Fresh)
        return fail(error, subject + " certificate is not FRESH");
    if (supported(subject)) {
        QJsonObject revalidation;
        QString revalidationError;
        if (CertificationRevalidationService::latest(root, subject, &revalidation, &revalidationError)) {
            const QString evidencePath = revalidation.value("evidenceArtifact").toString();
            const auto current = object(resolve(root, evidencePath));
            if (revalidation.value("subject") != subject || revalidation.value("revalidationOfCertificateId") != value.value("certificateId")
                || revalidation.value("status") != "FRESH" || current.value("status") != "FRESH"
                || sha(read(resolve(root, evidencePath))) != revalidation.value("evidenceFingerprint"))
                return fail(error, "Revalidation does not bind the historical certificate and current evidence");
            for (const auto& field : QStringList{"subject", "revalidationId", "revalidationOfCertificateId", "sourceRevision", "sourceFingerprint", "contractFingerprint"})
                if (current.value(field) != revalidation.value(field)) return fail(error, "Revalidation binding mismatch: " + field);
            if (!source(root, subject, revalidation.value("sourceRevision").toString(), error)
                || !FoundationCertificationCampaign::validateHistorical(root, subject, current.value("historicalSourceSnapshot").toObject(), error)) return false;
        } else {
            if (!revalidationError.isEmpty()) return fail(error, revalidationError);
            if (!FoundationCertificationCampaign::validateEvidence(root, subject, value.value("lifecycle").toString(),
                    value.value("sourceRevision").toString(), value.value("evidenceArtifact").toString(), nullptr, error)) return false;
        }
    }
    if (certificate) *certificate = value;
    return true;
}
}

bool FoundationCertificationCampaign::start(const QString& root, const QString& file,
                                             const QString& subject, QString* error)
{
    using namespace foundation_campaign;
    ProjectModel model;
    if (!supported(subject)) return fail(error, "Unsupported Foundation");
    if (!load(root, file, &model, error)) return false;
    const auto state = model.processVersionState();
    if (state.hasActiveProcess) return fail(error, "Another lifecycle iteration is active");
    if (!state.hasNextProcess || !state.nextProcess.isFoundation()
        || state.nextProcess.foundationNumber() != subject.mid(1).toInt())
        return fail(error, "Requested Foundation is not the canonical next lifecycle item");
    if (!model.startNextProcess(error)) return false;
    return save(root, file, model, error);
}

bool FoundationCertificationCampaign::verify(const QString& root, const QString& file, const QString& subject,
                                              const QString& revision, QJsonObject* result, QString* error)
{
    using namespace foundation_campaign;
    ProjectModel model;
    if (!load(root, file, &model, error) || !active(model, subject, error)
        || !source(root, subject, revision, error)) return false;
    if (git(root, {"rev-parse", "HEAD"}) != revision) return fail(error, "New verification must bind current HEAD");
    const auto lifecycle = model.processVersionState().activeProcess;
    if (lifecycle.certification != 0) return fail(error, "Iteration is already certified");
    const QString attempt = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString ns = "ARAMF_WORKER/certification/evidence/" + subject.toLower() + "/"
        + lifecycle.identifier() + "/" + attempt;
    QJsonArray checks;
    bool pass = true;
    QString sourceFingerprint, contractFingerprint;
    if (!CertificationSourceManifestProvider::fingerprint(root, subject, &sourceFingerprint, error)
        || !CertificationContractManifestProvider::fingerprint(subject, root, &contractFingerprint, error)) return false;
    for (const auto& command : commands(root, subject)) {
        QProcess process;
        process.setWorkingDirectory(root);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(command.program, command.arguments);
        const bool finished = process.waitForFinished(3600000);
        if (!finished) { process.kill(); process.waitForFinished(); }
        const int exitCode = finished && process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
        const QByteArray bytes = process.readAllStandardOutput();
        const QString log = ns + "/checks/" + command.name + ".log";
        if (!writeNew(resolve(root, log), bytes, error)) return false;
        checks.append(QJsonObject{{"name", command.name}, {"commandIdentity", identity(command)},
            {"sourceRevision", revision}, {"lifecycle", lifecycle.identifier()}, {"namespace", ns},
            {"exitCode", exitCode}, {"status", exitCode == 0 ? "PASS" : "FAIL"},
            {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {"reference", log}, {"fingerprint", sha(bytes)}});
        pass &= exitCode == 0;
    }
    QString finalSource;
    if (!CertificationSourceManifestProvider::fingerprint(root, subject, &finalSource, error)) return false;
    pass &= sourceFingerprint == finalSource && domainValid(root, subject, error);
    QJsonObject evidence{{"schemaVersion", 1}, {"subject", subject}, {"lifecycle", lifecycle.identifier()},
        {"sourceSnapshot", sourceSnapshot(root, subject, revision, error)},
        {"namespace", ns}, {"sourceRevision", revision}, {"sourceFingerprint", sourceFingerprint},
        {"contractFingerprint", contractFingerprint}, {"verificationLevel", "HOST_TEST"},
        {"status", pass ? "PASS" : "FAIL"}, {"checks", checks},
        {"provenance", QJsonObject{{"actor", "tool"}, {"tool", "FoundationCertificationCampaign"}}},
        {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    const QString artifact = ns + "/evidence.json";
    if (evidence.value("sourceSnapshot").toObject().isEmpty()) { pass = false; evidence.insert("status", "FAIL"); }
    if (!writeNew(resolve(root, artifact), QJsonDocument(evidence).toJson(), error)) return false;
    if (result) *result = {{"artifact", artifact}, {"status", evidence.value("status")}};
    return pass || fail(error, "Foundation verification failed; immutable attempt evidence preserved at " + artifact);
}

static bool validateArtifact(const QString& root, const QString& subject,
    const QString& lifecycle, const QString& revision, const QString& artifact, QJsonObject* result, QString* error,
    bool historical)
{
    using namespace foundation_campaign;
    if (error) error->clear();
    if (!supported(subject) || (!historical && !source(root, subject, revision, error))) return false;
    const QString absolute = QFileInfo(resolve(root, artifact)).canonicalFilePath();
    const QString base = QFileInfo(resolve(root, "ARAMF_WORKER/certification/evidence/" + subject.toLower())).canonicalFilePath();
    if (base.isEmpty() || absolute.isEmpty() || !absolute.startsWith(base + "/"))
        return fail(error, "Evidence is outside the subject namespace");
    const auto evidence = object(absolute);
    if (!historical && evidence.contains("sourceSnapshot")
        && evidence.value("sourceSnapshot").toObject() != FoundationCertificationCampaign::sourceSnapshot(root, subject, revision, error))
        return fail(error, "Source witness differs from current committed source");
    const auto provenance = evidence.value("provenance").toObject();
    if (evidence.value("schemaVersion").toInt() != 1 || evidence.value("verificationLevel") != "HOST_TEST"
        || provenance.value("actor") != "tool" || provenance.value("tool") != "FoundationCertificationCampaign"
        || !QDateTime::fromString(evidence.value("timestamp").toString(), Qt::ISODateWithMs).isValid())
        return fail(error, "Missing evidence schema, verification level, timestamp, or producer provenance");
    QString sourceFingerprint, contractFingerprint;
    if (historical) {
        sourceFingerprint = evidence.value("sourceFingerprint").toString();
        contractFingerprint = evidence.value("contractFingerprint").toString();
        if (sourceFingerprint.size() != 64 || contractFingerprint.size() != 64) return fail(error, "Incomplete historical fingerprints");
    } else if (!CertificationSourceManifestProvider::fingerprint(root, subject, &sourceFingerprint, error)
        || !CertificationContractManifestProvider::fingerprint(subject, root, &contractFingerprint, error)) return false;
    const QString ns = evidence.value("namespace").toString();
    const QString expectedPrefix = "ARAMF_WORKER/certification/evidence/" + subject.toLower() + "/" + lifecycle + "/";
    if (!ns.startsWith(expectedPrefix) || ns.mid(expectedPrefix.size()).isEmpty()
        || ns.mid(expectedPrefix.size()).contains('/') || ns.contains("..") || ns.contains('\\'))
        return fail(error, "Evidence namespace does not match canonical lifecycle");
    if (evidence.value("subject") != subject || evidence.value("lifecycle") != lifecycle
        || evidence.value("sourceRevision") != revision || evidence.value("sourceFingerprint") != sourceFingerprint
        || evidence.value("contractFingerprint") != contractFingerprint || evidence.value("status") != "PASS"
        || QFileInfo(resolve(root, ns + "/evidence.json")).canonicalFilePath() != absolute)
        return fail(error, "Evidence subject, lifecycle, source, namespace, or result binding mismatch");
    const auto checks = evidence.value("checks").toArray();
    const auto expected = commands(root, subject);
    if (checks.size() != expected.size()) return fail(error, "Incomplete verification check set");
    QSet<QString> names;
    for (const auto& value : checks) {
        const auto check = value.toObject();
        const QString name = check.value("name").toString();
        const auto it = std::find_if(expected.begin(), expected.end(), [&](const Command& command){return command.name == name;});
        const QString log = ns + "/checks/" + name + ".log";
        const QString logAbsolute = QFileInfo(resolve(root, log)).canonicalFilePath();
        if (names.contains(name) || it == expected.end() || check.value("commandIdentity") != identity(*it)
            || check.value("sourceRevision") != revision || check.value("lifecycle") != lifecycle
            || check.value("namespace") != ns || check.value("status") != "PASS"
            || !QDateTime::fromString(check.value("timestamp").toString(), Qt::ISODateWithMs).isValid()
            || !check.value("exitCode").isDouble() || check.value("exitCode").toInt(-1) != 0
            || check.value("reference") != log || logAbsolute.isEmpty() || !logAbsolute.startsWith(base + "/")
            || check.value("fingerprint") != sha(read(logAbsolute)))
            return fail(error, "Invalid, duplicate, missing, or altered verification check: " + name);
        names.insert(name);
    }
    if (result) *result = evidence;
    return true;
}

bool FoundationCertificationCampaign::validateEvidence(const QString& root, const QString& subject,
    const QString& lifecycle, const QString& revision, const QString& artifact, QJsonObject* result, QString* error)
{
    return validateArtifact(root, subject, lifecycle, revision, artifact, result, error, false);
}

bool FoundationCertificationCampaign::validateCurrentSource(const QString& root, const QString& subject, const QString& revision, QString* error)
{
    return foundation_campaign::source(root, subject, revision, error);
}

QJsonObject FoundationCertificationCampaign::sourceSnapshot(const QString& root, const QString& subject, const QString& revision, QString* error)
{
    using namespace foundation_campaign;
    if (!source(root, subject, revision, error)) return {};
    QJsonArray files;
    for (const auto& path : CertificationSourceManifestProvider::manifest(subject).files)
        files.append(QJsonObject{{"path", path}, {"bytesBase64", QString::fromLatin1(read(resolve(root, path)).toBase64())}});
    QString fingerprint;
    if (!CertificationSourceManifestProvider::fingerprint(root, subject, &fingerprint, error)) return {};
    return {{"sourceRevision", revision}, {"sourceFingerprint", fingerprint}, {"files", files}};
}

bool FoundationCertificationCampaign::validateHistorical(const QString& root, const QString& subject,
    const QJsonObject& suppliedSnapshot, QString* error)
{
    using namespace foundation_campaign;
    if (error) error->clear();
    QJsonObject certificate;
    if (!supported(subject) || !CertificationService().latestForSubject(root, subject, &certificate, error)) return false;
    const QString artifact = certificate.value("evidenceArtifact").toString();
    const QString revision = certificate.value("sourceRevision").toString();
    if (certificate.value("result") != "PASS" || certificate.value("certificationStatus") != "CERTIFIED"
        || !certificate.value("evidenceComplete").toBool() || artifact.isEmpty()
        || sha(read(resolve(root, artifact))) != certificate.value("evidenceFingerprint")
        || revision.size() != 40 || git(root, {"rev-parse", revision + "^{commit}"}) != revision)
        return fail(error, "Invalid immutable historical certificate/evidence/revision binding");
    QJsonObject evidence;
    if (!validateArtifact(root, subject, certificate.value("lifecycle").toString(), revision, artifact, &evidence, error, true)) return false;
    // New issuances contain their source witness. Legacy issuances may acquire
    // an append-only witness through revalidation, but only an exact match to
    // the original sealed source fingerprint can establish that bridge.
    return CertificationRevalidationService::validateHistoricalSource(root, certificate, suppliedSnapshot, error);
}

bool FoundationCertificationCampaign::certify(const QString& root, const QString& file, const QString& subject,
    const QString& revision, const QString& artifact, QJsonObject* result, QString* error)
{
    using namespace foundation_campaign;
    ProjectModel model;
    if (!load(root, file, &model, error) || !active(model, subject, error)) return false;
    const auto lifecycle = model.processVersionState().activeProcess;
    if (lifecycle.certification != 0) return fail(error, "Iteration is already certified");
    QJsonObject evidence;
    if (!validateEvidence(root, subject, lifecycle.identifier(), revision, artifact, &evidence, error)
        || !domainValid(root, subject, error)) return false;
    const QString relative = QDir(root).relativeFilePath(resolve(root, artifact));
    const QString fingerprint = sha(read(resolve(root, artifact)));
    CertificationService service;
    QJsonObject certificate;
    // Recover a durable issue before a failed lifecycle save without issuing twice.
    QJsonObject existing;
    service.latestForSubject(root, subject, &existing, nullptr);
    if (existing.value("evidenceFingerprint") == fingerprint && existing.value("result") == "PASS")
        certificate = existing;
    else {
        QJsonObject started;
        const QJsonObject context{{"sourceRevision", revision}, {"evidenceArtifact", relative},
            {"evidenceFingerprint", fingerprint}, {"lifecycle", lifecycle.identifier()}};
        if (!service.start(root, subject, "FOUNDATION", "project", "HOST_TEST",
                           QJsonArray{"domain-bound-evidence"}, context, &started, error)) return false;
        for (auto it = context.begin(); it != context.end(); ++it) started.insert(it.key(), it.value());
        started.insert("foundationVersion", subject + "." + QString::number(lifecycle.loop) + "." + QString::number(lifecycle.iteration));
        if (!existing.isEmpty()) started.insert("supersedesCertificateId", existing.value("certificateId"));
        if (!service.issue(root, started, "PASS", QJsonArray{QJsonObject{{"reference", relative},
                           {"fingerprint", fingerprint}, {"verified", true}, {"type", "HOST_TEST"}}}, &certificate, error)) return false;
    }
    QJsonObject persisted;
    if (!certificateValid(root, subject, &persisted, error)
        || persisted.value("certificateId") != certificate.value("certificateId"))
        return fail(error, "Certificate persistence or freshness validation failed");
    if (!model.certifyCurrentProcessIteration(error) || !save(root, file, model, error)) return false;
    if (result) *result = persisted;
    return true;
}

bool FoundationCertificationCampaign::complete(const QString& root, const QString& file,
                                                const QString& subject, QString* error)
{
    using namespace foundation_campaign;
    ProjectModel model;
    if (!load(root, file, &model, error) || !active(model, subject, error)) return false;
    const auto lifecycle = model.processVersionState().activeProcess;
    QJsonObject certificate;
    if (lifecycle.certification != 1) return fail(error, "Completion requires a certified iteration");
    if (!certificateValid(root, subject, &certificate, error)) return false;
    auto uncertified = lifecycle;
    uncertified.certification = 0;
    if (!validateEvidence(root, subject, uncertified.identifier(), certificate.value("sourceRevision").toString(),
                          certificate.value("evidenceArtifact").toString(), nullptr, error)) return false;
    return model.completeActiveProcess(error) && save(root, file, model, error);
}

QJsonObject FoundationCertificationCampaign::freshness(const QString& root)
{
    QJsonObject result;
    for (const auto& subject : QStringList{"F1","F2","F3","F4","S1","S2","S3","S4","S5","S6","P1","P2","P3","P4","P5"})
        result.insert(subject, CertificationFreshnessService::evaluate(subject, root).toJson());
    return result;
}

bool FoundationCertificationCampaign::acceptIntegration(const QString& root, const QString& file,
                                                         QJsonObject* result, QString* error)
{
    using namespace foundation_campaign;
    ProjectModel model;
    if (!load(root, file, &model, error)) return false;
    auto state = model.processVersionState();
    if (state.hasActiveProcess || state.hasActiveStructure) return fail(error, "An unfinished lifecycle blocks integration acceptance");
    QJsonObject certificates;
    for (const auto& subject : QStringList{"F1","F2","F3","F4","P1","P2","P3","P4","P5"}) {
        QJsonObject certificate;
        if (!certificateValid(root, subject, &certificate, error)) return false;
        certificates.insert(subject, certificate.value("certificateId"));
    }
    for (int n = 1; n <= 4; ++n) {
        bool found = false;
        for (const auto& version : state.completedHistory)
            found |= version.isFoundation() && version.foundationNumber() == n && version.certification == 1 && version.done == 1;
        if (!found) return fail(error, "Foundation lifecycle is incomplete");
    }
    const auto report = FoundationIntegrationService::validate(root, &model, error);
    if (!report.valid) return false;
    const QString path = "ARAMF_WORKER/certification/integration/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + "/acceptance.json";
    const QJsonObject acceptance{{"schemaVersion", 1}, {"acceptanceType", "AUTHORITATIVE_ACCEPTANCE"},
        {"result", "PASS"}, {"certificates", certificates}, {"freshness", freshness(root)},
        {"validation", report.fullReport}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!writeNew(resolve(root, path), QJsonDocument(acceptance).toJson(), error)) return false;
    // Derived only after independent certificates, their current freshness, and
    // external integration have all passed. This does not instantiate peers inside a Foundation.
    state.foundationIntegrationValid = report.valid;
    auto project = ProjectPersistence().toJson(model);
    project.insert("processVersion", processVersionStateToJson(state));
    if (!state.isP6Eligible(error) || !ProjectPersistence().fromJson(&model, project, error)
        || !save(root, file, model, error)) return false;
    if (result) *result = {{"artifact", path}, {"foundationIntegrationValid", true}, {"p6Entry", "UNBLOCKED"}};
    return true;
}

bool FoundationCertificationCampaign::eligible(const QString& root, const QString& file, QString* error)
{
    using namespace foundation_campaign;
    ProjectModel model;
    if (!load(root, file, &model, error)
        || !model.processVersionState().isP6Eligible(error)) return false;
    for (const auto& subject : QStringList{"F1","F2","F3","F4","P1","P2","P3","P4","P5"})
        if (!certificateValid(root, subject, nullptr, error)) return false;
    bool accepted = false;
    const QDir directory(resolve(root, "ARAMF_WORKER/certification/integration"));
    for (const auto& entry : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto record = object(directory.filePath(entry + "/acceptance.json"));
        if (record.value("acceptanceType") != "AUTHORITATIVE_ACCEPTANCE" || record.value("result") != "PASS") continue;
        bool matching = true;
        for (const auto& subject : QStringList{"F1","F2","F3","F4","P1","P2","P3","P4","P5"}) {
            QJsonObject certificate;
            if (!certificateValid(root, subject, &certificate, error)) return false;
            const auto current = CertificationFreshnessService::evaluate(subject, root);
            const auto bound = record.value("freshness").toObject().value(subject).toObject();
            matching &= record.value("certificates").toObject().value(subject) == certificate.value("certificateId")
                && bound.value("currentSourceFingerprint") == current.currentSourceFingerprint
                && bound.value("currentContractFingerprint") == current.currentContractFingerprint
                && bound.value("currentEvidenceFingerprint") == current.currentEvidenceFingerprint;
        }
        accepted |= matching;
    }
    if (!accepted) return fail(error, "No persisted current external integration acceptance");
    return FoundationIntegrationService::validate(root, &model, error).valid;
}
