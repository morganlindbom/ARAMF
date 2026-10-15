// WorkerTaskServices.cpp
#include "WorkerTaskServices.h"
#include "AramfPaths.h"
#include "CertificationService.h"
#include "CertificationFreshness.h"
#include "CertificationRevalidation.h"
#include "FoundationCertificationCampaign.h"
#include "ContextCoordinationService.h"
#include "ConfigurationUpdateService.h"
#include "DocumentInstruction.h"
#include "ProjectMemory.h"
#include "ProjectPersistence.h"
#include "Services.h"
#include "ValidationRouting.h"
#include "WorkerContextResolver.h"
#include "foundations/F2/IdentityTrustFoundation.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QSet>
#include <QDateTime>
#include <QLockFile>
#include <QUuid>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QTemporaryFile>
#include <algorithm>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
QStringList strings(const QJsonValue& value)
{
    QStringList result;
    for (const auto& v : value.toArray()) if (v.isString() && !v.toString().isEmpty()) result.append(v.toString());
    result.removeDuplicates();
    std::sort(result.begin(), result.end());
    return result;
}
QJsonArray array(QStringList values)
{
    values.removeDuplicates();
    std::sort(values.begin(), values.end());
    return QJsonArray::fromStringList(values);
}
QJsonValue semantic(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject out;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) out.insert(it.key(), semantic(it.value()));
        return out;
    }
    if (value.isArray()) {
        QList<QByteArray> values;
        for (const auto& entry : value.toArray()) values.append(QJsonDocument(QJsonArray{semantic(entry)}).toJson(QJsonDocument::Compact));
        std::sort(values.begin(), values.end());
        QJsonArray out;
        for (const auto& entry : values) out.append(QJsonDocument::fromJson(entry).array().first());
        return out;
    }
    return value;
}
QString digest(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QString fingerprint(const QJsonObject& value)
{
    return digest(QJsonDocument(semantic(value).toObject()).toJson(QJsonDocument::Compact));
}
QString fileHash(const QString& path)
{
    QFile file(path);
    if (!file.exists()) return QStringLiteral("MISSING");
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("UNREADABLE");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return QStringLiteral("UNREADABLE");
    return QString::fromLatin1(hash.result().toHex());
}
QJsonObject readObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
void issue(QJsonArray& errors, const QString& code, const QString& message,
           const QString& path = {}, const QString& severity = QStringLiteral("BLOCKED"))
{
    errors.append(QJsonObject{{"code", code}, {"message", message}, {"path", path}, {"severity", severity}});
}
QString state(const QJsonArray& errors)
{
    bool warning = false;
    bool blocked = false;
    for (const auto& error : errors) {
        const auto severity = error.toObject().value("severity").toString();
        if (severity == "FATAL") return QStringLiteral("FATAL");
        if (severity == "WARNING") warning = true; else blocked = true;
    }
    return blocked ? QStringLiteral("BLOCKED") : warning ? QStringLiteral("READY_WITH_WARNINGS") : QStringLiteral("READY");
}
QString root(const ProjectModel& model) { return QDir(model.projectPath()).absolutePath(); }
QString worker(const ProjectModel& model) { return AramfPaths::workerDirectoryName(model.workerNameSuffix()); }
QString pathFor(const ProjectModel& model, const QString& relative) { return QDir(root(model)).filePath(relative); }
QString workerPath(const ProjectModel& model, const QString& relative) { return worker(model) + '/' + relative; }
bool safePath(const QString& projectRoot, const QString& relative)
{
    if (relative.isEmpty() || relative.contains('\\') || relative.contains(':') || relative.contains('*')
        || relative.contains('?') || QDir::isAbsolutePath(relative) || QDir::cleanPath(relative) != relative
        || relative == "." || relative == ".." || relative.startsWith("../") || relative.compare(".git", Qt::CaseInsensitive) == 0
        || relative.startsWith(".git/", Qt::CaseInsensitive)) return false;
    for (const auto& part : relative.split('/')) if (part.endsWith('.') || part.endsWith(' ')) return false;
    QFileInfo anchor(QDir(projectRoot).filePath(relative));
    while (!anchor.exists() && anchor.absoluteFilePath() != anchor.absolutePath()) anchor = QFileInfo(anchor.absolutePath());
    const QString canonicalRoot = QFileInfo(projectRoot).canonicalFilePath();
    const QString canonical = anchor.canonicalFilePath();
    return !canonicalRoot.isEmpty() && (canonical == canonicalRoot || canonical.startsWith(canonicalRoot + '/'));
}
bool git(const QString& projectRoot, const QStringList& arguments, QByteArray* output)
{
    QProcess process;
    process.start(QStringLiteral("git"), QStringList{QStringLiteral("-C"), projectRoot} + arguments);
    if (!process.waitForFinished(10000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return false;
    *output = process.readAllStandardOutput();
    return true;
}
bool relocationPath(const ProjectModel& model, const QString& path)
{
    if (!safePath(root(model), path) || path.startsWith(worker(model) + '/', Qt::CaseInsensitive)) return false;
    QFileInfo part(pathFor(model, path));
    while (part.absoluteFilePath() != root(model) && part.absoluteFilePath() != part.absolutePath()) {
        if (part.isSymLink()) return false;
        part.setFile(part.absolutePath());
    }
    return true;
}
bool committedContent(const ProjectModel& model, const QString& revision, const QString& path, const QString& hash)
{
    QByteArray resolved, original;
    if (revision.size() != 40 || hash.size() != 64 || !relocationPath(model, path)
        || !git(root(model), {"rev-parse", revision + "^{commit}"}, &resolved)
        || QString::fromUtf8(resolved).trimmed() != revision
        || !git(root(model), {"show", revision + ':' + path}, &original)) return false;
    // Git may store LF while the checked-out, byte-bound baseline uses CRLF.
    // The destination must still match the exact physical baseline hash.
    auto crlf = original; crlf.replace("\r\n", "\n"); crlf.replace("\n", "\r\n");
    return digest(original) == hash || digest(crlf) == hash;
}
// Task evidence files are excluded to avoid a checksum depending on itself.
// In Git projects ignored build products are excluded, but Worker state is
// always observed even when the generated Worker is intentionally ignored.
QJsonObject snapshot(const ProjectModel& model, QJsonArray& errors)
{
    QStringList files;
    QByteArray output;
    const bool inGit = git(root(model), {"rev-parse", "--show-toplevel"}, &output);
    if (inGit) {
        if (QFileInfo(QString::fromUtf8(output).trimmed()).canonicalFilePath() != QFileInfo(root(model)).canonicalFilePath())
            issue(errors, "REPOSITORY_ROOT_MISMATCH", "Project root is nested inside a different Git worktree.", root(model), "FATAL");
        if (!git(root(model), {"ls-files", "--unmerged", "-z"}, &output) || !output.isEmpty())
            issue(errors, "REPOSITORY_CONFLICT", "Unresolved Git conflicts or unreadable index.", {}, "FATAL");
        if (git(root(model), {"ls-files", "--cached", "--others", "--exclude-standard", "-z"}, &output)) {
            for (const auto& name : output.split('\0')) if (!name.isEmpty()) files.append(QString::fromUtf8(name));
        } else issue(errors, "REPOSITORY_UNREADABLE", "Cannot enumerate the Git working tree.", {}, "FATAL");
    } else {
        issue(errors, "REPOSITORY_WITHOUT_GIT", "Filesystem boundary checks are available; Git index/conflict checks are unavailable.", {}, "WARNING");
    }
    QDirIterator iterator(inGit ? pathFor(model, worker(model)) : root(model), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (iterator.hasNext()) files.append(QDir(root(model)).relativeFilePath(iterator.next()));
    // Explicit canonical dependencies remain observable even when Git ignores
    // them (for example locally supplied generated headers).
    const auto metadata = model.ruleConfiguration().scopeMetadata;
    for (const auto& value : metadata) files << strings(value.toObject().value("files"));
    QJsonObject result;
    for (const auto& path : strings(array(files))) {
        if (path.startsWith(".git/")) continue;
        if (!safePath(root(model), path)) {
            issue(errors, "UNSAFE_PATH", "Path escapes the project or follows an external link.", path, "FATAL");
            continue;
        }
        if (path.startsWith(workerPath(model, "context/handoffs/")) && path.endsWith(".json"))
            continue;
        if (path.startsWith(workerPath(model, "verification/tasks/")) && path.endsWith(".json")) {
            const QSet<QString> reserved{"project.json", "worker-manifest.json", "scope-routes.json", "task-routes.json", "resources.json", "latest-validation.json", "agent-status.json", "agent-memory.json"};
            const auto artifact = readObject(pathFor(model, path));
            const bool evidence = artifact.contains("check") && artifact.contains("contractId") && artifact.contains("status");
            const bool contract = artifact.value("contract").toObject().contains("contractId");
            const bool result = artifact.value("authority").toString() == "DERIVED" && artifact.contains("contractId") && artifact.contains("completionState");
            if (!reserved.contains(QFileInfo(path).fileName()) && (evidence || contract || result)) continue;
        }
        result.insert(path, fileHash(pathFor(model, path)));
    }
    // Protect external authoritative sources too. Their identity is read-only.
    for (const auto& resource : model.resources()) {
        if (resource.type == "url" || resource.location.contains("://")) continue;
        if (!resource.location.isEmpty()) {
            const auto observed = WorkerContextResolver::resourceSnapshot(model, resource.id);
            result.insert(QStringLiteral("external:") + resource.id,
                observed.value("valid").toBool() ? observed.value("fingerprint").toString() : QStringLiteral("UNREADABLE"));
            if (resource.type == "folder") result.insert(QStringLiteral("external-manifest:") + resource.id, observed);
            if (!observed.value("valid").toBool()) issue(errors, "RESOURCE_INSPECTION_FAILED", observed.value("error").toString(), resource.location);
        }
    }
    return result;
}
QJsonObject binding(const ProjectModel& model)
{
    QJsonObject inputs;
    const QStringList canonical{"project.json", "worker-manifest.json", "routing/scope-routes.json", "routing/task-routes.json",
        "routing/validation-policy.json", "resources/resources.json", "memory/decisions.md", "memory/memory-contract.json",
        "memory/framework-knowledge.json", "memory/memory-config.json", "AGENTS.md"};
    for (const auto& path : canonical) {
        const QString absolute = pathFor(model, workerPath(model, path));
        auto json = readObject(absolute);
        json.remove("_file");
        if (path == QStringLiteral("project.json")) {
            // The Worker project representation also carries the mutable P2
            // execution snapshot. It is derived state, not a TaskContract
            // authorization input, so exclude it from the binding while
            // retaining the rest of the canonical project configuration.
            json.remove(QStringLiteral("orchestration"));
        }
        inputs.insert(path, path.endsWith(".json") && !json.isEmpty() ? fingerprint(json) : fileHash(absolute));
    }
    return {{"projectRoot", root(model)}, {"projectId", model.projectId()}, {"workerIdentity", worker(model)},
        // Runtime ownership and orchestration are mutable execution state, not
        // authorization inputs.  Binding only the canonical project
        // configuration keeps a valid contract stable while P2 claims and
        // releases resources or updates its execution snapshot.
        {"modelFingerprint", fingerprint(ProjectPersistence().configuration(model))}, {"inputs", inputs}};
}
void parallelState(const ProjectModel& model, const QJsonObject& files, QJsonArray& errors)
{
    const QSet<QString> names{"project.json", "worker-manifest.json", "task-routes.json", "scope-routes.json",
        "current-state.md", "latest-validation.json", "resources.json"};
    QSet<QString> seen;
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (!it.key().startsWith(worker(model) + '/') || it.key().startsWith(workerPath(model, "custom/"))) continue;
        const QString name = QFileInfo(it.key()).fileName().toLower();
        if (names.contains(name)) {
            if (seen.contains(name)) issue(errors, "CANONICAL_OWNER_DUPLICATE", "Competing canonical Worker file.", it.key());
            seen.insert(name);
        }
        if (name == "agent-status.json" || name == "agent-memory.json" || name == "memory-store.json" || name == "status.db")
            issue(errors, "CANONICAL_OWNER_DUPLICATE", "Ad-hoc agent memory/status store is forbidden.", it.key());
    }
}
QJsonObject role(const QString& owner, const QStringList& actions, const QString& writer)
{
    return {{"owner", owner}, {"actions", array(actions)}, {"writer", writer}};
}
QJsonObject fileRole(const ProjectModel& model, const QString& path, const QJsonObject& policy)
{
    if (path.startsWith("external:") || path.startsWith(workerPath(model, "custom/"))) return role("user", {"READ", "NEVER_TOUCH"}, "user");
    for (const auto& resource : model.resources()) {
        if (resource.location.isEmpty() || resource.location.contains("://")) continue;
        if (QDir::cleanPath(pathFor(model, path)).compare(QDir::cleanPath(pathFor(model, resource.location)), Qt::CaseInsensitive) == 0)
            return role(resource.role == "source-of-truth" ? "source-of-truth" : "external-resource", {"READ", "NEVER_TOUCH"}, "user");
    }
    const auto exact = policy.value("files").toObject();
    if (exact.contains(path)) return exact.value(path).toObject();
    if (path == "AGENTS.md" || path.startsWith(worker(model) + '/')) return role("governance", {"READ"}, "ARAMF");
    return role("user", {"READ"}, "user");
}
QStringList reauthorizationOutputs();
void validateCanonical(const ProjectModel& model, QJsonArray& errors, const QJsonObject& reauth = {})
{
    const auto reauthInputs = strings(reauth.value("canonicalInputs"));
    const auto canRegenerate = [&reauthInputs](const QString& path) { return reauthInputs.contains(path); };
    const auto config = readObject(pathFor(model, workerPath(model, "project.json")));
    const auto manifest = readObject(pathFor(model, workerPath(model, "worker-manifest.json")));
    const QString generationHash = projectConfigurationFingerprint(model, model.generationOptions());
    if (config.isEmpty() || manifest.isEmpty()) issue(errors, "MIGRATION_REQUIRED", "Generate the versioned Worker before preparing a task.");
    if (manifest.value("workerSchemaVersion").toInt() != 1 || config.value("schemaVersion").toInt() != 1) {
        if (!canRegenerate(workerPath(model, "project.json")))
            issue(errors, "WORKER_SCHEMA_UNSUPPORTED", "Task contracts require Worker schema v1.", workerPath(model, "project.json"));
    }
    if (config.value("projectId").toString() != model.projectId() || manifest.value("workerIdentity").toString() != worker(model))
        issue(errors, "WORKER_IDENTITY_MISMATCH", "Worker identity does not match the canonical project.");
    if (config.value("configurationFingerprint").toString() != generationHash || manifest.value("generatedFromFingerprint").toString() != generationHash) {
        if (!canRegenerate(workerPath(model, "project.json")) || !canRegenerate(workerPath(model, "worker-manifest.json")))
            issue(errors, "STALE_DERIVED_ARTIFACT", "Project/manifest generation fingerprints are stale.");
    }
    const QString contextIndexPath = pathFor(model, workerPath(model, "context/context-index.json"));
    if (QFileInfo::exists(contextIndexPath)) {
        const auto context = ContextCoordinationService::validate(model);
        const bool contextRefreshCovered = canRegenerate(workerPath(model, "context/context-index.json"))
            && canRegenerate(workerPath(model, "context/compressed-context.json"))
            && canRegenerate(workerPath(model, "context/freshness.json"))
            && canRegenerate(workerPath(model, "context/agent-adapters.json"))
            && canRegenerate(workerPath(model, "context/task-dag.json"));
        if (!context.value(QStringLiteral("valid")).toBool() && !contextRefreshCovered)
            issue(errors, "STALE_DERIVED_ARTIFACT",
                  "Derived P1 context is stale or invalid; regenerate it through ContextCoordinationService.",
                  context.value(QStringLiteral("errorCode")).toString());
    }
    const auto expected = GenerationServices::derivedTaskArtifacts(model, model.generationOptions());
    for (auto it = expected.begin(); it != expected.end(); ++it) {
        if (it.key().contains(QStringLiteral("/context/"))) continue;
        auto actual = readObject(pathFor(model, it.key())); actual.remove("_file");
        if (semantic(actual) != semantic(it.value())) {
            if (!canRegenerate(it.key())) issue(errors, "STALE_DERIVED_ARTIFACT", "Derived content disagrees with its canonical producer; regenerate it.", it.key());
        }
    }
    const auto summary = readObject(pathFor(model, workerPath(model, "verification/latest-validation.json")));
    const auto evidence = readObject(pathFor(model, workerPath(model, "verification/verification-result.json")));
    if (summary.value("schemaVersion").toInt() != 1 || summary.value("fingerprint").toString() != generationHash
        || summary.value("overallStatus").toString() != "PASS" || evidence.value("overallStatus").toString() != "PASS"
        || evidence.value("fingerprint").toString() != generationHash) {
        if (!canRegenerate(workerPath(model, "verification/latest-validation.json"))
            || !canRegenerate(workerPath(model, "verification/verification-result.json")))
            issue(errors, "VALIDATION_SUMMARY_CORRUPT", "Current PASS summary and its detailed verification evidence are required.");
    }
    const auto canonical = manifest.value("canonicalFiles").toObject();
    QSet<QString> owners;
    for (const auto& entry : canonical) {
        if (owners.contains(entry.toString())) issue(errors, "CANONICAL_OWNER_DUPLICATE", "Manifest assigns multiple owners to the same path.", entry.toString());
        owners.insert(entry.toString());
    }
    if (model.generationOptions().generateMemory) {
        for (const QString& name : {QStringLiteral("memory/decisions.md"), QStringLiteral("memory/event-log.jsonl"), QStringLiteral("memory/memory-contract.json")})
            if (!QFileInfo::exists(pathFor(model, workerPath(model, name)))) issue(errors, "USER_SOURCE_MISSING", "Canonical memory source is missing; it must not be fabricated.", name);
    }
    for (const QString& name : {QStringLiteral("AGENTS.md"), QStringLiteral("routing/validation-policy.json"), QStringLiteral("memory/current-state.md")}) {
        const auto hash = fileHash(pathFor(model, workerPath(model, name)));
        if (hash == "MISSING" || hash == "UNREADABLE") issue(errors, "WORKER_ROUTE_MISSING", "Required control source is unavailable.", name);
    }
    // Generated resource routing cannot introduce a resource or change its
    // authority independently of the saved ProjectResource registry.
    const auto resources = readObject(pathFor(model, workerPath(model, "resources/resources.json"))).value("resources").toArray();
    if (resources.size() != model.resources().size()) issue(errors, "STALE_DERIVED_ARTIFACT", "Resource registry count differs from canonical model.");
    for (const auto& value : resources) {
        const auto resource = value.toObject();
        bool found = false;
        for (const auto& source : model.resources()) if (source.id == resource.value("id").toString()) {
            found = source.location == resource.value("location").toString() && source.role == resource.value("role").toString()
                && source.authorityLevel == resource.value("authority").toString() && source.enabled == resource.value("enabled").toBool()
                && array(source.scopes) == array(strings(resource.value("scopes")));
        }
        if (!found) issue(errors, "STALE_DERIVED_ARTIFACT", "Resource routing differs from its canonical source.", resource.value("id").toString());
    }
    const auto verification = VerificationServices().verify(model, model.generationOptions(), false);
    if (verification.overallStatus != VerificationStatus::Pass
        && (!canRegenerate(workerPath(model, "verification/latest-validation.json"))
            || !canRegenerate(workerPath(model, "verification/verification-result.json"))))
        issue(errors, "VALIDATION_FAILED", "Canonical read-only Worker verification is not PASS; stored summary cannot authorize work.");
}
}

QJsonObject WorkerTaskRequest::toJson() const
{
    QJsonObject result{{"goal", goal}, {"type", type}, {"scopes", array(scopes)}, {"files", array(files)},
        {"definitionOfDone", array(definitionOfDone)}, {"history", history}, {"destructive", destructive}};
    if (!relocations.isEmpty()) result.insert("relocations", relocations);
    if (!protectedGrantId.isEmpty()) result.insert("protectedGrantId", protectedGrantId);
    if (!reauthorizationId.isEmpty()) result.insert("reauthorizationId", reauthorizationId);
    if (!acceptanceProfile.isEmpty()) result.insert("acceptanceProfile", acceptanceProfile);
    return result;
}
WorkerTaskRequest WorkerTaskRequest::fromJson(const QJsonObject& value)
{
    WorkerTaskRequest request{value.value("goal").toString(), value.value("type").toString(), strings(value.value("scopes")),
        strings(value.value("files")), strings(value.value("definitionOfDone")), value.value("history").toBool(), value.value("destructive").toBool(), value.value("relocations").toArray(), value.value("protectedGrantId").toString()};
    request.reauthorizationId = value.value("reauthorizationId").toString();
    request.acceptanceProfile = value.value("acceptanceProfile").toString();
    return request;
}

QJsonObject TaskReauthorizationRequest::toJson() const
{
    // Keep authorization input declarative; all fingerprints and baselines are service-owned.
    return {{"task", task.toJson()}, {"canonicalInputs", array(canonicalInputs)},
        {"operations", operations}, {"validityMinutes", validityMinutes}};
}

TaskReauthorizationRequest TaskReauthorizationRequest::fromJson(const QJsonObject& value)
{
    TaskReauthorizationRequest request;
    request.task = WorkerTaskRequest::fromJson(value.value("task").toObject());
    request.canonicalInputs = strings(value.value("canonicalInputs"));
    request.operations = value.value("operations").toObject();
    request.validityMinutes = value.value("validityMinutes").toInt();
    return request;
}

namespace {
// Bind the complete task intent independently of its grant reference.

// A grant cannot be carried to a different goal, file set, scope or acceptance
// contract by copying its ID into another serialized request.
QString grantRequestFingerprint(const WorkerTaskRequest& task)
{
    auto intent = task.toJson(); intent.remove("protectedGrantId");
    return fingerprint(intent);
}

// Define the sole build edit supported by this narrowly scoped grant.

// Appending this exact registration block leaves all existing CMake bytes
// intact. The grant authorizes neither arbitrary replacement nor other paths.
QByteArray p6Registration()
{
    return "\n# P6 Canonical Code Bank build and test registration.\n"
        "target_sources(aramf PRIVATE src/processes/P6/CodeBank.h src/processes/P6/CodeBank.cpp src/processes/P6/CodeBankStorage.cpp src/processes/P6/CodeBankVerification.cpp)\n"
        "if(BUILD_TESTING)\n"
        "    add_executable(aramf_p6_tests tests/P6CodeBankTests.cpp ${ARAMF_CORE_SOURCES} src/core/WorkerTaskServices.h src/core/WorkerTaskServices.cpp src/processes/P6/CodeBank.cpp src/processes/P6/CodeBankStorage.cpp src/processes/P6/CodeBankVerification.cpp)\n"
        "    target_include_directories(aramf_p6_tests PRIVATE src)\n"
        "    target_link_libraries(aramf_p6_tests PRIVATE Qt6::Core)\n"
        "    add_test(NAME aramf_p6_tests COMMAND aramf_p6_tests)\n"
        "endif()\n";
}

// Recover and validate a live grant from canonical append-only authority.

// Forged references, mismatched project/task/path/operation, expired grants and
// successful prior consumption fail closed. Both prepare and postflight use
// this check; serialized permission fields are never sufficient authority.

QString exactEventHash(const QJsonObject& event)
{
    // Object keys are canonical in QJsonObject; array ordering is preserved.
    return digest(QJsonDocument(event).toJson(QJsonDocument::Compact));
}
QString p6ScopedAuthorityInstruction()
{
    return "Admin Morgan Lindbom explicitly authorizes a narrowly scoped ARAMF override for the P6 preparation remediation. Authorize deterministic folder-resource fingerprinting with explicit governed handling of generated contents, and an auditable, fail-closed, single-use grant mechanism consumed by both task preparation and postflight. The protected-path exception is limited to CMakeLists.txt for the exact P6 build and test registration in the P6 task. Preserve default protection for all other paths. Create the exact governed repair contract, run required validation and revalidation, regenerate context through its canonical service, and start P6 only if the repair contract is READY and every applicable gate passes. Do not certify or complete P6. Do not push changes.";
}
bool humanAuthority(const QJsonObject& event)
{
    return event.value("eventType") == "ADMIN_OVERRIDE" && event.value("authority") == "ADMIN_OVERRIDE"
        && event.value("administrator") == "Admin Morgan Lindbom"
        && event.value("provenance").toObject().value("actor") == "human"
        && ProjectMemory().isVerifiedAdministrativeOverride(event.value("instruction").toString())
        && event.value("preColdStartValidation").toObject().value("status") == "PASS"
        && event.value("preMemoryValidation").toObject().value("status") == "PASS";
}
bool scopedP6Authority(const QJsonObject& event)
{
    return humanAuthority(event) && event.value("instruction") == p6ScopedAuthorityInstruction()
        && event.value("scope") == "P6-PREP-REMEDIATION-2026-10-02-01"
        && event.value("requestedAction") == "Prepare exact source/test repair contract using canonical configuration/context services; implement only after READY; no CMake write until separately enforced P6 grant; no P6 start until every gate passes"
        && event.value("affectedFiles").toArray().contains("CMakeLists.txt") && event.value("affectedSystems").toArray().contains("P6");
}
QJsonObject uniqueEvent(const QList<QJsonObject>& events, const QString& id)
{
    QJsonObject found;
    for (const auto& event : events) if (event.value("eventId") == id) {
        if (!found.isEmpty()) return {}; found = event;
    }
    return found;
}
bool currentGrantMemory(const ProjectModel& model, QString* error)
{
    const auto report = ProjectMemory().validate(root(model), error, false);
    bool freshCold = false;
    for (const auto& value : report.value("checks").toArray()) {
        const auto check = value.toObject(); freshCold |= check.value("name") == "cold-start-fresh" && check.value("status") == "PASS";
    }
    if (!error->isEmpty() || report.value("status") != "PASS" || !freshCold) {
        *error = "Protected authority requires coherent current memory and enabled fresh cold-start validation"; return false;
    }
    return true;
}
QJsonObject checkedGrant(const ProjectModel& model, const WorkerTaskRequest& task, QString* error,
                         const QJsonObject* consumedContract = nullptr)
{
    if (task.protectedGrantId.isEmpty()) return {};
    if (!model.workerNameSuffix().isEmpty() || AramfPaths::runtimeWorkerDirectoryName() != worker(model)) {
        *error = "Protected P6 grants require the default project-local authority store"; return {};
    }
    QString readError; const auto events = ProjectMemory().events(root(model), &readError);
    QJsonObject grant, issuance, consumption; int uses = 0;
    for (const auto& event : events) {
        if (event.value("eventType") == "PROTECTED_GRANT_CONSUMED" && event.value("grantId") == task.protectedGrantId) { ++uses; consumption = event; }
        const auto candidate = event.value("protectedPathGrant").toObject();
        if (candidate.value("grantId") != task.protectedGrantId) continue;
        if (!grant.isEmpty()) { *error = "Duplicate grant identity"; return {}; }
        grant = candidate; issuance = event;
    }
    const bool linked = grant.value("schemaVersion").toInt() == 2;
    QJsonObject authority = linked ? uniqueEvent(events, grant.value("authorityEventId").toString()) : issuance;
    bool authorityValid = humanAuthority(authority);
    if (linked) {
        QJsonObject completed; int completions = 0, issuances = 0;
        for (const auto& event : events) {
            if (event.value("eventType") == "PROTECTED_GRANT_ISSUED"
                && event.value("authorityEventId") == grant.value("authorityEventId")) ++issuances;
            if (event.value("eventType") == "PROTECTED_GRANT_VALIDATED" && event.value("grantId") == task.protectedGrantId) { ++completions; completed = event; }
            if ((event.value("eventType") == "PROTECTED_GRANT_REVOKED" || event.value("eventType") == "PROTECTED_GRANT_FAILED")
                && event.value("grantId") == task.protectedGrantId) { *error = "Protected grant is revoked or failed"; return {}; }
            if (event.value("eventType") == "PROTECTED_AUTHORITY_REVOKED" && event.value("authorityEventId") == grant.value("authorityEventId")) {
                *error = "Original protected-path authority was revoked"; return {};
            }
        }
        const QJsonObject service{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}};
        authorityValid &= scopedP6Authority(authority) && exactEventHash(authority) == grant.value("authorityEventFingerprint")
            && issuance.value("eventType") == "PROTECTED_GRANT_ISSUED" && issuance.value("provenance") == service
            && issuance.value("authorityEventId") == authority.value("eventId")
            && issuance.value("authorityEventFingerprint") == exactEventHash(authority)
            && issuances == 1 && completions == 1 && completed.value("provenance") == service
            && completed.value("issuedEventId") == issuance.value("eventId") && completed.value("issuedEventFingerprint") == exactEventHash(issuance)
            && completed.value("authorityEventId") == authority.value("eventId")
            && completed.value("authorityEventFingerprint") == exactEventHash(authority)
            && completed.value("coldStartValidation").toObject().value("status") == "PASS"
            && completed.value("memoryValidation").toObject().value("status") == "PASS"
            && currentGrantMemory(model, &readError);
    } else {
        for (const auto& event : events)
            if ((event.value("eventType") == "PROTECTED_GRANT_REVOKED" && event.value("grantId") == task.protectedGrantId)
                || (event.value("eventType") == "PROTECTED_AUTHORITY_REVOKED" && event.value("authorityEventId") == authority.value("eventId"))) {
                *error = "Protected grant or original authority was revoked"; return {};
            }
    }
    const auto expires = QDateTime::fromString(grant.value("expiresAt").toString(), Qt::ISODate);
    const auto issued = QDateTime::fromString(grant.value("issuedAt").toString(), Qt::ISODate);
    QByteArray registration = p6Registration();
    if (!linked && grant.value("registrationFingerprint") != digest(registration)) {
        registration.replace("${ARAMF_CORE_SOURCES} src/core/WorkerTaskServices.h src/core/WorkerTaskServices.cpp", "${ARAMF_CORE_SOURCES}");
    }
    if (!readError.isEmpty() || grant.isEmpty() || !authorityValid
        || (grant.value("schemaVersion").toInt() != 1 && !linked) || grant.value("projectId") != model.projectId()
        || grant.value("projectRoot") != root(model) || grant.value("workerIdentity") != worker(model)
        || grant.value("requestFingerprint") != grantRequestFingerprint(task)
        || grant.value("path") != "CMakeLists.txt" || grant.value("operation") != "MODIFY"
        || grant.value("purpose") != "P6_BUILD_TEST_REGISTRATION" || !expires.isValid() || !issued.isValid()
        || issued > QDateTime::currentDateTimeUtc() || expires <= QDateTime::currentDateTimeUtc()
        || issued.secsTo(expires) <= 0 || issued.secsTo(expires) > 86400
        || grant.value("registrationFingerprint") != digest(registration)) {
        *error = "Protected grant authority, binding or lifetime is invalid"; return {};
    }
    QFile cmake(pathFor(model, "CMakeLists.txt"));
    if (!cmake.open(QIODevice::ReadOnly)) { *error = "Cannot inspect protected content"; return {}; }
    const auto bytes = cmake.readAll(); const QString hash = digest(bytes);
    const bool baseMatches = hash == grant.value("baseFingerprint") && digest(bytes + registration) == grant.value("resultFingerprint");
    const bool resultMatches = hash == grant.value("resultFingerprint") && bytes.endsWith(registration)
        && digest(bytes.left(bytes.size() - registration.size())) == grant.value("baseFingerprint");
    if (cmake.error() != QFileDevice::NoError || (!baseMatches && !resultMatches)) {
        *error = "Protected grant does not describe the exact registration edit"; return {};
    }
    grant.insert("authorityEventId", authority.value("eventId"));
    if (linked) { grant.insert("issuanceEventId", issuance.value("eventId")); grant.insert("issuanceEventFingerprint", exactEventHash(issuance)); }
    if (uses) {
        const QJsonObject expectedProvenance{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}};
        const auto report = consumption.value("verifiedPostflight").toObject();
        QFile eventLog(pathFor(model, "ARAMF_WORKER/memory/event-log.jsonl"));
        const qint64 prefixSize = static_cast<qint64>(consumption.value("eventPrefixBytes").toDouble(-1));
        bool prefixValid = prefixSize >= 0 && eventLog.open(QIODevice::ReadOnly) && prefixSize < eventLog.size();
        if (prefixValid) {
            const auto prefix = eventLog.read(prefixSize);
            prefixValid = prefix.size() == prefixSize && digest(prefix) == consumption.value("eventPrefixFingerprint")
                && QJsonDocument::fromJson(eventLog.readLine()).object().value("eventId") == consumption.value("eventId")
                && eventLog.error() == QFileDevice::NoError;
        }
        if (uses != 1 || !consumedContract || consumption.value("contractId") != consumedContract->value("contractId")
            || consumption.value("contractFingerprint") != exactEventHash(*consumedContract)
            || consumption.value("requestFingerprint") != grantRequestFingerprint(task)
            || consumption.value("authorityEventId") != authority.value("eventId")
            || consumption.value("authorityEventFingerprint") != exactEventHash(authority)
            || consumption.value("grantFingerprint") != exactEventHash(grant)
            || consumption.value("provenance") != expectedProvenance || !resultMatches || !prefixValid
            || report.value("contractId") != consumedContract->value("contractId") || report.value("status") != "PASS"
            || (report.value("completionState") != "VERIFIED" && report.value("completionState") != "CERTIFIED")
            || !report.value("missingEvidence").toArray().isEmpty() || consumption.value("resultFingerprint") != report.value("resultFingerprint")
            || consumption.value("postflightFingerprint") != exactEventHash(report)) {
            *error = "Protected grant was consumed without this exact unique verified contract binding"; return {};
        }
        for (const auto& item : report.value("acceptedEvidence").toArray()) {
            const auto entry = item.toObject();
            if (!safePath(root(model), entry.value("artifact").toString())
                || fileHash(pathFor(model, entry.value("artifact").toString())) != entry.value("artifactFingerprint")) {
                *error = "Consumed grant evidence changed after verification"; return {};
            }
        }
    }
    return grant;
}

// Restrict refresh authority to artifacts with canonical service producers.

// This fixed inventory prevents an administrator approval from turning an arbitrary file into generated output.
QStringList reauthorizationOutputs()
{
    return {"ARAMF_WORKER/project.json", "ARAMF_WORKER/worker-manifest.json",
        "ARAMF_WORKER/routing/scope-routes.json", "ARAMF_WORKER/routing/task-routes.json",
        "ARAMF_WORKER/context/context-index.json", "ARAMF_WORKER/context/compressed-context.json",
        "ARAMF_WORKER/context/freshness.json", "ARAMF_WORKER/context/agent-adapters.json",
        "ARAMF_WORKER/context/task-dag.json", "ARAMF_WORKER/verification/latest-validation.json",
        "ARAMF_WORKER/verification/verification-result.json", "ARAMF_WORKER/verification/generation-state.json"};
}

// Schema 2 is an explicit successor for a retained failed run. Its observation
// set is not a permission set: only ordinary mapped source files are writable.
QJsonObject repairSuccessorGrant(const ProjectModel& model, const QString& id,
                                 const QJsonObject* successor, QString* error);
QStringList repairSourceFiles();
QStringList repairSourceFiles(const QString& profileId);
bool refreshProtectedMutation(const ProjectModel& model, QJsonObject* cold, QJsonObject* memory, QString* error, bool context);

// Read a previously approved grant from append-only history and reject replay.

// Authority is recovered from a unique ADMIN_OVERRIDE event; serialized contract fields alone never grant access.
QJsonObject reauthorizationGrant(const ProjectModel& model, const QString& id, QString* error)
{
    const auto events = ProjectMemory().events(root(model), error);
    QJsonObject grant, authority, validationEvent;
    int validationEvents = 0;
    for (const auto& event : events) {
        if ((event.value("eventType") == "TASK_REAUTHORIZATION_BOUND"
             || event.value("eventType") == "TASK_REAUTHORIZATION_CONSUMED"
             || event.value("eventType") == "TASK_REAUTHORIZATION_REVOKED")
            && event.value("authorizationId").toString() == id) {
            if (error) *error = "Reauthorization approval was already bound, consumed or revoked";
            return {};
        }
        const auto candidate = event.value("taskReauthorizationGrant").toObject();
        if (candidate.value("authorizationId").toString() != id) continue;
        if (!grant.isEmpty()) { if (error) *error = "Duplicate reauthorization authority"; return {}; }
        grant = candidate; authority = event;
    }
    for (const auto& event : events)
        if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED"
            && event.value("authorizationId").toString() == id) {
            ++validationEvents;
            validationEvent = event;
        }
    if (grant.value("schemaVersion").toInt() == 2)
        return repairSuccessorGrant(model, id, nullptr, error);
    const auto expires = QDateTime::fromString(grant.value("expiresAt").toString(), Qt::ISODate);
    const auto issued = QDateTime::fromString(grant.value("issuedAt").toString(), Qt::ISODate);
    if (grant.isEmpty() || (error && !error->isEmpty()) || authority.value("eventType") != "ADMIN_OVERRIDE"
        || authority.value("authority") != "ADMIN_OVERRIDE"
        || authority.value("administrator") != "Admin Morgan Lindbom"
        || !ProjectMemory().isVerifiedAdministrativeOverride(authority.value("instruction").toString())
        || authority.value("provenance").toObject().value("actor") != "human"
        || authority.value("preMemoryValidation").toObject().value("status") != "PASS"
        || authority.value("preColdStartValidation").toObject().value("status") != "PASS"
        || validationEvents != 1 || validationEvent.value("authorityEventId") != authority.value("eventId")
        || validationEvent.value("originContractId") != grant.value("originContractId")
        || validationEvent.value("originContractFingerprint") != grant.value("originContractFingerprint")
        || validationEvent.value("originPostflightFingerprint") != grant.value("originPostflightFingerprint")
        || validationEvent.value("postMemoryValidation").toObject().value("status") != "PASS"
        || validationEvent.value("postColdStartValidation").toObject().value("status") != "PASS"
        || validationEvent.value("provenance").toObject().value("actor") != "system"
        || validationEvent.value("provenance").toObject().value("tool") != "WorkerTaskServices"
        || grant.value("schemaVersion").toInt() != 1 || grant.value("projectId") != model.projectId()
        || grant.value("projectRoot") != root(model) || grant.value("workerIdentity") != worker(model)
        || !issued.isValid() || issued > QDateTime::currentDateTimeUtc()
        || !expires.isValid() || expires <= QDateTime::currentDateTimeUtc()
        || issued.secsTo(expires) <= 0 || issued.secsTo(expires) > 86400) {
        if (error && error->isEmpty()) *error = QStringLiteral("Reauthorization identity, validation, project binding or lifetime is invalid (pre-memory=%1 pre-cold=%2 post-memory=%3 post-cold=%4 provenance=%5 issued=%6 expires=%7 project=%8 root=%9 worker=%10)")
            .arg(authority.value("preMemoryValidation").toObject().value("status").toString(),
                 authority.value("preColdStartValidation").toObject().value("status").toString(),
                 validationEvent.value("postMemoryValidation").toObject().value("status").toString(),
                 validationEvent.value("postColdStartValidation").toObject().value("status").toString(),
                 authority.value("provenance").toObject().value("actor").toString(),
                 grant.value("issuedAt").toString(), grant.value("expiresAt").toString(),
                 grant.value("projectId").toString(), grant.value("projectRoot").toString(),
                 grant.value("workerIdentity").toString());
        return {};
    }
    grant.insert("authorityEventId", authority.value("eventId"));
    return grant;
}

// Verify a grant that has already been bound to exactly one successor contract.

// Postflight is read-only here: it recovers both immutable audit records and rejects every ambiguous or replayed lifecycle.
QJsonObject boundReauthorizationGrant(const ProjectModel& model, const QJsonObject& contract, QJsonArray& errors)
{
    const QJsonObject link = contract.value("reauthorization").toObject();
    const QString id = link.value("authorizationId").toString();
    if (id.isEmpty()) {
        issue(errors, "REAUTHORIZATION_INVALID", "Successor contract has no authorization link.");
        return {};
    }
    QString readError;
    const auto events = ProjectMemory().events(root(model), &readError);
    if (!readError.isEmpty()) {
        issue(errors, "REAUTHORIZATION_AUDIT_INVALID", readError);
        return {};
    }
    QJsonObject grant, approval, bindingEvent, validationEvent;
    int approvals = 0, bindings = 0, successorBindings = 0, validationEvents = 0;
    bool revoked = false;
    for (const auto& event : events) {
        const QJsonObject candidate = event.value("taskReauthorizationGrant").toObject();
        if (candidate.value("authorizationId").toString() == id) {
            ++approvals;
            grant = candidate;
            approval = event;
        }
        if (event.value("authorizationId").toString() == id) {
            if (event.value("eventType") == "TASK_REAUTHORIZATION_BOUND") {
                ++bindings;
                bindingEvent = event;
            }
            if (event.value("eventType") == "TASK_REAUTHORIZATION_REVOKED"
                || event.value("eventType") == "TASK_REAUTHORIZATION_CONSUMED") revoked = true;
            if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED") {
                ++validationEvents;
                validationEvent = event;
            }
        }
        if (event.value("eventType") == "TASK_REAUTHORIZATION_BOUND"
            && event.value("contractId") == contract.value("contractId")) ++successorBindings;
    }
    if (grant.value("schemaVersion").toInt() == 2) {
        auto result = repairSuccessorGrant(model, id, &contract, &readError);
        if (result.isEmpty()) issue(errors, "REAUTHORIZATION_AUDIT_INVALID", readError);
        return result;
    }
    const QSet<QString> grantKeys{"schemaVersion", "authorizationId", "projectId", "projectRoot", "workerIdentity",
        "originContractId", "originContractFingerprint", "originPostflightFingerprint", "originContractSnapshot",
        "originPostflightSnapshot", "originEvidenceSnapshot", "originEvidenceBytes", "task", "sourceFiles", "canonicalInputs", "operations",
        "approvedPathFingerprints", "binding", "issuedAt", "expiresAt"};
    QSet<QString> actualGrantKeys;
    for (auto it = grant.begin(); it != grant.end(); ++it) actualGrantKeys.insert(it.key());
    const auto expires = QDateTime::fromString(grant.value("expiresAt").toString(), Qt::ISODate);
    const auto issued = QDateTime::fromString(grant.value("issuedAt").toString(), Qt::ISODate);
    const QJsonObject originContract = grant.value("originContractSnapshot").toObject();
    const QJsonObject originReport = grant.value("originPostflightSnapshot").toObject();
    const QJsonObject originalBinding = grant.value("binding").toObject();
    const QString originId = grant.value("originContractId").toString();
    const QStringList files = strings(grant.value("sourceFiles"));
    const QStringList inputs = strings(grant.value("canonicalInputs"));
    const QJsonObject operations = grant.value("operations").toObject();
    QStringList scope = files + inputs;
    scope.removeDuplicates();
    QJsonObject approvedHashes = grant.value("approvedPathFingerprints").toObject();
    QSet<QString> hashKeys;
    for (auto it = approvedHashes.begin(); it != approvedHashes.end(); ++it) hashKeys.insert(it.key());
    QSet<QString> operationKeys;
    for (auto it = operations.begin(); it != operations.end(); ++it) operationKeys.insert(it.key());
    const bool grantShapeValid = actualGrantKeys == grantKeys && !id.isEmpty() && approvals == 1 && bindings == 1
        && successorBindings == 1 && !revoked
        && grant.value("schemaVersion").toInt() == 1 && grant.value("authorizationId") == id
        && hashKeys == QSet<QString>(scope.cbegin(), scope.cend())
        && operationKeys == QSet<QString>(scope.cbegin(), scope.cend())
        && strings(grant.value("sourceFiles")).size() == grant.value("sourceFiles").toArray().size()
        && strings(grant.value("canonicalInputs")).size() == grant.value("canonicalInputs").toArray().size()
        && !files.isEmpty() && !inputs.isEmpty() && approvedHashes.size() == scope.size()
        && expires.isValid() && issued.isValid() && issued <= QDateTime::currentDateTimeUtc()
        && expires > QDateTime::currentDateTimeUtc() && issued.secsTo(expires) <= 86400
        && grant.value("projectId") == model.projectId() && grant.value("projectRoot") == root(model)
        && grant.value("workerIdentity") == worker(model)
        && originContract.value("contractId") == originId && fingerprint([&] { auto value = originContract; value.remove("contractId"); return value; }()) == originId
        && originReport.value("contractId") == originId && originReport.value("status") == "FAIL"
        && originReport.value("authority") == "DERIVED"
        && fingerprint(originReport) == grant.value("originPostflightFingerprint").toString()
        && fingerprint(originContract) == grant.value("originContractFingerprint").toString()
        && originContract.value("preflight").toObject().value("status").toString().startsWith("READY")
        && grant.value("originPostflightFingerprint") == link.value("originPostflightFingerprint")
        && grant.value("originContractFingerprint") == link.value("originContractFingerprint")
        && grant.value("originContractId") == link.value("originContractId")
        && grant.value("originEvidenceSnapshot") == originReport.value("acceptedEvidence")
        && originalBinding.value("projectRoot") == root(model)
        && originalBinding.value("projectId") == model.projectId()
        && originalBinding.value("workerIdentity") == worker(model)
        && semantic(approval.value("affectedFiles")) == semantic(array(scope))
        && bindingEvent.value("eventType") == "TASK_REAUTHORIZATION_BOUND"
        && bindingEvent.value("authorityEventId") == approval.value("eventId")
        && bindingEvent.value("contractId") == contract.value("contractId")
        && bindingEvent.value("projectId") == model.projectId()
        && bindingEvent.value("provenance").toObject().value("actor") == "system"
        && bindingEvent.value("provenance").toObject().value("tool") == "WorkerTaskServices"
        && bindingEvent.value("originContractId") == originId
        && bindingEvent.value("originContractFingerprint") == grant.value("originContractFingerprint")
        && bindingEvent.value("originPostflightFingerprint") == grant.value("originPostflightFingerprint");
    if (approval.value("eventType") != "ADMIN_OVERRIDE" || approval.value("authority") != "ADMIN_OVERRIDE"
        || approval.value("administrator") != "Admin Morgan Lindbom"
        || !ProjectMemory().isVerifiedAdministrativeOverride(approval.value("instruction").toString())
        || approval.value("provenance").toObject().value("actor") != "human"
        || approval.value("preMemoryValidation").toObject().value("status") != "PASS"
        || approval.value("preColdStartValidation").toObject().value("status") != "PASS"
        || validationEvents != 1 || validationEvent.value("authorityEventId") != approval.value("eventId")
        || validationEvent.value("originContractId") != grant.value("originContractId")
        || validationEvent.value("originContractFingerprint") != grant.value("originContractFingerprint")
        || validationEvent.value("originPostflightFingerprint") != grant.value("originPostflightFingerprint")
        || validationEvent.value("postMemoryValidation").toObject().value("status") != "PASS"
        || validationEvent.value("postColdStartValidation").toObject().value("status") != "PASS"
        || validationEvent.value("provenance").toObject().value("actor") != "system"
        || validationEvent.value("provenance").toObject().value("tool") != "WorkerTaskServices"
        || !grantShapeValid) {
        issue(errors, "REAUTHORIZATION_AUDIT_INVALID", "Grant authority, origin records, lifetime or unique binding failed validation.");
        return {};
    }
    const QJsonObject evidenceBytes = grant.value("originEvidenceBytes").toObject();
    QSet<QString> evidenceBytePaths;
    for (auto it = evidenceBytes.begin(); it != evidenceBytes.end(); ++it) evidenceBytePaths.insert(it.key());
    QSet<QString> reportEvidencePaths;
    for (const auto& entryValue : originReport.value("acceptedEvidence").toArray())
        reportEvidencePaths.insert(entryValue.toObject().value("artifact").toString());
    if (evidenceBytePaths != reportEvidencePaths) {
        issue(errors, "REAUTHORIZATION_ORIGIN_EVIDENCE_CHANGED", "Stored origin evidence bytes do not exactly match the report evidence set.");
        return {};
    }
    for (const auto& entryValue : originReport.value("acceptedEvidence").toArray()) {
        const auto entry = entryValue.toObject();
        const QString path = entry.value("artifact").toString();
        const QByteArray expectedBytes = QByteArray::fromBase64(evidenceBytes.value(path).toString().toLatin1());
        QFile evidenceFile(pathFor(model, path));
        if (!safePath(root(model), path) || !evidenceBytes.contains(path) || !evidenceFile.open(QIODevice::ReadOnly)
            || evidenceFile.readAll() != expectedBytes || digest(expectedBytes) != entry.value("artifactFingerprint").toString()) {
            issue(errors, "REAUTHORIZATION_ORIGIN_EVIDENCE_CHANGED", "An original evidence artifact is missing or changed byte-for-byte.", path);
            return {};
        }
    }
    const QSet<QString> linkKeys{"authorizationId", "authorityEventId", "originContractId", "originContractFingerprint",
        "originPostflightFingerprint", "sourceFiles", "canonicalInputs", "operations"};
    QSet<QString> actualLinkKeys;
    for (auto it = link.begin(); it != link.end(); ++it) actualLinkKeys.insert(it.key());
    if (actualLinkKeys != linkKeys || link.value("authorityEventId") != approval.value("eventId")
        || semantic(link.value("sourceFiles")) != semantic(grant.value("sourceFiles"))
        || semantic(link.value("canonicalInputs")) != semantic(grant.value("canonicalInputs"))
        || semantic(link.value("operations")) != semantic(grant.value("operations"))) {
        issue(errors, "REAUTHORIZATION_SCOPE_INVALID", "Successor link differs from the exact approved scope.");
        return {};
    }
    const auto task = grant.value("task").toObject();
    QSet<QString> taskKeys{"goal", "type", "scopes", "files", "definitionOfDone", "history", "destructive"};
    if (task.contains("acceptanceProfile")) taskKeys.insert("acceptanceProfile");
    QSet<QString> actualTaskKeys;
    for (auto it = task.begin(); it != task.end(); ++it) actualTaskKeys.insert(it.key());
    auto expectedRequest = task;
    expectedRequest.insert("reauthorizationId", id);
    if (actualTaskKeys != taskKeys || semantic(contract.value("request")) != semantic(expectedRequest)
        || semantic(contract.value("permittedFiles")) != semantic(array(scope))) {
        issue(errors, "REAUTHORIZATION_SCOPE_INVALID", "Successor request or permitted files differ from the exact approved scope.");
        return {};
    }
    for (const auto& path : scope) {
        const QString operation = operations.value(path).toString();
        const bool validSource = files.contains(path) && (operation == "MODIFY" || operation == "CREATE");
        const bool validInput = inputs.contains(path) && operation == "REGENERATE" && reauthorizationOutputs().contains(path);
        if ((!validSource && !validInput) || !safePath(root(model), path)
            || contract.value("baseline").toObject().value(path).toString() != approvedHashes.value(path).toString()) {
            issue(errors, "REAUTHORIZATION_SCOPE_INVALID", "Operation, canonical producer or successor baseline exceeds the grant.", path);
            return {};
        }
    }
    auto permitted = strings(contract.value("permittedFiles"));
    for (const auto& path : scope) if (!permitted.contains(path)) {
        issue(errors, "REAUTHORIZATION_SCOPE_INVALID", "Authorized file is missing from successor scope.", path);
        return {};
    }
    auto result = grant;
    result.insert("authorityEventId", approval.value("eventId"));
    result.insert("scope", array(scope));
    return result;
}
}

// Persist an administrator-approved, request-bound P6 build grant.

// This operation is separate from read-only preparation. Only the exact seven
// P6 paths are accepted, and a pending grant blocks duplicate issuance for the
// same request. All permissions remain default-deny until normal preparation.
QJsonObject WorkerTaskServices::issueP6BuildGrant(const ProjectModel& model, const WorkerTaskRequest& task,
                                                const QString& instruction, const QString& expiresAt)
{
    const QStringList exact{"CMakeLists.txt", "src/processes/P6/CodeBank.h", "src/processes/P6/CodeBank.cpp",
        "src/processes/P6/CodeBankStorage.cpp", "src/processes/P6/CodeBankVerification.cpp",
        "src/processes/P6/README.md", "tests/P6CodeBankTests.cpp"};
    const auto expires = QDateTime::fromString(expiresAt, Qt::ISODate);
    const auto now = QDateTime::currentDateTimeUtc();
    if (!model.workerNameSuffix().isEmpty() || AramfPaths::runtimeWorkerDirectoryName() != worker(model)
        || !ProjectMemory().isVerifiedAdministrativeOverride(instruction) || !task.protectedGrantId.isEmpty()
        || array(task.files) != array(exact) || task.destructive || !task.relocations.isEmpty()
        || !task.goal.contains("P6") || task.definitionOfDone.isEmpty() || !expires.isValid()
        || now.secsTo(expires) <= 0 || now.secsTo(expires) > 86400 || !safePath(root(model), "CMakeLists.txt"))
        return {{"status", "BLOCKED"}, {"error", "Explicit admin authority, exact P6 request and bounded lifetime required"}};
    QLockFile lock(QDir::tempPath() + "/aramf-protected-grant-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return {{"status", "BLOCKED"}, {"error", "Grant authority is busy"}};
    QString error;
    const auto events = ProjectMemory().events(root(model), &error);
    if (!error.isEmpty()) return {{"status", "BLOCKED"}, {"error", error}};
    for (const auto& event : events) {
        const auto existing = event.value("protectedPathGrant").toObject();
        if (existing.value("requestFingerprint") == grantRequestFingerprint(task))
            return {{"status", "BLOCKED"}, {"error", "This exact task already has a grant; automatic replacement is forbidden"}};
    }
    QFile cmake(pathFor(model, "CMakeLists.txt"));
    if (!cmake.open(QIODevice::ReadOnly)) return {{"status", "BLOCKED"}, {"error", "Cannot read CMake baseline"}};
    const auto bytes = cmake.readAll();
    if (cmake.error() != QFileDevice::NoError || bytes.contains("aramf_p6_tests"))
        return {{"status", "BLOCKED"}, {"error", "Incomplete or already registered CMake baseline"}};
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject grant{{"schemaVersion", 1}, {"grantId", id}, {"projectId", model.projectId()},
        {"projectRoot", root(model)}, {"workerIdentity", worker(model)}, {"requestFingerprint", grantRequestFingerprint(task)},
        {"path", "CMakeLists.txt"}, {"operation", "MODIFY"}, {"purpose", "P6_BUILD_TEST_REGISTRATION"},
        {"issuedAt", now.toString(Qt::ISODate)}, {"expiresAt", expiresAt}, {"baseFingerprint", digest(bytes)},
        {"resultFingerprint", digest(bytes + p6Registration())}, {"registrationFingerprint", digest(p6Registration())}};
    QJsonObject audit;
    if (!ProjectMemory().recordAdministrativeOverride(root(model), instruction, "CMakeLists.txt READ NEVER_TOUCH",
        "Exact P6 build/test registration only", "P6-build-registration", "MODIFY CMakeLists.txt with exact approved registration bytes",
        {"CMakeLists.txt"}, {"P6", "build-system"}, false, {{"protectedPathGrant", grant}}, &audit, &error))
        return {{"status", "BLOCKED"}, {"error", error}};
    return {{"status", "PASS"}, {"grant", grant}, {"registration", QString::fromUtf8(p6Registration())}};
}

// Record a single-use Admin authorization bound to an existing failed task.

// The service computes every fingerprint and path digest; caller data contains only exact paths, operations and task intent.
QJsonObject WorkerTaskServices::authorizeReauthorization(const ProjectModel& model, const QJsonObject& originalContract,
                                                         const QJsonObject& originalPostflight,
                                                         const TaskReauthorizationRequest& request,
                                                         const QString& instruction)
{
    const auto contract = originalContract.contains("contract") ? originalContract.value("contract").toObject() : originalContract;
    auto unsignedContract = contract; unsignedContract.remove("contractId");
    const QString contractId = contract.value("contractId").toString();
    const QString postflightId = originalPostflight.value("contractId").toString();
    const auto priorStatus = contract.value("preflight").toObject().value("status").toString();
    if (!ProjectMemory().isVerifiedAdministrativeOverride(instruction)
        || contract.value("schemaVersion").toInt() != 1 || contract.value("authority") != "DERIVED"
        || contractId.isEmpty() || fingerprint(unsignedContract) != contractId
        || (priorStatus != "READY" && priorStatus != "READY_WITH_WARNINGS")
        || originalPostflight.value("schemaVersion").toInt() != 1
        || originalPostflight.value("authority") != "DERIVED" || postflightId != contractId
        || originalPostflight.value("status") != "FAIL" || request.task.destructive
        || !request.task.relocations.isEmpty() || !request.task.protectedGrantId.isEmpty()
        || !request.task.reauthorizationId.isEmpty() || request.task.goal.trimmed().isEmpty()
        || request.task.scopes.isEmpty() || request.task.files.isEmpty() || request.task.definitionOfDone.isEmpty()
        || request.validityMinutes < 1 || request.validityMinutes > 1440) {
        return {{"status", "BLOCKED"}, {"error", "Valid Admin instruction, READY origin, matching failed postflight and bounded exact request are required"}};
    }
    const auto contractRequest = contract.value("request").toObject();
    if (contractRequest.isEmpty() || contract.value("binding").toObject().value("projectId") != model.projectId())
        return {{"status", "BLOCKED"}, {"error", "Origin contract does not belong to this project"}};
    const auto reproducedReport = postflight(model, contract, originalPostflight.value("acceptedEvidence").toArray());
    if (semantic(reproducedReport) != semantic(originalPostflight))
        return {{"status", "BLOCKED"}, {"error", "Origin postflight does not reproduce from the current canonical service and evidence"}};

    const QStringList files = strings(array(request.task.files));
    const QStringList canonical = strings(array(request.canonicalInputs));
    if (files.size() != request.task.files.size() || canonical.size() != request.canonicalInputs.size() || canonical.isEmpty())
        return {{"status", "BLOCKED"}, {"error", "Scope paths must be unique and canonical refresh outputs must be explicit"}};
    const auto previouslyObserved = strings(originalPostflight.value("modifiedFiles"));
    if (previouslyObserved.size() != originalPostflight.value("modifiedFiles").toArray().size())
        return {{"status", "BLOCKED"}, {"error", "Origin report has duplicate or invalid modified-file entries"}};
    QSet<QString> delegatedRecorderPaths;
    const auto policy = mutationPolicy(model);
    // The reproduced report has already validated recorder ownership, canonical
    // state and append-only history. Keep that existing delegation; it is not
    // permission for the caller to write memory files or broaden source scope.
    for (const auto& value : originalPostflight.value("serviceChanges").toArray()) {
        const auto change = value.toObject();
        const auto path = change.value("path").toString();
        if (change.value("service") == "ProjectMemory recorder"
            && change.value("validation") == "canonical-producer-and-boundary"
            && fileRole(model, path, policy).value("writer") == "ProjectMemory recorder") delegatedRecorderPaths.insert(path);
    }
    for (const auto& path : previouslyObserved)
        if (!files.contains(path) && !canonical.contains(path) && !delegatedRecorderPaths.contains(path))
            return {{"status", "BLOCKED"}, {"error", "Authorization must explicitly cover every path in the original failed report: " + path}};
    const auto allowedOutputs = reauthorizationOutputs();
    QJsonObject scopedHashes;
    for (const auto& path : files) {
        QByteArray dirty;
        if (!safePath(root(model), path) || path.startsWith(worker(model) + "/")
            || path == "CMakeLists.txt" || path.startsWith(".git/")
            || path.startsWith(workerPath(model, "custom/"))
            || !git(root(model), {"status", "--porcelain", "--", path}, &dirty) || dirty.trimmed().isEmpty()
            || fileHash(pathFor(model, path)) == "MISSING" || fileHash(pathFor(model, path)) == "UNREADABLE")
            return {{"status", "BLOCKED"}, {"error", "Each source path must be a changed, readable, safe non-protected file: " + path}};
        scopedHashes.insert(path, fileHash(pathFor(model, path)));
    }
    QJsonObject operationMap;
    for (auto it = request.operations.begin(); it != request.operations.end(); ++it) {
        const QString path = it.key(), operation = it.value().toString();
        const bool sourceOperation = files.contains(path) && (operation == "MODIFY" || operation == "CREATE");
        const bool outputOperation = canonical.contains(path) && operation == "REGENERATE";
        if ((!sourceOperation && !outputOperation) || !safePath(root(model), path))
            return {{"status", "BLOCKED"}, {"error", "Operation exceeds the exact source or canonical producer scope: " + path}};
        operationMap.insert(path, operation);
    }
    if (operationMap.size() != files.size() + canonical.size())
        return {{"status", "BLOCKED"}, {"error", "Every scoped source and canonical input requires exactly one allowed operation"}};
    for (const auto& path : canonical) {
        if (!allowedOutputs.contains(path) || operationMap.value(path) != "REGENERATE")
            return {{"status", "BLOCKED"}, {"error", "Canonical input has no approved ARAMF producer: " + path}};
        scopedHashes.insert(path, fileHash(pathFor(model, path)));
    }
    const auto now = QDateTime::currentDateTimeUtc();
    const QString authorizationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject evidenceBytes;
    for (const auto& entryValue : originalPostflight.value("acceptedEvidence").toArray()) {
        const auto entry = entryValue.toObject();
        const QString artifact = entry.value("artifact").toString();
        QFile file(pathFor(model, artifact));
        if (!safePath(root(model), artifact) || !file.open(QIODevice::ReadOnly))
            return {{"status", "BLOCKED"}, {"error", "Cannot preserve an origin evidence artifact: " + artifact}};
        const QByteArray contents = file.readAll();
        if (file.error() != QFileDevice::NoError || digest(contents) != entry.value("artifactFingerprint").toString())
            return {{"status", "BLOCKED"}, {"error", "Origin evidence artifact changed before authorization: " + artifact}};
        evidenceBytes.insert(artifact, QString::fromLatin1(contents.toBase64()));
    }
    const QJsonObject grant{{"schemaVersion", 1}, {"authorizationId", authorizationId},
        {"projectId", model.projectId()}, {"projectRoot", root(model)}, {"workerIdentity", worker(model)},
        {"originContractId", contractId}, {"originContractFingerprint", fingerprint(contract)},
        {"originPostflightFingerprint", fingerprint(originalPostflight)}, {"originContractSnapshot", contract},
        {"originPostflightSnapshot", originalPostflight}, {"originEvidenceSnapshot", originalPostflight.value("acceptedEvidence")},
        {"originEvidenceBytes", evidenceBytes},
        {"task", request.task.toJson()},
        {"sourceFiles", array(files)}, {"canonicalInputs", array(canonical)}, {"operations", operationMap},
        {"approvedPathFingerprints", scopedHashes}, {"binding", binding(model)}, {"issuedAt", now.toString(Qt::ISODate)},
        {"expiresAt", now.addSecs(request.validityMinutes * 60).toString(Qt::ISODate)}};
    QJsonObject audit; QString error;
    if (!ProjectMemory().recordAdministrativeOverride(root(model), instruction,
            "A READY TaskContract must precede implementation unless this exact reauthorization protocol applies",
            "Review and authorize existing modifications without replacing their history or caller-provided hashes",
            "task-reauthorization", "Create one single-use exact-scope successor TaskContract",
            files + canonical, {"WorkerTaskServices", "TaskContract", "GenerationServices", "ContextCoordinationService", "VerificationServices"},
            false, {{"taskReauthorizationGrant", grant}}, &audit, &error))
        return {{"status", "BLOCKED"}, {"error", error}};
    QString eventError;
    auto events = ProjectMemory().events(root(model), &eventError);
    if (!eventError.isEmpty()) return {{"status", "BLOCKED"}, {"error", eventError}};
    QJsonObject approvalEvent;
    for (auto it = events.crbegin(); it != events.crend(); ++it)
        if (it->value("taskReauthorizationGrant").toObject().value("authorizationId") == authorizationId)
            { approvalEvent = *it; break; }
    if (approvalEvent.isEmpty() || audit.value("status") != "PASS"
        || audit.value("postMemoryValidation").toObject().value("status") != "PASS"
        || audit.value("postColdStartValidation").toObject().value("status") != "PASS")
        return {{"status", "BLOCKED"}, {"error", "Admin audit event or post-authorization validations could not be read back"}};
    const QJsonObject validated{{"authorizationId", authorizationId}, {"authorityEventId", approvalEvent.value("eventId")},
        {"originContractId", contractId}, {"originContractFingerprint", fingerprint(contract)},
        {"originPostflightFingerprint", fingerprint(originalPostflight)},
        {"postMemoryValidation", audit.value("postMemoryValidation")},
        {"postColdStartValidation", audit.value("postColdStartValidation")},
        {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}}}};
    if (!ProjectMemory().appendEvent(root(model), "TASK_REAUTHORIZATION_AUTHORIZED",
            "Post-authorization memory and cold-start validations passed", validated, &error))
        return {{"status", "BLOCKED"}, {"error", error}};
    events = ProjectMemory().events(root(model), &eventError);
    int persistedGrants = 0, persistedValidations = 0;
    for (const auto& event : events) {
        persistedGrants += event.value("taskReauthorizationGrant").toObject().value("authorizationId") == authorizationId;
        persistedValidations += event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED"
            && event.value("authorizationId") == authorizationId;
    }
    if (!eventError.isEmpty() || persistedGrants != 1 || persistedValidations != 1)
        return {{"status", "BLOCKED"}, {"error", eventError.isEmpty() ? "Append-only authorization chain is ambiguous" : eventError}};
    return {{"status", "PASS"}, {"authorizationId", authorizationId},
        {"authorityEventId", approvalEvent.value("eventId")}, {"expiresAt", grant.value("expiresAt")}};
}

// Create a new derived contract from a verified, unused Admin approval.

// The new baseline is captured internally after approval; the original contract and failed postflight remain untouched.
QJsonObject WorkerTaskServices::prepareReauthorized(const ProjectModel& model, const QString& authorizationId,
                                                    const QJsonObject& originalContract,
                                                    const QJsonObject& originalPostflight)
{
    QLockFile lock(QDir::tempPath() + "/aramf-reauthorization-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0))
        return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"},
            {"errors", QJsonArray{QJsonObject{{"code", "REAUTHORIZATION_BUSY"}, {"message", "Reauthorization binding is busy"}}}}}}};
    QString error;
    const auto grant = reauthorizationGrant(model, authorizationId, &error);
    const auto contract = originalContract.contains("contract") ? originalContract.value("contract").toObject() : originalContract;
    if (grant.isEmpty()) return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"}, {"errors", QJsonArray{QJsonObject{{"code", "REAUTHORIZATION_INVALID"}, {"message", error}}}}}}};
    if (grant.value("originContractId") != contract.value("contractId")
        || grant.value("originContractFingerprint") != fingerprint(contract)
        || grant.value("originPostflightFingerprint") != fingerprint(originalPostflight)
        || originalPostflight.value("contractId") != contract.value("contractId")
        || grant.value("projectId") != model.projectId() || grant.value("projectRoot") != root(model))
        return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"}, {"errors", QJsonArray{QJsonObject{{"code", "REAUTHORIZATION_BINDING_MISMATCH"}, {"message", "Origin contract, postflight or project differs from approved authority"}}}}}}};
    const auto hashes = grant.value("approvedPathFingerprints").toObject();
    for (const auto& path : strings(grant.value("sourceFiles")) + strings(grant.value("canonicalInputs")))
        if (hashes.value(path).toString() != fileHash(pathFor(model, path)))
            return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"}, {"errors", QJsonArray{QJsonObject{{"code", "REAUTHORIZATION_SOURCE_CHANGED"}, {"message", "Approved source changed after Admin review"}, {"path", path}}}}}}};
    WorkerTaskRequest task = WorkerTaskRequest::fromJson(grant.value("task").toObject());
    task.reauthorizationId = authorizationId;
    task.internalReauthorizationPreparation = true;
    QJsonObject successor = prepare(model, task);
    const QString preflight = successor.value("preflight").toObject().value("status").toString();
    if (!preflight.startsWith("READY")) return successor;
    successor.insert("reauthorization", QJsonObject{{"authorizationId", authorizationId},
        {"authorityEventId", grant.value("authorityEventId")}, {"originContractId", grant.value("originContractId")},
        {"originContractFingerprint", grant.value("originContractFingerprint")},
        {"originPostflightFingerprint", grant.value("originPostflightFingerprint")},
        {"sourceFiles", grant.value("sourceFiles")}, {"canonicalInputs", grant.value("canonicalInputs")},
        {"operations", grant.value("operations")}});
    auto idContent = successor; idContent.remove("contractId");
    const QString contractId = fingerprint(idContent);
    successor.insert("contractId", contractId);
    const QJsonObject consumed{{"authorizationId", authorizationId}, {"contractId", contractId},
        {"authorityEventId", grant.value("authorityEventId")}, {"projectId", model.projectId()},
        {"originContractId", grant.value("originContractId")},
        {"originContractFingerprint", grant.value("originContractFingerprint")},
        {"originPostflightFingerprint", grant.value("originPostflightFingerprint")},
        {"provenance", QJsonObject{{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}}}};
    if (!ProjectMemory().appendEvent(root(model), "TASK_REAUTHORIZATION_BOUND",
            "Admin reauthorization bound to successor TaskContract", consumed, &error))
        return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"}, {"errors", QJsonArray{QJsonObject{{"code", "REAUTHORIZATION_AUDIT_FAILED"}, {"message", error}}}}}}};
    if (grant.value("schemaVersion").toInt() == 2) {
        QJsonObject cold, memory;
        if (!refreshProtectedMutation(model, &cold, &memory, &error, true))
            return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"}, {"errors", QJsonArray{QJsonObject{{"code", "REAUTHORIZATION_AUDIT_FAILED"}, {"message", error}}}}}}};
    }
    QJsonArray auditErrors;
    if (boundReauthorizationGrant(model, successor, auditErrors).isEmpty())
        return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "BLOCKED"}, {"errors", auditErrors}}}};
    return successor;
}

QJsonObject ChangeImpactResolver::resolve(const ProjectModel& model, const WorkerTaskRequest& task)
{
    const auto context = WorkerContextResolver::resolveImpact(pathFor(model, worker(model)), task.scopes);
    QStringList potential, generated, tests, traits, affected = strings(context.value("scopes"));
    for (const auto& value : context.value("resolvedRoutes").toArray()) {
        const auto metadata = value.toObject().value("taskMetadata").toObject();
        potential << strings(metadata.value("files"));
        generated << strings(metadata.value("generatedArtifacts"));
        tests << strings(metadata.value("tests"));
        traits << strings(metadata.value("riskTraits"));
    }
    QStringList unrelated = model.ruleConfiguration().projectScopes;
    for (const auto& scope : affected) unrelated.removeAll(scope);
    QStringList transitive = affected;
    for (const auto& scope : task.scopes) transitive.removeAll(scope);
    return {{"directScopes", array(task.scopes)}, {"transitiveScopes", array(transitive)},
        {"affectedScopes", array(affected)}, {"directFiles", array(task.files)}, {"potentialFiles", array(potential)},
        {"generatedArtifacts", array(generated)}, {"instructions", context.value("instructions")},
        {"resources", context.value("resources")}, {"tests", array(tests)}, {"riskTraits", array(traits)},
        {"unrelatedScopes", array(unrelated)}, {"context", context}};
}

QJsonObject WorkerTaskServices::mutationPolicy(const ProjectModel& model)
{
    QJsonObject files;
    const auto manifest = readObject(pathFor(model, workerPath(model, "worker-manifest.json")));
    const auto derived = manifest.value("derivedFiles").toObject();
    const QStringList generated{"project.json", "worker-manifest.json", "routing/task-routes.json", "routing/scope-routes.json",
        "context/context-index.json", "context/compressed-context.json", "context/freshness.json", "context/agent-adapters.json"};
    for (const auto& path : generated) files.insert(workerPath(model, path), role("generated", {"READ", "REGENERATE"}, "GenerationServices"));
    files.insert(workerPath(model, "context/context-index.json"), role("generated", {"READ", "REGENERATE"}, "ContextCoordinationService"));
    files.insert(workerPath(model, "context/compressed-context.json"), role("generated", {"READ", "REGENERATE"}, "ContextCoordinationService"));
    files.insert(workerPath(model, "context/freshness.json"), role("generated", {"READ", "REGENERATE"}, "ContextCoordinationService"));
    files.insert(workerPath(model, "context/agent-adapters.json"), role("generated", {"READ", "REGENERATE"}, "ContextCoordinationService"));
    files.insert(workerPath(model, "context/task-dag.json"), role("generated", {"READ", "REGENERATE"}, "ContextCoordinationService"));
    for (const auto& path : derived) {
        QString relative = path.toString();
        if (relative.startsWith("ARAMF_WORKER/")) relative.replace(0, 12, worker(model));
        if (!files.contains(relative)) files.insert(relative, role("generated", {"READ", "REGENERATE"}, "ARAMF"));
    }
    const auto memory = readObject(pathFor(model, workerPath(model, "memory/memory-contract.json")));
    for (const auto& path : memory.value("ownedFiles").toArray())
        files.insert(workerPath(model, path.toString()), role("governance", {"READ"}, "ProjectMemory recorder"));
    files.insert(workerPath(model, "memory/event-log.jsonl"), role("historical", {"READ_WHEN_REQUIRED", "APPEND_ONLY", "NEVER_REWRITE"}, "ProjectMemory recorder"));
    files.insert(workerPath(model, "memory/decisions.md"), role("governance", {"READ"}, "ProjectMemory decision workflow"));
    files.insert(workerPath(model, "memory/cold-start-validation.json"), role("validation", {"READ", "REGENERATE"}, "ProjectMemory recorder"));
    files.insert(workerPath(model, "verification/latest-validation.json"), role("validation", {"READ", "REGENERATE"}, "VerificationServices"));
    files.insert(workerPath(model, "verification/verification-result.json"), role("validation", {"READ", "REGENERATE"}, "VerificationServices"));
    files.insert(workerPath(model, "certification/certificates.jsonl"), role("historical", {"READ_WHEN_REQUIRED", "APPEND_ONLY", "NEVER_REWRITE"}, "CertificationService"));
    files.insert(workerPath(model, "certification/current-certification-state.json"), role("validation", {"READ", "REGENERATE"}, "CertificationService"));
    return {{"schemaVersion", 1}, {"files", files}, {"defaultActions", QJsonArray{"READ"}},
        {"sourceMutation", "MODIFY/GENERATE only for exact canonical scope files selected in the task."},
        {"deletePolicy", "Denied; requires a separate explicitly authorized maintenance workflow."},
        {"customPolicy", "User-owned; no automatic mutation."}};
}

namespace {
QJsonObject deriveTaskContract(const ProjectModel& model, const WorkerTaskRequest& task, const QJsonObject* consumedContract)
{
    WorkerResourceObservation observation;
    QJsonArray errors;
    if (!QDir(model.projectPath()).exists() || model.projectPath().isEmpty()) {
        issue(errors, "REPOSITORY_MISSING", "Select an existing project root.", {}, "FATAL");
        return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "FATAL"}, {"errors", errors}}}};
    }
    QJsonObject reauth;
    if (!task.reauthorizationId.isEmpty()) {
        if (!task.internalReauthorizationPreparation) {
            issue(errors, "REAUTHORIZATION_INTERNAL_ONLY", "Use the approved reauthorization prepare operation.");
        } else {
            QString reauthError;
            reauth = reauthorizationGrant(model, task.reauthorizationId, &reauthError);
            auto taskIntent = task.toJson(); taskIntent.remove("reauthorizationId");
            if (reauth.isEmpty() || reauth.value("task") != taskIntent
                || reauth.value("binding").toObject() != binding(model))
                issue(errors, "REAUTHORIZATION_INVALID", reauthError.isEmpty() ? "Approval binding or task intent differs." : reauthError);
        }
    }
    if (task.goal.trimmed().isEmpty() || task.scopes.isEmpty() || task.files.isEmpty() || task.definitionOfDone.isEmpty())
        issue(errors, "TASK_CONTRACT_INVALID", "Goal, scopes, exact proposed files and definition of done are required.");
    if (!task.acceptanceProfile.isEmpty()) {
        const auto profile = WorkerTaskServices::acceptanceProfile(task.acceptanceProfile);
        const auto allowed = strings(profile.value("files"));
        bool scopeValid = !profile.isEmpty();
        for (const auto& file : task.files) scopeValid &= allowed.contains(file);
        if (!scopeValid || semantic(array(task.definitionOfDone)) != semantic(profile.value("requirements")))
            issue(errors, "ACCEPTANCE_PROFILE_INVALID", "Profile requires its exact canonical requirements and bounded implementation files; arbitrary prose cannot inherit acceptance.");
    }
    validateCanonical(model, errors, reauth);
    const auto canonicalRules = model.ruleConfiguration();
    for (auto it = canonicalRules.scopeMetadata.begin(); it != canonicalRules.scopeMetadata.end(); ++it) {
        if (!canonicalRules.projectScopes.contains(it.key()) || !it.value().isObject())
            issue(errors, "TASK_METADATA_INVALID", "Scope metadata must belong to an active scope.", it.key());
        const auto metadata = it.value().toObject();
        for (const QString& key : {QStringLiteral("files"), QStringLiteral("affects"), QStringLiteral("tests"), QStringLiteral("riskTraits"), QStringLiteral("generatedArtifacts")}) {
            if (metadata.contains(key) && !metadata.value(key).isArray()) issue(errors, "TASK_METADATA_INVALID", "Scope metadata sets must be arrays.", it.key() + '/' + key);
            for (const auto& value : metadata.value(key).toArray()) {
                if (!value.isString() || value.toString().isEmpty()) issue(errors, "TASK_METADATA_INVALID", "Scope metadata entries must be nonempty strings.", it.key() + '/' + key);
                if ((key == "files" || key == "generatedArtifacts") && !safePath(root(model), value.toString())) issue(errors, "UNSAFE_PATH", "Unsafe canonical file mapping.", value.toString(), "FATAL");
                if (key == "affects" && !canonicalRules.projectScopes.contains(value.toString())) issue(errors, "WORKER_SCOPE_UNRESOLVED", "Scope dependency has no canonical route.", value.toString());
                if (key == "riskTraits" && !QStringList{"memory", "persistence", "hardware", "topology", "governance", "schema", "migration", "cross-project", "runtime", "ui"}.contains(value.toString()))
                    issue(errors, "TASK_METADATA_INVALID", "Unknown risk trait must be resolved before execution.", value.toString());
            }
        }
    }
    auto policy = WorkerTaskServices::mutationPolicy(model);
    QString grantError;
    const auto protectedGrant = checkedGrant(model, task, &grantError, consumedContract);
    if (!grantError.isEmpty()) issue(errors, "PROTECTED_GRANT_INVALID", grantError, "CMakeLists.txt");
    auto impact = ChangeImpactResolver::resolve(model, task);
    QStringList potential = strings(impact.value("potentialFiles"));
    QStringList generated = strings(impact.value("generatedArtifacts"));
    if (!reauth.isEmpty()) {
        potential << strings(reauth.value("sourceFiles"));
        generated << strings(reauth.value("canonicalInputs"));
        impact.insert("potentialFiles", array(potential));
        impact.insert("generatedArtifacts", array(generated));
        QStringList traits = strings(impact.value("riskTraits")); traits << "governance" << "schema";
        impact.insert("riskTraits", array(traits));
        auto policies = policy.value("files").toObject();
        for (const auto& path : strings(reauth.value("canonicalInputs"))) {
            const QString writer = path.contains("/context/") ? "ContextCoordinationService"
                : path.endsWith("/generation-state.json") ? "GenerationServices"
                : path.contains("/verification/") ? "VerificationServices" : "GenerationServices";
            policies.insert(path, role("generated", {"READ", "REGENERATE"}, writer));
        }
        policy.insert("files", policies);
    }
    const auto context = impact.value("context").toObject();
    if (!context.value("valid").toBool()) {
        for (const auto& message : context.value("diagnostics").toArray()) issue(errors, "WORKER_SCOPE_UNRESOLVED", message.toString());
    }
    QJsonArray permissions;
    QStringList permitted, protectedFiles;
    potential = strings(impact.value("potentialFiles"));
    const auto permissionsConfigured = model.aiConfiguration().permissions;
    if (!permissionsConfigured.contains("read-project-files") || !permissionsConfigured.contains("modify-files"))
        issue(errors, "AGENT_PERMISSION_MISSING", "Canonical AI permissions must allow read-project-files and modify-files.");
    for (const auto& file : strings(array(task.files))) {
        auto ownership = fileRole(model, file, policy);
        const bool source = !file.startsWith(worker(model) + '/') && file != "AGENTS.md"
            && ownership.value("owner").toString() == "user" && !strings(ownership.value("actions")).contains("NEVER_TOUCH");
        if (!safePath(root(model), file)) issue(errors, "UNSAFE_PATH", "Proposed path is unsafe or escapes the project.", file, "FATAL");
        else if (!potential.contains(file)) { issue(errors, "FILE_OWNERSHIP_UNKNOWN", "Proposed file has no ownership mapping in the active task scopes.", file); issue(errors, "TASK_SCOPE_VIOLATION", "Proposed file is not mapped to an affected canonical scope.", file); }
        else if (!source && !(file == "CMakeLists.txt" && !protectedGrant.isEmpty())) issue(errors, "FORBIDDEN_FILE_MODIFICATION", "File must be changed by its canonical owner/service, not the agent.", file);
        else {
            const bool exists = QFileInfo::exists(pathFor(model, file));
            if (!exists && !permissionsConfigured.contains("create-files")) issue(errors, "AGENT_PERMISSION_MISSING", "Creating a new source file requires create-files permission.", file);
            ownership = role("user", exists ? QStringList{"READ", "MODIFY"} : QStringList{"READ", "GENERATE"}, "active-task-agent");
            permitted.append(file);
        }
        ownership.insert("path", file);
        permissions.append(ownership);
        for (const auto& scope : strings(impact.value("unrelatedScopes")))
            if (strings(canonicalRules.scopeMetadata.value(scope).toObject().value("files")).contains(file))
                issue(errors, "TASK_SCOPE_VIOLATION", "Shared file also belongs to an unresolved scope; declare its impact before modifying.", scope);
    }
    if (!reauth.isEmpty()) {
        for (const auto& path : strings(reauth.value("canonicalInputs"))) {
            permitted.append(path);
            auto permission = role("generated", {"READ", "REGENERATE"}, policy.value("files").toObject()
                .value(path).toObject().value("writer").toString());
            permission.insert("path", path);
            permissions.append(permission);
        }
    }
    permitted.removeDuplicates();
    const auto exactPolicies = policy.value("files").toObject();
    protectedFiles << exactPolicies.keys();
    for (const auto& resource : model.resources()) protectedFiles.append(resource.location);
    QStringList traits = strings(impact.value("riskTraits"));
    const auto baseValidation = ValidationRouting::route(task.files, task.type);
    QString risk = "LOW";
    QStringList reasons;
    if (strings(impact.value("affectedScopes")).size() > 1 || baseValidation.level == ValidationLevel::Subsystem) { risk = "MEDIUM"; reasons << "shared-component-or-multiple-scopes"; }
    for (const auto& trait : {"memory", "persistence", "hardware", "topology"}) if (traits.contains(trait)) { risk = "HIGH"; reasons << trait; }
    if (strings(impact.value("affectedScopes")).size() >= 3) { risk = "HIGH"; reasons << "three-or-more-scopes"; }
    if (baseValidation.level == ValidationLevel::FullRegression) { risk = "HIGH"; reasons << "canonical-full-regression-route"; }
    for (const auto& trait : {"governance", "schema", "migration", "cross-project"}) if (traits.contains(trait)) { risk = "CRITICAL"; reasons << trait; }
    if (task.destructive) { risk = "CRITICAL"; reasons << "destructive-operation"; issue(errors, "FORBIDDEN_FILE_MODIFICATION", "Destructive tasks require a separate governed maintenance workflow."); }
    auto tests = ValidationRouting::taskPlan(task.files, task.type, impact, risk);
    if (task.acceptanceProfile == "task-p6-requirements-operations/v1" || task.acceptanceProfile == "task-p6-build-registration/v1"
        || task.acceptanceProfile == "task-p6-prerequisite-closure/v1" || task.acceptanceProfile == "task-p6-activation-governance/v1"
        || task.acceptanceProfile == "task-p6-activation-campaign-isolation/v1"
        || task.acceptanceProfile == "task-p6-activation-campaign-isolation/v2") {
        const auto profile = WorkerTaskServices::acceptanceProfile(task.acceptanceProfile);
        tests.insert("completionChecks", array(strings(tests.value("completionChecks")) + strings(profile.value("checks"))));
    }
    QJsonObject evidenceDependencies;
    QStringList taskDependencies = potential + task.files;
    // A profile can require broader observations without granting any writes.
    // This optional field is derived only for newly explicit profile requests;
    // legacy immutable contracts retain their original dependency serialization.
    QJsonObject prerequisiteReadSet;
    const auto readSubjects = strings(WorkerTaskServices::acceptanceProfile(task.acceptanceProfile).value("prerequisiteSubjects"));
    for (const auto& subject : readSubjects) {
        const auto manifest = CertificationSourceManifestProvider::manifest(subject);
        QStringList files = manifest.files;
        for (const auto& file : files) {
            QFileInfo part(pathFor(model, file));
            bool safe = safePath(root(model), file) && part.isFile() && part.isReadable();
            while (part.absoluteFilePath() != root(model) && part.absoluteFilePath() != part.absolutePath()) {
                safe &= !part.isSymLink() && !part.isJunction(); part = QFileInfo(part.absolutePath());
            }
            const QString hash = fileHash(pathFor(model, file));
            if (!safe || !QRegularExpression("^[a-f0-9]{64}$").match(hash).hasMatch())
                issue(errors, "PREREQUISITE_READ_INVALID", "Canonical prerequisite observation requires a safe readable regular source file.", file);
            taskDependencies.append(file);
        }
        prerequisiteReadSet.insert(subject, QJsonObject{{"subject", subject}, {"files", array(files)}});
    }
    for (const auto& value : context.value("resources").toArray()) taskDependencies.append("external:" + value.toObject().value("id").toString());
    for (const auto& check : strings(tests.value("completionChecks"))) evidenceDependencies.insert(check, array(taskDependencies));
    QJsonObject focusedDependencies;
    for (const auto& route : context.value("resolvedRoutes").toArray()) {
        const auto metadata = route.toObject().value("taskMetadata").toObject();
        QStringList dependencies = strings(metadata.value("files"));
        for (const auto& value : context.value("resources").toArray()) {
            const auto resource = value.toObject();
            const auto scopes = strings(resource.value("scopes"));
            if (scopes.isEmpty() || scopes.contains("all") || scopes.contains(route.toObject().value("id").toString()))
                dependencies.append("external:" + resource.value("id").toString());
        }
        for (const auto& check : strings(metadata.value("tests"))) focusedDependencies.insert(check, array(strings(focusedDependencies.value(check)) + dependencies));
    }
    auto sharedImpact = impact; sharedImpact.insert("tests", QJsonArray{});
    const auto sharedChecks = strings(ValidationRouting::taskPlan(task.files, task.type, sharedImpact, risk).value("completionChecks"));
    for (auto it = focusedDependencies.begin(); it != focusedDependencies.end(); ++it)
        if (!sharedChecks.contains(it.key())) evidenceDependencies.insert(it.key(), it.value());
    if (strings(impact.value("tests")).isEmpty()) issue(errors, "REQUIRED_EVIDENCE_UNMAPPED", "Map focused test IDs in canonical scopeMetadata before execution.");

    QJsonArray sourceDependencies;
    QHash<QString, QString> authorities;
    for (const auto& value : context.value("resources").toArray()) {
        const auto resource = value.toObject();
        const QString location = resource.value("location").toString();
        const QString resourceId = resource.value("id").toString();
        const bool remote = location.contains("://");
        const auto observed = remote ? QJsonObject{} : WorkerContextResolver::resourceSnapshot(model, resourceId);
        const QString hash = remote ? QStringLiteral("UNVERIFIED_REMOTE") : observed.value("valid").toBool()
            ? observed.value("fingerprint").toString() : QStringLiteral("UNREADABLE");
        if (remote || hash == "MISSING" || hash == "UNREADABLE") issue(errors, resource.value("role").toString() == "source-of-truth" ? "SOURCE_OF_TRUTH_MISSING" : "USER_SOURCE_MISSING", "Relevant source must be locally available and readable; remote sources require explicit verified local evidence.", location);
        sourceDependencies.append(QJsonObject{{"id", resourceId}, {"source", location}, {"type", resource.value("type")},
            {"fingerprint", hash}, {"authority", resource.value("authority")}, {"manifest", observed}});
        if (resource.value("role").toString() == "source-of-truth") {
            auto scopes = strings(resource.value("scopes"));
            if (scopes.isEmpty() || scopes.contains("all")) scopes = strings(impact.value("affectedScopes"));
            for (const auto& scope : scopes) {
                if (authorities.contains(scope)) issue(errors, "RESOURCE_AUTHORITY_CONFLICT", "Multiple Sources of Truth govern the same task scope without explicit arbitration.", scope);
                authorities.insert(scope, resourceId);
            }
        }
    }
    QJsonArray instructions;
    for (const auto& id : strings(context.value("instructions"))) {
        const auto instruction = id == "aramf-thesis-instruction" ? DocumentInstructions::thesis() : DocumentInstructions::report();
        if (instruction.id != id) issue(errors, "INSTRUCTION_AUTHORITY_CONFLICT", "Instruction ID is not owned by the canonical instruction catalog.", id);
        else instructions.append(DocumentInstructions::toJson(instruction));
    }
    // Legacy decisions without scope metadata remain global: never guess them
    // out of applicability based on words in their summaries.
    QJsonArray decisions;
    QString decisionError;
    const QString oldSuffix = AramfPaths::detail::workerSuffixOverride();
    AramfPaths::setRuntimeWorkerNameSuffix(model.workerNameSuffix());
    const auto currentDecisions = ProjectMemory().currentDecisions(root(model), &decisionError);
    AramfPaths::setRuntimeWorkerNameSuffix(oldSuffix);
    for (const auto& decision : currentDecisions) decisions.append(decision);
    if (!decisionError.isEmpty()) issue(errors, "GOVERNANCE_SOURCE_INVALID", decisionError);
    QStringList constraints{"Do not create a second canonical memory, project configuration, routing or status store.",
        "Do not mutate files outside permittedFiles, including unrelated subsystem source.",
        "Do not overwrite external templates, resources or Sources of Truth.",
        "Do not rewrite event history or directly edit recorder-owned state.",
        "Do not weaken validation or change public behavior outside the definition of done.",
        "Do not claim VERIFIED or CERTIFIED without the required bound evidence.",
        "Do not broaden scope or load the full Worker when routing fails.",
        "Do not grant permissions to unmapped files or override canonical authority."};
    for (const auto& scope : strings(impact.value("unrelatedScopes"))) constraints << QStringLiteral("Do not modify unrelated scope: %1").arg(scope);
    const auto baseline = snapshot(model, errors);
    QJsonArray relocations;
    QSet<QString> endpoints;
    QByteArray revisionBytes;
    if (!task.relocations.isEmpty()) git(root(model), {"rev-parse", "HEAD^{commit}"}, &revisionBytes);
    const QString revision = QString::fromUtf8(revisionBytes).trimmed();
    for (const auto& value : task.relocations) {
        const auto pair = value.toObject();
        const QString oldPath = pair.value("oldPath").toString(), newPath = pair.value("newPath").toString();
        const QString oldKey = oldPath.toCaseFolded(), newKey = newPath.toCaseFolded();
        const QString hash = baseline.value(oldPath).toString();
        const QFileInfo destination(pathFor(model, newPath));
        if (!permitted.contains(oldPath) || !permitted.contains(newPath)
            || !relocationPath(model, newPath) || !committedContent(model, revision, oldPath, hash)
            || !QFileInfo(pathFor(model, oldPath)).isFile() || destination.exists() || destination.isSymLink()
            || oldKey == newKey || endpoints.contains(oldKey) || endpoints.contains(newKey)) {
            issue(errors, "RELOCATION_UNPROVEN", "Relocation requires unique authorized paths, an absent destination and unchanged committed source content.", oldPath);
            continue;
        }
        endpoints.insert(oldKey); endpoints.insert(newKey);
        relocations.append(QJsonObject{{"oldPath", oldPath}, {"newPath", newPath},
            {"contentFingerprint", hash}, {"sourceRevision", revision}});
    }
    QJsonObject historyLengths;
    for (const QString& relative : {QStringLiteral("memory/event-log.jsonl"), QStringLiteral("certification/certificates.jsonl")})
        historyLengths.insert(workerPath(model, relative), static_cast<double>(QFileInfo(pathFor(model, workerPath(model, relative))).size()));
    parallelState(model, baseline, errors);
    QJsonObject contract{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"request", task.toJson()}, {"binding", binding(model)},
        {"protectedGrant", protectedGrant},
        {"impact", impact}, {"permittedFiles", array(permitted)}, {"protectedFiles", array(protectedFiles)}, {"mutationPolicy", policy},
        {"filePermissions", permissions}, {"negativeConstraints", array(constraints)}, {"instructions", instructions},
        {"resources", sourceDependencies}, {"decisions", decisions}, {"decisionSource", workerPath(model, "memory/decisions.md")},
        {"risk", QJsonObject{{"level", risk}, {"reasons", array(reasons)}, {"reviewDepth", risk == "LOW" ? "focused" : "cross-component"}}},
        {"historyFiles", task.history ? QJsonArray{workerPath(model, "memory/event-log.jsonl")} : QJsonArray{}},
        {"validation", tests}, {"requiredEvidence", tests.value("completionChecks")}, {"evidenceDependencies", evidenceDependencies}, {"taskDependencies", array(taskDependencies)},
        {"baseline", baseline}, {"historyLengths", historyLengths},
        {"preflight", QJsonObject{{"status", state(errors)}, {"errors", errors}}}, {"completionState", "NOT_STARTED"}};
    if (!prerequisiteReadSet.isEmpty()) contract.insert("prerequisiteReadSet", prerequisiteReadSet);
    if (!task.relocations.isEmpty()) contract.insert("authorizedRelocations", relocations);
    contract.insert("contractId", fingerprint(contract));
    return contract;
}

}
QJsonObject WorkerTaskServices::prepare(const ProjectModel& model, const WorkerTaskRequest& task)
{
    return deriveTaskContract(model, task, nullptr);
}

QJsonObject WorkerTaskServices::postflight(const ProjectModel& model, const QJsonObject& contract, const QJsonArray& evidence)
{
    if (contract.value("contractKind") == "canonical-scope-registration-operation/v1")
        return postflightScopeRegistrationOperation(model, contract);
    if (contract.value("contractKind") == "canonical-p6-activation-operation/v1")
        return postflightActivationOperation(model, contract);
    if (contract.value("contractKind") == "canonical-certification-operation/v1")
        return postflightCertificationOperation(model, contract);
    if (contract.value("contractKind") == "canonical-p6-requirements-operation/v1")
        return postflightRequirementsOperation(model, contract);
    WorkerResourceObservation observation;
    QJsonArray errors;
    auto unsignedContract = contract;
    unsignedContract.remove("contractId");
    if (contract.value("schemaVersion").toInt() != 1)
        issue(errors, "TASK_CONTRACT_INVALID", "Unsupported or missing TaskContract schema.");
    else if (fingerprint(unsignedContract) != contract.value("contractId").toString())
        issue(errors, "TASK_CONTRACT_TAMPERED", "Contract content/hash changed after preflight.");
    const QString preflight = contract.value("preflight").toObject().value("status").toString();
    if (preflight != "READY" && preflight != "READY_WITH_WARNINGS") issue(errors, "TASK_PREFLIGHT_BLOCKED", "Invalid preflight cannot authorize work.");
    const auto taskRequest = WorkerTaskRequest::fromJson(contract.value("request").toObject());
    if (!taskRequest.acceptanceProfile.isEmpty()) {
        const auto profile = acceptanceProfile(taskRequest.acceptanceProfile);
        const auto allowed = strings(profile.value("files"));
        bool scopeValid = !profile.isEmpty();
        for (const auto& file : taskRequest.files) scopeValid &= allowed.contains(file);
        if (!scopeValid || semantic(array(taskRequest.definitionOfDone)) != semantic(profile.value("requirements")))
            issue(errors, "TASK_PREFLIGHT_BLOCKED", "Acceptance profile requires its exact canonical requirements and bounded implementation files.");
    }
    const bool reauthorized = contract.contains("reauthorization");
    QJsonObject reauthorization;
    QJsonObject authoritative;
    if (reauthorized) {
        reauthorization = boundReauthorizationGrant(model, contract, errors);
        const auto originalBinding = contract.value("binding").toObject();
        const auto currentBinding = binding(model);
        bool bindingValid = originalBinding.value("projectRoot") == currentBinding.value("projectRoot")
            && originalBinding.value("projectId") == currentBinding.value("projectId")
            && originalBinding.value("workerIdentity") == currentBinding.value("workerIdentity")
            && originalBinding.value("modelFingerprint") == currentBinding.value("modelFingerprint");
        auto oldInputs = originalBinding.value("inputs").toObject();
        auto newInputs = currentBinding.value("inputs").toObject();
        QStringList refreshable;
        for (const auto& path : strings(reauthorization.value("canonicalInputs"))) {
            QString relative = path;
            if (relative.startsWith(worker(model) + '/')) relative.remove(0, worker(model).size() + 1);
            refreshable.append(relative);
        }
        for (const auto& key : refreshable) { oldInputs.remove(key); newInputs.remove(key); }
        bindingValid = bindingValid && semantic(oldInputs) == semantic(newInputs);
        if (!bindingValid) issue(errors, "STALE_TASK_CONTRACT", "A project identity or non-authorized canonical input changed.");
        authoritative = contract;
    } else {
        if (contract.value("binding").toObject() != binding(model)) issue(errors, "STALE_TASK_CONTRACT", "Canonical inputs or project identity changed; derive and review a new contract.");
        // A recomputed checksum is not authority. Re-derive ordinary permissions from canonical inputs.
        authoritative = deriveTaskContract(model, WorkerTaskRequest::fromJson(contract.value("request").toObject()), &contract);
        // Completed relocations no longer satisfy preparation-time existence checks;
        // compare their authority here and validate their actual move below.
        for (const QString& field : {QStringLiteral("permittedFiles"), QStringLiteral("requiredEvidence"), QStringLiteral("risk"), QStringLiteral("impact"), QStringLiteral("evidenceDependencies"), QStringLiteral("taskDependencies"), QStringLiteral("protectedGrant"), QStringLiteral("prerequisiteReadSet")})
            if (semantic(contract.value(field)) != semantic(authoritative.value(field))) issue(errors, "TASK_CONTRACT_INVALID", "Contract authorization differs from canonical derivation.", field);
    }
    validateCanonical(model, errors);
    const auto current = snapshot(model, errors);
    const auto before = contract.value("baseline").toObject();
    QString grantError;
    const auto protectedGrant = checkedGrant(model, WorkerTaskRequest::fromJson(contract.value("request").toObject()), &grantError, &contract);
    if (!grantError.isEmpty()) issue(errors, "PROTECTED_GRANT_INVALID", grantError, "CMakeLists.txt");
    QSet<QString> relocatedSources, relocationEndpoints;
    QJsonArray observedRelocations;
    const auto requested = contract.value("request").toObject().value("relocations").toArray();
    const auto authorized = contract.value("authorizedRelocations").toArray();
    if (requested.size() != authorized.size()) issue(errors, "RELOCATION_UNPROVEN", "Incomplete relocation authorization.");
    for (const auto& value : authorized) {
        const auto relocation = value.toObject();
        const QString oldPath = relocation.value("oldPath").toString(), newPath = relocation.value("newPath").toString();
        const QString hash = relocation.value("contentFingerprint").toString();
        bool declared = false;
        for (const auto& pair : requested)
            declared |= pair.toObject().value("oldPath") == oldPath && pair.toObject().value("newPath") == newPath;
        const auto allowed = strings(authoritative.value("permittedFiles"));
        const QString oldKey = oldPath.toCaseFolded(), newKey = newPath.toCaseFolded();
        const bool valid = declared && allowed.contains(oldPath) && allowed.contains(newPath)
            && relocationPath(model, newPath)
            && committedContent(model, relocation.value("sourceRevision").toString(), oldPath, hash)
            && before.value(oldPath) == hash && before.value(newPath).toString("MISSING") == "MISSING"
            && oldKey != newKey && !relocationEndpoints.contains(oldKey) && !relocationEndpoints.contains(newKey);
        relocationEndpoints.insert(oldKey); relocationEndpoints.insert(newKey);
        if (valid && current.value(oldPath) == hash && current.value(newPath).toString("MISSING") == "MISSING") continue;
        if (!valid || QFileInfo::exists(pathFor(model, oldPath)) || QFileInfo(pathFor(model, oldPath)).isSymLink()
            || !QFileInfo(pathFor(model, newPath)).isFile() || current.value(newPath) != hash) {
            issue(errors, "RELOCATION_UNPROVEN", "Relocation content, repository binding, ownership or unique destination continuity failed.", oldPath);
            continue;
        }
        relocatedSources.insert(oldPath);
        auto observed = relocation; observed.insert("operation", "RELOCATE");
        observed.insert("contractId", contract.value("contractId")); observed.insert("authorization", contract.value("binding"));
        observedRelocations.append(observed);
    }
    // Evidence is about implementation/source state. Canonical validation and
    // recorder outputs are checked below but cannot make evidence self-stale.
    const auto dependencyHash = [&](const QJsonValue& paths) {
        QJsonObject dependencies;
        for (const auto& path : strings(paths)) dependencies.insert(path, current.value(path).toString("MISSING"));
        return fingerprint(dependencies);
    };
    QJsonObject implementation;
    for (const auto& path : strings(contract.value("taskDependencies"))) implementation.insert(path, current.value(path).toString("MISSING"));
    const QString resultFingerprint = fingerprint(implementation);
    QJsonObject certificate;
    const QString certificateSuffix = AramfPaths::detail::workerSuffixOverride();
    AramfPaths::setRuntimeWorkerNameSuffix(model.workerNameSuffix());
    CertificationService().latestForSubject(root(model), contract.value("contractId").toString(), &certificate);
    AramfPaths::setRuntimeWorkerNameSuffix(certificateSuffix);
    const bool physicalRequired = strings(contract.value("requiredEvidence")).contains("physical-certification");
    bool certificateEvidenceValid = !certificate.value("evidenceReferences").toArray().isEmpty();
    bool physicalEvidenceValid = false;
    for (const auto& value : certificate.value("evidenceReferences").toArray()) {
        const auto reference = value.toObject();
        const QString path = reference.value("reference").toString();
        const bool valid = safePath(root(model), path) && fileHash(pathFor(model, path)) == reference.value("fingerprint").toString();
        certificateEvidenceValid = certificateEvidenceValid && valid && reference.value("verified").toBool();
        const QString type = reference.value("type").toString().toLower();
        physicalEvidenceValid = physicalEvidenceValid || (valid && (type == "physical" || type == "on-target"));
    }
    const bool certified = certificateEvidenceValid && certificate.value("status").toString() == "PASS" && certificate.value("evidenceComplete").toBool()
        && certificate.value("context").toObject().value("resultFingerprint").toString() == resultFingerprint
        && (!physicalRequired || (physicalEvidenceValid && certificate.value("verificationLevel").toString() == "HARDWARE_CERTIFIED"));
    if (physicalRequired && !certified)
        issue(errors, certificate.isEmpty() ? "CERTIFICATION_MISSING" : "PHYSICAL_CERTIFICATION_STALE", "Current task/result-bound physical certification has not been verified.", {}, "WARNING");
    QStringList paths = before.keys() + current.keys(), modified;
    paths.removeDuplicates();
    const QString previousSuffix = AramfPaths::detail::workerSuffixOverride();
    AramfPaths::setRuntimeWorkerNameSuffix(model.workerNameSuffix());
    bool memoryChecked = false, memoryValid = false;
    const auto checkMemory = [&]() {
        if (!memoryChecked) { memoryValid = ProjectMemory().validate(root(model), nullptr, false).value("status").toString() == "PASS"; memoryChecked = true; }
        return memoryValid;
    };
    const auto appendIntact = [&](const QString& path) {
        QFile file(pathFor(model, path));
        if (!file.open(QIODevice::ReadOnly)) return false;
        const qint64 length = static_cast<qint64>(contract.value("historyLengths").toObject().value(path).toDouble());
        return length <= file.size() && digest(file.read(length)) == before.value(path).toString();
    };
    const auto expectedDerived = GenerationServices::derivedTaskArtifacts(model, model.generationOptions());
    const QJsonObject postflightPolicy = reauthorized ? contract.value("mutationPolicy").toObject() : mutationPolicy(model);
    const auto generationStateError = [&]() {
        auto actual = readObject(pathFor(model, workerPath(model, "verification/generation-state.json")));
        if (actual.isEmpty()) return QStringLiteral("missing or malformed canonical generation state");
        actual.remove("_file");
        const auto options = model.generationOptions();
        auto canonicalState = ProjectPersistence().toJson(model);
        canonicalState.remove("projectPath"); canonicalState.remove("projectFilePath"); canonicalState.remove("workflowProgress");
        const QSet<QString> allowedKeys{"fingerprint", "canonicalState", "projectRoot", "generatedAt", "agentRules",
            "routing", "platforms", "resources", "memory", "provenance", "academicDocuments",
            "academicDocumentHashes", "configurationArtifactHashes"};
        QSet<QString> actualKeys;
        for (auto it = actual.begin(); it != actual.end(); ++it) actualKeys.insert(it.key());
        const auto generatedAt = QDateTime::fromString(actual.value("generatedAt").toString(), Qt::ISODate);
        if (!actualKeys.subtract(allowedKeys).isEmpty()) return QStringLiteral("unknown generation-state fields");
        if (actual.value("fingerprint") != projectConfigurationFingerprint(model, options)) return QStringLiteral("generation fingerprint mismatch");
        if (semantic(actual.value("canonicalState")) != semantic(canonicalState)) return QStringLiteral("canonical model snapshot mismatch");
        if (actual.value("projectRoot") != root(model) || !generatedAt.isValid()) return QStringLiteral("root or generation timestamp invalid");
        const auto serializedOptions = canonicalState.value("generationOptions").toObject();
        const bool academicOptionPresent = serializedOptions.contains("academicDocuments");
        const bool academicStateValid = academicOptionPresent
            ? actual.value("academicDocuments") == serializedOptions.value("academicDocuments") && actual.value("academicDocumentHashes").isObject()
            : !actual.contains("academicDocuments") && !actual.contains("academicDocumentHashes");
        if (actual.value("agentRules").toBool() != options.generateAgentRules || actual.value("routing").toBool() != options.generateRouting
            || actual.value("platforms").toBool() != options.generatePlatforms || actual.value("resources").toBool() != options.generateResources
            || actual.value("memory").toBool() != options.generateMemory || actual.value("provenance").toBool() != options.generateProvenance
            || !academicStateValid) return QStringLiteral("generation options or document hashes mismatch");
        const auto hashes = actual.value("configurationArtifactHashes").toObject();
        for (auto it = hashes.begin(); it != hashes.end(); ++it)
            if (!safePath(root(model), it.key()) || fileHash(pathFor(model, it.key())) != it.value().toString())
                return QStringLiteral("configuration artifact hash mismatch: ") + it.key();
        return QString{};
    };
    bool verificationChecked = false;
    VerificationResult currentVerification;
    QString certificationError;
    const auto expectedCertification = CertificationService().derivedCurrentState(root(model), &certificationError);
    auto actualCertification = CertificationService().currentState(root(model)); actualCertification.remove("_file");
    const bool certificationStateValid = certificationError.isEmpty() && !expectedCertification.isEmpty() && actualCertification == expectedCertification;
    const bool orchestrationStatePresent = !model.orchestrationState().isEmpty();
    QJsonArray serviceChanges;
    for (const auto& path : paths) {
        if (path.startsWith("external-manifest:")) continue;
        if (before.value(path) == current.value(path)) continue;
        if (path.startsWith("external:")) {
            const QString id = path.mid(9);
            const auto oldManifest = before.value("external-manifest:" + id).toObject();
            const auto newManifest = current.value("external-manifest:" + id).toObject();
            if (!oldManifest.isEmpty() && !newManifest.isEmpty()) {
                bool covered = oldManifest.value("valid").toBool() && newManifest.value("valid").toBool()
                    && oldManifest.value("policy") == newManifest.value("policy");
                QString resourceRoot;
                for (const auto& resource : model.resources()) if (resource.id == id) resourceRoot = pathFor(model, resource.location);
                const auto oldEntries = oldManifest.value("entries").toObject();
                const auto newEntries = newManifest.value("entries").toObject();
                QStringList children = oldEntries.keys() + newEntries.keys(); children.removeDuplicates();
                for (const auto& child : children) {
                    if (oldEntries.value(child) == newEntries.value(child)) continue;
                    const QString relative = QDir(root(model)).relativeFilePath(QDir(resourceRoot).filePath(child));
                    const auto oldEntry = oldEntries.value(child).toObject(), newEntry = newEntries.value(child).toObject();
                    if (newEntry.value("type") == "folder" && oldEntry.isEmpty()) {
                        bool authorizedChild = false;
                        for (const auto& permitted : strings(contract.value("permittedFiles")))
                            authorizedChild |= permitted.startsWith(relative + '/');
                        covered &= authorizedChild;
                    } else {
                        covered &= safePath(root(model), relative) && current.contains(relative)
                            && before.value(relative).toString("MISSING") == oldEntry.value("sha256").toString("MISSING")
                            && current.value(relative).toString("MISSING") == newEntry.value("sha256").toString("MISSING");
                    }
                }
                if (covered) continue; // Each changed child is still checked by the ordinary permission loop.
                issue(errors, "RESOURCE_CHILD_UNAUTHORIZED", "Folder changes lack complete child-level baseline coverage", path);
            }
            if (!protectedGrant.isEmpty()) {
                bool cmake = false;
                for (const auto& resource : model.resources())
                    cmake |= resource.id == id && pathFor(model, resource.location) == pathFor(model, "CMakeLists.txt");
                if (cmake && before.value(path) == protectedGrant.value("baseFingerprint")
                    && current.value(path) == protectedGrant.value("resultFingerprint")) continue;
            }
        }
        if (path.startsWith(workerPath(model, "context/handoffs/")) && path.endsWith(".json")) continue;
        modified.append(path);
        const auto ownership = fileRole(model, path, postflightPolicy);
        if (reauthorized && reauthorization.value("schemaVersion").toInt() != 2
            && !strings(reauthorization.value("scope")).contains(path)
            && ownership.value("writer").toString() != "ProjectMemory recorder")
            issue(errors, "REAUTHORIZATION_SCOPE_INVALID", "Observed modification is outside the exact approved source and canonical-input scope.", path);
        if (path == "CMakeLists.txt" && !protectedGrant.isEmpty()
            && strings(contract.value("permittedFiles")).contains(path)
            && before.value(path) == protectedGrant.value("baseFingerprint")
            && current.value(path) == protectedGrant.value("resultFingerprint")) continue;
        bool delegated = false;
        if (ownership.value("writer").toString() == "ProjectMemory recorder") {
            delegated = checkMemory() && appendIntact(workerPath(model, "memory/event-log.jsonl"));
            if (path == workerPath(model, "memory/event-log.jsonl")) delegated = delegated && appendIntact(path);
        }
        const bool declaredGeneration = strings(contract.value("impact").toObject().value("generatedArtifacts")).contains(path);
        const bool p1Generation = expectedDerived.contains(path) && ownership.value("writer").toString() == "ContextCoordinationService";
        const bool requestedGeneration = declaredGeneration || p1Generation;
        const bool reauthorizedContextOutput = reauthorized && strings(reauthorization.value("canonicalInputs")).contains(path)
            && ownership.value("writer").toString() == "ContextCoordinationService";
        const bool reauthorizedGenerationState = reauthorized && path == workerPath(model, "verification/generation-state.json")
            && strings(reauthorization.value("canonicalInputs")).contains(path)
            && ownership.value("writer").toString() == "GenerationServices";
        const bool canonicalOrchestrationOutput = orchestrationStatePresent && path == workerPath(model, "project.json");
        if (expectedDerived.contains(path) && (ownership.value("writer").toString() == "GenerationServices" || p1Generation)
            && (requestedGeneration || canonicalOrchestrationOutput)
            && strings(ownership.value("actions")).contains("REGENERATE")) {
            auto actual = readObject(pathFor(model, path)); actual.remove("_file");
            delegated = !actual.isEmpty() && semantic(actual) == semantic(expectedDerived.value(path));
        }
        if (reauthorizedContextOutput) {
            const auto contextCheck = ContextCoordinationService::validate(model);
            delegated = contextCheck.value("valid").toBool();
        }
        const QString generationStateValidationError = reauthorizedGenerationState ? generationStateError() : QString{};
        if (reauthorizedGenerationState) delegated = generationStateValidationError.isEmpty();
        if (ownership.value("writer").toString() == "VerificationServices" && (requestedGeneration || orchestrationStatePresent)) {
            if (!verificationChecked) { currentVerification = VerificationServices().verify(model, model.generationOptions(), false); verificationChecked = true; }
            auto actual = readObject(pathFor(model, path)); actual.remove("_file"); actual.remove("checkedAt");
            const auto expected = path.endsWith("latest-validation.json") ? currentVerification.summary : currentVerification.evidence;
            delegated = currentVerification.overallStatus == VerificationStatus::Pass && semantic(actual) == semantic(expected);
        }
        // A legitimate certificate remains historical evidence when its source
        // becomes stale. Freshness controls CERTIFIED, not permission to retain it.
        if (!certificate.isEmpty() && certificationStateValid && ownership.value("writer").toString() == "CertificationService")
            delegated = checkMemory() && appendIntact(workerPath(model, "memory/event-log.jsonl"))
                && appendIntact(workerPath(model, "certification/certificates.jsonl"));
        if (delegated) { serviceChanges.append(QJsonObject{{"path", path}, {"service", ownership.value("writer")}, {"validation", "canonical-producer-and-boundary"}}); continue; }
        if (ownership.value("owner").toString() == "generated" || ownership.value("owner").toString() == "validation")
            issue(errors, requestedGeneration ? "GENERATED_CONTENT_MISMATCH" : "UNEXPECTED_SERVICE_REGENERATION",
                generationStateValidationError.isEmpty() ? "Derived change is undeclared or does not match its canonical producer."
                    : QStringLiteral("Derived change failed its canonical producer check: ") + generationStateValidationError, path);
        if (!strings(contract.value("permittedFiles")).contains(path)) issue(errors, "TASK_SCOPE_VIOLATION", "Observed modification outside the exact permitted file set.", path);
        if ((!current.contains(path) || current.value(path).toString() == "MISSING") && !relocatedSources.contains(path)) issue(errors, "FORBIDDEN_FILE_MODIFICATION", "Deletion was not authorized by this task.", path);
        if (path.startsWith("external:") || ownership.value("owner").toString() != "user" || strings(ownership.value("actions")).contains("NEVER_TOUCH"))
            issue(errors, "FORBIDDEN_FILE_MODIFICATION", "A protected owner/source was modified.", path);
    }
    AramfPaths::setRuntimeWorkerNameSuffix(previousSuffix);
    parallelState(model, current, errors);
    QStringList missing;
    QJsonArray accepted;
    QJsonObject evidenceFingerprints;
    bool evidenceFailure = false;
    for (const auto& required : strings(contract.value("requiredEvidence"))) {
        bool pass = false;
        const QString expectedFingerprint = dependencyHash(contract.value("evidenceDependencies").toObject().value(required));
        evidenceFingerprints.insert(required, expectedFingerprint);
        // diff-boundary is evaluated here from the actual filesystem.
        if (required == "diff-boundary") pass = state(errors).startsWith("READY");
        else if (required == "physical-certification") pass = certified;
        else for (const auto& item : evidence) {
            const auto entry = item.toObject();
            if (entry.value("check").toString() != required) continue;
            const QString artifact = entry.value("artifact").toString();
            const bool artifactValid = safePath(root(model), artifact) && fileHash(pathFor(model, artifact)) == entry.value("artifactFingerprint").toString();
            const auto record = artifactValid ? readObject(pathFor(model, artifact)) : QJsonObject{};
            const bool fresh = record.contains("dependencyFingerprint") ? record.value("dependencyFingerprint").toString() == expectedFingerprint : record.value("resultFingerprint").toString() == resultFingerprint;
            const bool bound = record.value("contractId") == contract.value("contractId") && fresh
                && record.value("check").toString() == required;
            if (artifactValid && !fresh) issue(errors, "STALE_EVIDENCE", "Evidence dependencies changed after the check ran.", required, "WARNING");
            if (!artifactValid) issue(errors, "EVIDENCE_INTEGRITY_INVALID", "Evidence file is missing or its checksum changed.", artifact, "WARNING");
            if (bound && record.value("status").toString() == "FAIL") { issue(errors, "VALIDATION_FAILED", "Required validation failed.", required, "WARNING"); evidenceFailure = true; }
            if (artifactValid && bound && record.value("status").toString() == "PASS" && required != "physical-certification") { pass = true; accepted.append(entry); }
        }
        // Physical certification cannot be promoted by arbitrary JSON evidence.
        // The existing CertificationService remains the only certificate owner.
        if (!pass) missing.append(required);
    }
    const bool safe = state(errors).startsWith("READY");
    const bool softwareComplete = !evidenceFailure && (missing.isEmpty() || missing == QStringList{"physical-certification"});
    QString completion = !safe ? "BLOCKED" : softwareComplete ? (certified ? "CERTIFIED" : "VERIFIED") : modified.isEmpty() ? "IN_PROGRESS" : "IMPLEMENTED_UNVERIFIED";
    if (!missing.isEmpty()) issue(errors, "REQUIRED_EVIDENCE_MISSING", "Required evidence has not been verified: " + missing.join(", "), {}, "WARNING");
    QJsonObject report{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractId", contract.value("contractId")}, {"relocations", observedRelocations},
        {"resultFingerprint", resultFingerprint}, {"evidenceFingerprints", evidenceFingerprints}, {"modifiedFiles", array(modified)}, {"serviceChanges", serviceChanges}, {"status", safe && !evidenceFailure ? "PASS" : "FAIL"},
        {"completionState", completion}, {"missingEvidence", array(missing)}, {"acceptedEvidence", accepted}, {"errors", errors}, {"certificateId", certified ? certificate.value("certificateId") : QJsonValue{}},
        {"certification", "CERTIFIED is issued only by the canonical CertificationService; physical evidence is never inferred."}};
    if (reauthorization.value("schemaVersion").toInt() == 2 && safe && softwareComplete) {
        QJsonArray sources, origins;
        for (const auto& value : reauthorization.value("sourceAuthorities").toArray()) sources.append(value.toObject().value("contractId"));
        for (const auto& value : reauthorization.value("origins").toArray()) origins.append(value.toObject().value("contract").toObject().value("contractId"));
        report.insert("successorCompletion", QJsonObject{{"authorizationId", reauthorization.value("authorizationId")},
            {"sourceAuthorityContractIds", sources}, {"retainedFailedOriginContractIds", origins},
            {"claim", "This successor has current observations; original contract statuses baselines and failed evidence remain historical and unchanged"}});
    }
    return report;
}

namespace {
#ifdef Q_OS_WIN
struct StartupWindowObservation { DWORD processId = 0; QList<HWND> windows; };
BOOL CALLBACK collectStartupWindows(HWND window, LPARAM parameter)
{
    auto* observation = reinterpret_cast<StartupWindowObservation*>(parameter);
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == observation->processId && IsWindowVisible(window) && GetWindow(window, GW_OWNER) == nullptr)
        observation->windows.append(window);
    return TRUE;
}
#endif
QString evidenceDirectory(const QJsonObject& contract)
{
    const QString id = contract.value("contractId").toString();
    return QRegularExpression("^[a-f0-9]{64}$").match(id).hasMatch()
        ? "build/aramf-task-evidence/" + id : QString{};
}
bool evidencePathAllowed(const ProjectModel& model, const QString& relative)
{
    if (!safePath(root(model), relative)) return false;
    QFileInfo part(pathFor(model, relative));
    while (part.absoluteFilePath() != root(model) && part.absoluteFilePath() != part.absolutePath()) {
        if (part.isSymLink() || part.isJunction()) return false;
        part = QFileInfo(part.absolutePath());
    }
    QByteArray output;
    if (!git(root(model), {"check-ignore", "--", relative}, &output) || output.trimmed().isEmpty()) return false;
    if (!git(root(model), {"ls-files", "--", relative}, &output) || !output.trimmed().isEmpty()) return false;
    const QString absolute = QDir::cleanPath(pathFor(model, relative));
    for (const auto& resource : model.resources()) {
        if (!resource.enabled || resource.type == "url" || resource.location.contains("://")) continue;
        const QString location = QDir::cleanPath(QDir(root(model)).absoluteFilePath(resource.location));
        if (absolute.compare(location, Qt::CaseInsensitive) == 0
            || (resource.type == "folder" && absolute.startsWith(location + '/', Qt::CaseInsensitive))) return false;
    }
    return true;
}
bool writeNew(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly) || file.write(bytes) != bytes.size() || !file.flush()) return false;
    file.close();
    return fileHash(path) == digest(bytes);
}
}

QJsonObject WorkerTaskServices::acceptanceProfile(const QString& id)
{
    // Brief: Bind the Android/member contract to measured core assertions plus full UI/build regression.
    if (id == "task-android-profiles-members/v1") {
        static const auto profile = QJsonDocument::fromJson(QByteArray(R"ARAMF({
  "id": "task-android-profiles-members/v1",
  "version": 1,
  "requirements": [
    "Backward-compatible loading and affected migration, navigation, user overrides, capability states, profile separation, toolchain values, UI behavior and generated output have meaningful regressions.",
    "Dedicated stable project.members page uses the next valid project position and stable ID without renumbering; members can be added, edited and removed with optional contacts, roles, responsibilities and affiliation.",
    "Profile selection consistently configures applicable capabilities, environment, AI permissions, resources, rules and validation while preserving unrelated choices and explicit user overrides; all defaults are reviewable and editable.",
    "Required current builds, focused tests, routing-policy checks, producer readback and exact-scope postflight pass; historical failures are preserved, physical device testing is not claimed, and P6 remains started, uncertified and incomplete.",
    "Save and reopen, review, template save and restore, generation and verification retain profile data and members; reusable templates strip personal identities and contact details while retaining role slots.",
    "StorePath defaults and provenance match actual repository b9489f02ec2c4cc6e5bdbc28adf593e8dc1cb4b6, distinguishing Gradle runtime JDK from Java target, user-reported environment from verification, actual dependencies from planned capabilities and unsupported Wi-Fi RTT.",
    "StorePath, Android Application Lab 2 and optional Keyestudio profiles remain distinct; Lab 2 includes Retrofit/GitHub REST writes, operator decisions, defensive errors and incident closure only when selected, without storing secrets or performing remote writes."
  ],
  "files": [
    "src/core/ComponentVersion.cpp",
    "src/core/EnvironmentCatalog.cpp",
    "src/core/EnvironmentCatalog.h",
    "src/core/ProjectModel.cpp",
    "src/core/ProjectModel.h",
    "src/core/ProjectPersistence.cpp",
    "src/core/Services.cpp",
    "src/core/TemplateManager.cpp",
    "src/core/TemplateValidation.cpp",
    "src/core/WorkerTaskServices.cpp",
    "src/core/WorkflowPageMetadata.cpp",
    "src/ui/mainwindow/MainWindow.cpp",
    "src/ui/mainwindow/MainWindow.h",
    "src/ui/workflow/WorkflowPageId.h",
    "src/ui/workflow/WorkflowWidget.cpp",
    "src/ui/workflows/output/review/ReviewPage.cpp",
    "src/ui/workflows/project/frameworks/mobile/ProjectFrameworkMobilePage.h",
    "src/ui/workflows/project/members/ProjectMembersPage.h",
    "src/ui/workflows/project/overview/ProjectOverviewPage.cpp",
    "tests/TemplateConfigurationTests.cpp",
    "tests/WorkerTaskTests.cpp",
    "tests/WorkflowNavigationTests.cpp"
  ],
  "checks": [
    "aramf_core_tests::--worker-tasks",
    "production-build",
    "ctest"
  ],
  "criteria": [
    {
      "id": "ANDROID-1",
      "requirement": "Backward-compatible loading and affected migration, navigation, user overrides, capability states, profile separation, toolchain values, UI behavior and generated output have meaningful regressions.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android profile persistence round trip",
        "android profile legacy configuration clears optional fields",
        "android profile registered acceptance is exact"
      ]
    },
    {
      "id": "ANDROID-2",
      "requirement": "Dedicated stable project.members page uses the next valid project position and stable ID without renumbering; members can be added, edited and removed with optional contacts, roles, responsibilities and affiliation.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android members model supports add edit remove",
        "android members stable page belongs to project setup"
      ]
    },
    {
      "id": "ANDROID-3",
      "requirement": "Profile selection consistently configures applicable capabilities, environment, AI permissions, resources, rules and validation while preserving unrelated choices and explicit user overrides; all defaults are reviewable and editable.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android profiles configure capabilities and environment",
        "android profiles preserve unrelated user choices",
        "android profile generic edits synchronize",
        "android profiles configure safe local AI defaults"
      ]
    },
    {
      "id": "ANDROID-4",
      "requirement": "Required current builds, focused tests, routing-policy checks, producer readback and exact-scope postflight pass; historical failures are preserved, physical device testing is not claimed, and P6 remains started, uncertified and incomplete.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android profile operations preserve lifecycle",
        "android profile generation and verification pass",
        "android profile acceptance requires current full validation"
      ]
    },
    {
      "id": "ANDROID-5",
      "requirement": "Save and reopen, review, template save and restore, generation and verification retain profile data and members; reusable templates strip personal identities and contact details while retaining role slots.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android profile persistence round trip",
        "android templates preserve roles without identities",
        "android generated information retains members and profile"
      ]
    },
    {
      "id": "ANDROID-6",
      "requirement": "StorePath defaults and provenance match actual repository b9489f02ec2c4cc6e5bdbc28adf593e8dc1cb4b6, distinguishing Gradle runtime JDK from Java target, user-reported environment from verification, actual dependencies from planned capabilities and unsupported Wi-Fi RTT.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android StorePath observed toolchain defaults",
        "android StorePath runtime and targets remain separate",
        "android StorePath capabilities are not external tools",
        "android StorePath excludes future dependencies and RTT"
      ]
    },
    {
      "id": "ANDROID-7",
      "requirement": "StorePath, Android Application Lab 2 and optional Keyestudio profiles remain distinct; Lab 2 includes Retrofit/GitHub REST writes, operator decisions, defensive errors and incident closure only when selected, without storing secrets or performing remote writes.",
      "check": "aramf_core_tests::--worker-tasks",
      "assertions": [
        "android Lab 2 requirements remain distinct",
        "android Lab 2 incident closure and errors retain provenance",
        "android hardware stays optional and separate",
        "android profile switching removes owned course requirements",
        "android profile rejects unknown credentials and malformed input"
      ]
    }
  ]
})ARAMF")).object();
        return profile;
    }
    if (id == "task-scope-registration-operations/v1") {
        const QStringList requirements{
            "Scope registration preparation is read-only and binds the exact original blocked task, project identity, current configuration, source observations, minimal additive file mapping and canonical producer outputs.",
            "Only exact safe source files already named in the linked task may be added to an existing active scope; protected files, CMake, saved configurations, Worker outputs, traversal, wildcard paths, conflicting ownership and arbitrary metadata changes are rejected.",
            "Registration apply requires an unchanged READY operation within its bounded validity period and current measured verification of the registration service; forged contracts, altered fingerprints, stale inputs, wrong project, expiry and replay are rejected before writes.",
            "ConfigurationUpdateService performs persistence and derived synchronization; canonical context and verification services refresh only declared outputs and reload proves exact additions while all unrelated configuration and P6 lifecycle remain unchanged.",
            "Immutable operation proof, receipt and audit provenance link the original task and registration contract without rewriting historical contracts, baselines, reports, certificates or evidence; registration itself does not authorize source edits.",
            "A separately prepared ordinary TaskContract sees the new mapping and may become READY only after normal current governance checks; registration never bypasses ordinary postflight or publication gates.",
            "Independent regression cases prove each rejection condition separately and successful preparation, application, readback, postflight and successor preparation; actual measured reports support fixed semantic acceptance.",
            "Existing reauthorization, protected grant and lifecycle behavior remain unchanged and all required current build, targeted tests, full CTest and policy checks pass."};
        const QList<QStringList> assertions{
            {"scope registration preparation is read-only", "scope registration binds original task and exact outputs",
                "scope registration rejects legacy schema without writes", "scope registration rejects missing schema without writes",
                "scope registration rejects string schema without writes", "scope registration rejects fractional schema without writes",
                "scope registration rejects future schema without writes", "scope registration rejects malformed saved JSON without writes",
                "scope registration rejects saved model mismatch without writes", "scope registration CLI rejects invalid saved input without writes",
                "scope registration postflight rejects invalid saved input without writes", "scope registration does not misreport missing proof as expiry"},
            {"scope registration rejects extra intent fields", "scope registration rejects undeclared source", "scope registration rejects CMake", "scope registration rejects saved configuration", "scope registration rejects Worker output", "scope registration rejects traversal", "scope registration rejects wildcard", "scope registration rejects protected resource", "scope registration rejects inactive scope", "scope registration rejects conflicting mapping", "scope registration rejects incomplete mapping"},
            {"scope registration rejects forged operation", "scope registration rejects recomputed expanded operation", "scope registration rejects changed source", "scope registration rejects changed configuration", "scope registration rejects wrong project", "scope registration rejects expired operation", "scope registration rejects replay", "scope registration rejects unmeasured verification"},
            {"scope registration canonical apply and readback pass", "scope registration preserves unrelated state and lifecycle", "scope registration rejects altered context", "scope registration rejects altered cold-start"},
            {"scope registration retains immutable original and agent audit", "scope registration rejects altered receipt", "scope registration rejects altered proof", "scope registration rejects altered retained evidence"},
            {"scope registration successor prepares normally", "scope registration never permits source mutation"},
            {"scope registration measured acceptance completes", "scope registration rejects lock contention"},
            {"scope registration exact legacy requirements retain contract authority"}};
        QJsonArray criteria;
        for (int i = 0; i < requirements.size(); ++i)
            criteria.append(QJsonObject{{"id", QString("SCOPE-%1").arg(i + 1)}, {"requirement", requirements.at(i)},
                {"check", "aramf_core_tests::--worker-tasks"}, {"assertions", array(assertions.at(i))}});
        return {{"id", id}, {"version", 1}, {"requirements", QJsonArray::fromStringList(requirements)}, {"criteria", criteria},
            {"checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}},
            {"files", QJsonArray{"src/core/WorkerTaskServices.h", "src/core/WorkerTaskServices.cpp", "tests/WorkerTaskTests.cpp"}}};
    }
    if (id == "task-p6-activation-campaign-isolation/v2") {
        auto profile = acceptanceProfile("task-p6-activation-campaign-isolation/v1");
        profile.insert("id", id); profile.insert("version", 2); profile.insert("files", array(repairSourceFiles(id)));
        auto criteria = profile.value("criteria").toArray(), requirements = profile.value("requirements").toArray();
        const auto add = [&](const QString& key, const QString& requirement, const QString& check, const QStringList& assertions) {
            requirements.append(requirement); criteria.append(QJsonObject{{"id", key}, {"requirement", requirement}, {"check", check}, {"assertions", array(assertions)}});
        };
        add("P6-REPAIR-07", "Complete measured CTest includes the real Foundation certification regression with valid historical F2 and P1 source witnesses and current dependency restoration",
            "ctest", {"complete CTest includes successful Foundation certification"});
        add("P6-REPAIR-08", "The distinct v2 successor preserves v1 and binds exactly nine authorized sources with authentic failed successor lineage and independent cross-profile rejection",
            "aramf_core_tests::--worker-tasks", {"v2 repair profile preserves v1 and binds exact nine source files", "v2 repair successor enforces selected profile scope and real failed successor lineage"});
        profile.insert("criteria", criteria); profile.insert("requirements", requirements);
        return profile;
    }
    if (id == "task-p6-activation-campaign-isolation/v1") {
        auto profile = acceptanceProfile("task-p6-activation-governance/v1");
        profile.insert("id", id); profile.insert("files", array(repairSourceFiles()));
        auto criteria = profile.value("criteria").toArray(), requirements = profile.value("requirements").toArray();
        for (const auto& item : acceptanceProfile("task-p6-prerequisite-closure/v1").value("criteria").toArray())
            if (item.toObject().value("id") == "P1-SOURCE-01") { criteria.append(item); requirements.append(item.toObject().value("requirement")); }
        const auto add = [&](const QString& key, const QString& requirement, const QString& check, const QStringList& assertions) {
            requirements.append(requirement); criteria.append(QJsonObject{{"id", key}, {"requirement", requirement}, {"check", check}, {"assertions", array(assertions)}});
        };
        add("P6-REPAIR-01", "F2 independently recognizes explicit temporary repair intent and preserves legacy authority semantics and Foundation independence",
            "aramf_core_tests::--worker-tasks", {"repair authority is owned by independent F2 with unchanged legacy override", "repair authority rejects missing negated foreign and unsafe intent"});
        add("P6-REPAIR-02", "Two real UPDATE campaign runs preserve all original assertions with separate retained create-only results and legacy identities without repository writes",
            "aramf_update_campaign", {"UPDATE runner retains all 310 original assertions", "UPDATE runner isolates result and legacy paths", "UPDATE runner preserves create-only results and reports failed IDs"});
        add("P6-REPAIR-03", "Exact source READY authorities and reproduced failed origins bind a new bounded successor while retained observations grant no writes",
            "aramf_core_tests::--worker-tasks", {"repair successor binds original READY sources and reproduced FAIL origins", "repair successor retains campaign and canonical observations without write expansion", "repair successor rejects forged omitted widened or changed origins"});
        add("P6-REPAIR-04", "Actual directive and agent provenance produce a unique validated temporary grant with exact project scope and lifetime and preserved partial failures",
            "aramf_core_tests::--worker-tasks", {"repair successor records actual agent provenance without new human override", "repair successor rejects expiry duplication revocation and replay", "repair successor retains incomplete authority as inactive"});
        add("P6-REPAIR-05", "Pure successor postflight preserves historical bytes and validates current canonical producers while rejecting retained input and source boundary tampering",
            "aramf_core_tests::--worker-tasks", {"repair successor postflight is pure and permits actual canonical refresh", "repair successor rejects retained bytes history and undeclared changes"});
        add("P6-REPAIR-06", "Explicit combined evidence retains all activation and freshness criteria and cannot relabel legacy contracts or reuse contract-bound records",
            "aramf_core_tests::--worker-tasks", {"combined repair profile retains activation and freshness criteria and exact scope", "combined repair evidence cannot inherit old profile or contract evidence"});
        profile.insert("criteria", criteria); profile.insert("requirements", requirements);
        profile.insert("checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest", "aramf_freshness_tests", "aramf_update_campaign"});
        return profile;
    }
    if (id == "task-p6-activation-governance/v1") {
        const QStringList requirements{"A fixed activation-governance acceptance profile binds the exact three source files and measured named regression assertions while deriving the complete F1-F4 and P1-P5 source manifest union as read-only build CTest and acceptance dependencies without expanding write authority or altering legacy contracts",
            "Typed activate-p6 preparation is read-only and accepts only current verified authoritative integration lineage complete registered P6 requirements correct project Worker identity canonical namespace no active process or structure and exact next P6",
            "Activation plans derive unique fixed operation paths finite canonical effective output inventory and a bounded expiry from service observations while binding original verified source proof current nine prerequisite source contract certificate evidence and acceptance identities plus exact before and projected lifecycle",
            "Caller supplied permissions paths authority fingerprints results lifecycle substitutions unknown fields malformed expired stale foreign or replayed activation plans are rejected independently with original bytes preserved",
            "Locked activation apply rederives the current exact READY plan and all frozen inputs then measures the full Foundation campaign eligibility and explicit F4 P6 eligibility immediately before the only actual start transition",
            "Actual P6 start uses ProjectModel and F4 lifecycle followed by canonical ConfigurationUpdateService preview and apply save reload and verification with processVersion as the only semantic model delta and all unrelated options academic settings project identity and histories preserved",
            "Exact activation output permission comes only from canonical effective producer files saved model and explicit immutable proof receipt seal and audit outputs with no P6 code bank CMake certificate or undeclared task DAG mutation",
            "Activation records the real observed gate results and started lifecycle through canonical producers and preserves all original event decision certificate and completed lifecycle prefixes without issuing certification or completing advancing or implementing P6",
            "Interrupted or failed activation retains honest failed observations and cannot replay the start duplicate lifecycle or report success from a partial saved state; exact already-started state is usable only for verified readback or explicitly bounded failure recovery",
            "Pure activation postflight independently verifies actual current initial P6.1.1.0.0 next P7.1.0.0.0 unchanged completed history certificate bytes and source bindings exact requirements and authoritative acceptance canonical generation context and outside-scope preservation",
            "Activation postflight rejects independently tampered missing or mismatched receipts model lifecycle canonical output baseline acceptance prerequisite or evidence inputs and repeated successful inspection performs no writes or later lifecycle transition",
            "Isolated real canonical producer fixtures cover complete requirements and fresh integration through activation and restart readback plus each negative control concurrency expiry replay and partial failure without weakening any existing assertion",
            "Current production build complete CTest required regression campaigns actual named acceptance observations and complete current READY software postflight pass before any live activation operation is prepared"};
        const QList<QStringList> assertions{{"activation profile binds exact three implementation files", "activation profile derives complete prerequisite read union without write expansion", "activation profile preserves legacy contract serialization"},
            {"P6 activation preparation is read-only", "P6 activation requires verified integration and complete registered requirements", "P6 activation rejects wrong identity namespace active lifecycle or next process"},
            {"P6 activation derives unique fixed paths and finite producer-owned outputs", "P6 activation binds original proof all prerequisites acceptance and projected lifecycle", "P6 activation validity is bounded with service-derived timestamps"},
            {"P6 activation rejects forged authority permissions paths results and lifecycle", "P6 activation rejects malformed unknown foreign or stale plans before writes", "P6 activation actual expiry rejects without writes", "P6 activation replay preserves original artifacts"},
            {"P6 activation lock rejects concurrent apply", "P6 activation rederives frozen inputs and records both actual eligibility gates"},
            {"P6 activation persists only the canonical F4 start delta", "P6 activation reloads initial P6.1.1.0.0 and next P7.1.0.0.0", "P6 activation preserves unrelated model and academic settings"},
            {"P6 activation writes only exact canonical and immutable operation outputs", "P6 activation preserves code CMake bank certificate and task DAG bytes"},
            {"P6 activation receipt records actual gates and start without certification", "P6 activation preserves original event decision and completed lifecycle history"},
            {"P6 activation partial saved failure retains honest immutable stage evidence", "P6 activation failed operation cannot report success or start twice"},
            {"P6 activation postflight verifies initial lifecycle requirements acceptance and current prerequisites", "P6 activation postflight verifies canonical outputs and outside-scope preservation"},
            {"P6 activation postflight rejects independent receipt model output baseline and evidence tampering", "P6 activation repeated delayed postflight performs no writes or lifecycle transition", "P6 activation completed postflight verifies start before expiry"},
            {"P6 activation real requirements refresh and integration lineage reaches restart readback", "P6 activation negative controls preserve original bytes and retained failures"},
            {"activation software contract requires production build complete CTest and named observations", "activation software evidence rejects missing observations and covered source drift"}};
        QJsonArray criteria;
        for (int i = 0; i < assertions.size(); ++i)
            criteria.append(QJsonObject{{"id", QString("P6-ACT-%1").arg(i + 1, 2, 10, QChar('0'))},
                {"requirement", requirements.at(i)}, {"check", "aramf_core_tests::--worker-tasks"}, {"assertions", array(assertions.at(i))}});
        return {{"id", id}, {"version", 1}, {"requirements", array(requirements)}, {"criteria", criteria},
            {"checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}},
            {"prerequisiteSubjects", QJsonArray{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"}},
            {"files", QJsonArray{"src/core/WorkerTaskServices.cpp", "src/core/WorkerTaskServices.h", "tests/WorkerTaskTests.cpp"}}};
    }
    if (id == "task-p6-prerequisite-closure/v1") {
        auto profile = acceptanceProfile("task-p6-requirements-operations/v1");
        profile.insert("id", id);
        const QString requirement = "P1 versioned source ownership covers its finite governance helper implementation closure and measured isolated freshness tests reject every helper change missing source and transitive stale dependency while preserving history and Foundation independence";
        auto requirements = profile.value("requirements").toArray(); requirements.append(requirement); profile.insert("requirements", requirements);
        auto criteria = profile.value("criteria").toArray();
        criteria.append(QJsonObject{{"id", "P1-SOURCE-01"}, {"requirement", requirement}, {"check", "aramf_freshness_tests"},
            {"assertions", QJsonArray{"P1 helper manifest is exact and versioned", "P1 helper changes invalidate source independently",
                "P1 helper omission fails closed and unrelated files remain irrelevant", "P1 helper source changes stale standalone and transitive freshness",
                "P1 helper restoration preserves historical evidence and graph independence"}}});
        profile.insert("criteria", criteria);
        auto checks = profile.value("checks").toArray(); checks.append("aramf_freshness_tests"); profile.insert("checks", checks);
        auto files = profile.value("files").toArray(); files.append("src/core/CertificationFreshness.cpp"); files.append("tests/CertificationFreshnessTests.cpp"); profile.insert("files", files);
        return profile;
    }
    if (id == "task-p6-build-registration/v1") {
        const QString requirement = "The exact protected P6 build registration has complete measured regression evidence and a unique current grant binding; this profile does not certify Code Bank behavior or lifecycle";
        return {{"id", id}, {"version", 1}, {"requirements", QJsonArray{requirement}},
            {"criteria", QJsonArray{QJsonObject{{"id", "P6-REG-01"}, {"requirement", requirement}, {"check", "aramf_core_tests::--worker-tasks"},
                {"assertions", QJsonArray{"P6 exact test registration links WorkerTaskServices", "exact uniquely consumed grant remains valid in read-only postflight"}}}}},
            {"checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}},
            {"files", QJsonArray{"CMakeLists.txt", "src/processes/P6/CodeBank.h", "src/processes/P6/CodeBank.cpp", "src/processes/P6/CodeBankStorage.cpp",
                "src/processes/P6/CodeBankVerification.cpp", "src/processes/P6/README.md", "tests/P6CodeBankTests.cpp"}}};
    }
    if (id == "task-p6-requirements-operations/v1") {
        const QStringList requirements{
            "A fixed prerequisite verification profile derives every F1-F4 and P1-P5 source manifest as read-only dependencies before READY without expanding write permissions or altering legacy contracts",
            "Prerequisite build CTest and acceptance evidence reject each missing altered unsafe unreadable or caller-expanded manifest dependency and cannot relabel historical proof",
            "P6 requirements preparation is read-only and derives exact canonical producer-owned files from a pure configuration preview",
            "Typed P6 requirements accept only complete canonical Code Bank criteria in active source-code metadata and preserve unrelated configuration lifecycle ownership and grants",
            "READY requirements plans bind current identity source software proof base bytes and fixed agent-provenance decision records and reject forged stale or concurrent requests before writes",
            "Canonical configuration save reload generation and verification agree with the exact effective output inventory while inactive academic directories and all academic options remain unchanged",
            "Requirements operations preserve original decisions and event history record actual agent provenance reject replay retain failed stages and provide pure exact receipt and canonical-output postflight",
            "Completed requirements operation lineage permits fresh prerequisite revalidation only with current verified operation receipts unchanged covered source and authenticated original measured software proof",
            "The first narrow P6 build grant can reference the exact existing scoped human Admin event with service provenance unique issuance current identity exact request and exact append-only CMake hashes",
            "P6 preparation and consumption reject used grants while exact uniquely bound consumed-contract postflight remains read-only and requires current evidence context scope and authority with no reconsumption",
            "The exact P6 test registration includes WorkerTaskServices sources and grant tests independently reject wrong authority identity scope request hashes expiry replay duplicate binding source or evidence drift",
            "Exact append-only protected grant or authority revocation is validated at issuance preparation consumption and bound postflight with independent wrong-link and revoked-chain rejection tests",
            "Grant recorder partial failure cannot activate authority retries preserve original partial events and canonical cold-start consistency context and existing task DAG remain coherent",
            "Existing authorization certification operation and configuration regressions plus current production build complete CTest campaigns and all named acceptance assertions pass"};
        const QList<QStringList> assertions{
            {"prerequisite verification binds every canonical F1-F4 and P1-P5 manifest file", "prerequisite read binding includes protected CMake without write permission", "unmapped prerequisite source remains forbidden to modify", "ordinary legacy contracts retain original dependency serialization"},
            {"prerequisite read binding rejects missing unsafe or unreadable source", "prerequisite read binding rejects caller supplied read-set expansion", "prerequisite build and CTest each reject omitted manifest coverage", "prerequisite source change invalidates measured evidence before operation", "prerequisite source change during producer execution records failure", "legacy verification proof cannot be relabeled with new read dependencies"},
            {"P6 requirements preparation is read-only", "P6 requirements plan derives exact producer-owned files", "P6 requirements preview rejects removals without writing update-plan"},
            {"P6 requirements rejects inactive scope owner", "P6 requirements rejects unrelated configuration delta", "P6 requirements rejects incomplete baseline criteria", "P6 requirements saved baseline matches source-code route"},
            {"P6 requirements rejects forged permissions", "P6 requirements rejects stale source or base before writes", "P6 requirements lock rejects concurrent apply"},
            {"P6 requirements configuration save reload and verification agree", "P6 requirements disabled academic options remain unchanged", "P6 requirements inventory excludes inactive academic directories"},
            {"P6 requirements decision records actual agent provenance", "P6 requirements preserves original decisions and event prefix", "P6 requirements replay preserves original artifacts", "P6 requirements partial failure remains failed with retained stage evidence", "P6 requirements postflight rejects changed receipt or baseline", "P6 requirements postflight rejects changed canonical outputs", "P6 requirements repeated postflight is read-only", "P6 requirements preserves unrelated dirty files and lifecycle"},
            {"P6 requirements verified lineage permits exact prerequisite refresh", "P6 requirements lineage rejects changed source or altered original proof"},
            {"existing authority issuance preserves original human event and count", "issued grant records actual service provenance and exact original event hash", "grant issuance changes no CMake source or configuration bytes", "grant issuance rejects each invalid or nonscoped original authority", "grant issuance rejects second use of original authority", "direct explicit Admin grant branch remains valid"},
            {"consumed grant rejected in preparation", "exact uniquely consumed grant remains valid in read-only postflight", "consumed grant rejects each incorrect binding and duplicate event", "consumed grant source or evidence drift is rejected", "repeated consumed grant postflight never appends or consumes again", "replay consumption rejected"},
            {"P6 exact test registration links WorkerTaskServices", "grant independently rejects identity scope request operation and content fingerprint mismatches", "grant expiry and concurrent issuance or consumption are rejected"},
            {"protected authority and grant revocation independently reject every lifecycle boundary", "protected revocation rejects forged or mismatched links"},
            {"grant partial append never activates unvalidated authority", "grant retry preserves original partial event", "grant mutation refreshes cold-start and consistency before current verification", "grant refresh rejects missing task DAG before mutation"},
            {"P6 repair profile exposes every exact named assertion", "independent audit-chain fixture control passes postflight"}};
        QJsonArray criteria;
        for (int i = 0; i < assertions.size(); ++i)
            criteria.append(QJsonObject{{"id", QString("CFG-P6-%1").arg(i + 1, 2, 10, QChar('0'))},
                {"requirement", requirements.at(i)}, {"check", "aramf_core_tests::--worker-tasks"}, {"assertions", array(assertions.at(i))}});
        return {{"id", id}, {"version", 1}, {"requirements", array(requirements)}, {"criteria", criteria},
            {"checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}},
            {"prerequisiteSubjects", QJsonArray{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"}},
            {"files", QJsonArray{"src/core/WorkerTaskServices.cpp", "src/core/WorkerTaskServices.h",
                "src/core/ConfigurationUpdateService.cpp", "src/core/ConfigurationUpdateService.h",
                "tests/WorkerTaskTests.cpp", "tests/ConfigurationUpdateTests.cpp"}}};
    }
    if (id == "task-certification-operations/v1") {
        const QStringList requirements{
            "Canonical certification operation preparation is read-only and derives exact producer-owned output paths",
            "Caller supplied paths hashes status or forged authority cannot expand certification operation permissions",
            "Apply requires the exact READY plan current software evidence source bindings and unchanged baseline",
            "Revalidation and integration preserve immutable history reject replay and retain failed partial attempts",
            "Postflight verifies exact canonical receipts current freshness context and unrelated file preservation",
            "Existing task authorization and evidence security regression remains valid",
            "Current production build and complete CTest regression pass"};
        const QList<QStringList> assertions{
            {"operation preparation is read-only", "operation paths are exact and service-owned"},
            {"operation rejects caller output authority", "operation rejects forged expanded permissions"},
            {"operation valid software control prepares READY", "operation rejects stale source before writes", "operation rejects changed baseline before writes", "operation lock prevents concurrent apply",
                "operation real measured software control validates", "operation rejects label-only producer PASS", "operation rejects failed command under PASS label",
                "operation rejects omitted enabled memory validation", "operation rejects enabled memory validation marked inapplicable", "operation accepts disabled optional memory validation",
                "operation rejects omitted mandatory cold-start path",
                "operation rejects mismatched CTest execution count", "operation rejects missing measured acceptance assertion", "operation rejects missing acceptance prerequisite",
                "operation refreshes acceptance after measured prerequisite rerun without rewriting history"},
            {"operation replay preserves original artifacts", "operation failure retains partial evidence", "operation preserves original certificate history",
                "operation rejects unexplained missing immutable record", "operation rejects unexplained truncated immutable record",
                "operation rejects structurally incomplete immutable record",
                "operation classifies only sealed exact failed pair without discarding bytes", "operation rejects altered retained failed pair",
                "operation new revalidation supersedes classified interrupted attempt", "operation supported recovery seals interrupted outputs only as failed",
                "operation resumes intact unsealed recovery without replacing retained evidence"},
            {"operation revalidation completes verified", "operation integration follows verified lineage", "operation postflight rejects changed receipt", "operation rejects foreign acceptance identity",
                "operation rejects each tampered canonical context output", "operation completed integration postflight is read-only",
                "operation acceptance remains valid through legitimate P6 lifecycle start"},
            {"independent audit-chain fixture control passes postflight"}};
        QJsonArray criteria;
        for (int i = 0; i < assertions.size(); ++i)
            criteria.append(QJsonObject{{"id", QString::number(i + 1)}, {"requirement", requirements.at(i)},
                {"check", "aramf_core_tests::--worker-tasks"}, {"assertions", array(assertions.at(i))}});
        return {{"id", id}, {"version", 1}, {"requirements", QJsonArray::fromStringList(requirements)}, {"criteria", criteria},
            {"checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}},
            {"files", QJsonArray{"src/core/WorkerTaskServices.cpp", "src/core/WorkerTaskServices.h",
                "src/core/CertificationRevalidation.cpp", "src/core/CertificationRevalidation.h",
                "src/core/FoundationCertificationCampaign.cpp", "src/core/FoundationCertificationCampaign.h",
                "tests/WorkerTaskTests.cpp", "tests/CertificationRevalidationTests.cpp", "tests/FoundationCertificationTests.cpp"}}};
    }
    if (id != "task-reauthorization-security/v1") return {};
    QJsonArray criteria;
    const auto criterion = [&](const QString& key, const QString& requirement, const QStringList& assertions) {
        criteria.append(QJsonObject{{"id", key}, {"requirement", requirement},
            {"check", "aramf_core_tests::--worker-tasks"}, {"assertions", array(assertions)}});
    };
    criterion("revocation", "Revoked grants are rejected during preparation and postflight", {
        "revoked unused grant is independently rejected before preparation",
        "revocation independently rejects a previously valid bound grant"});
    criterion("origin-links", "Authorization and binding events preserve exact original contract and report links", {
        "unused approval rejects a different original contract", "unused approval rejects a different original report",
        "authorization event independently rejects wrong originContractId",
        "authorization event independently rejects wrong originContractFingerprint",
        "authorization event independently rejects wrong originPostflightFingerprint",
        "authorization event independently rejects wrong authorityEventId",
        "binding event independently rejects wrong originContractId",
        "binding event independently rejects wrong originContractFingerprint",
        "binding event independently rejects wrong originPostflightFingerprint"});
    criterion("unique-binding", "A consumed grant has one immutable successor binding without postflight re-consumption", {
        "consumed approval cannot be replayed for preparation", "duplicate approval independently rejects its chain",
        "duplicate binding independently rejects its chain", "duplicate authorization independently rejects its chain",
        "extra consumption independently rejects a bound chain",
        "concurrent binding is blocked without consuming authority",
        "repeated valid postflight is read-only and never consumes the bound approval again"});
    criterion("security-boundaries", "Independent negative cases reject expiry identity project scope operation and fingerprint mismatch", {
        "complete expired audit chain is independently rejected", "wrong administrator is independently rejected",
        "grant for a different project is independently rejected", "wrong origin contract link is independently rejected",
        "binding event independently rejects wrong contractId", "unused approval rejects a changed source fingerprint",
        "independent exact-scope rejection for sourceFiles", "independent exact-scope rejection for canonicalInputs",
        "independent exact-scope rejection for operations", "independent exact-scope rejection for baseline",
        "missing approval for an unknown grant is independently rejected", "missing unique binding is independently rejected"});
    criterion("preservation", "Original contracts baselines reports evidence and unrelated chains remain intact", {
        "authorization and preparation preserve original contract, baseline, report, and evidence bytes",
        "unrelated negative chains do not revoke or contaminate the valid control",
        "independent audit-chain fixture control passes postflight"});
    criterion("producer-integrity", "Canonical software evidence records actual outcomes and rejects unsupported stale or tampered inputs", {
        "canonical producer executes real git whitespace checks",
        "read-back producer evidence satisfies only its actual check",
        "unsupported semantic acceptance, arbitrary checks and invalid timeout cannot create PASS evidence",
        "tampered contract cannot start evidence execution",
        "tampered artifact is rejected by collector and postflight independently",
        "changing covered source stales previously real producer evidence",
        "actual command failure is persisted as FAIL with evidence rather than discarded or promoted",
        "failed command retains real nonzero exit and raw output bytes"});
    QJsonArray requirements;
    for (const auto& value : criteria) requirements.append(value.toObject().value("requirement"));
    requirements.append("Current production build and complete CTest regression pass");
    return {{"id", id}, {"version", 1}, {"criteria", criteria}, {"requirements", requirements},
        {"checks", QJsonArray{"aramf_core_tests::--worker-tasks", "production-build", "ctest"}},
        {"files", QJsonArray{"src/core/WorkerTaskServices.cpp", "src/core/WorkerTaskServices.h",
            "tests/WorkerTaskTests.cpp", "tests/CertificationFreshnessTests.cpp"}}};
}

QJsonObject WorkerTaskServices::p6RequirementsBaseline()
{
    const QStringList requirements{
        "Stable project bank artifact revision and owner identity; resolve the acting principal and role from authoritative bindings and reject missing or forged identity",
        "Every content read mutation review and reuse requires exact action revision project scope and resource authorization; listing binds an exact project bank index revision and returns only individually authorized revision bindings; reject expired revoked foreign or altered grants and path escapes",
        "Ingest exact raw bytes with origin revision digest actor and timestamp; reject incomplete provenance source drift and conflicting identities; never execute ingested content",
        "Run declared fixed validators and retain measured results; review binds the exact revision criteria and evidence; caller PASS missing checks failed checks and stale reviews cannot promote",
        "Permit only declared ingested validated reviewed promoted transitions and explicit rejection revocation or supersession; retain immutable previous revisions and append-only transition history",
        "Persist versioned schema stable UUIDs revision ordinals and raw bytes across process restart using locking and create-only records; reject replay unsupported schemas corruption and incomplete writes while preserving evidence",
        "Bind raw content metadata authoritative requirements and dependencies with SHA256; detect missing duplicated conflicting or altered records and evidence",
        "Compatibility requires exact declared language toolchain interface and dependency bindings; missing incompatible ambiguous or cyclic bindings fail without guessed version-range compatibility",
        "Store indexed durable evidence by artifact and revision; retrieve after restart and independently rehash every evidence artifact before reuse; retain failed attempts and reject stale deleted tampered or foreign evidence",
        "Authorize reuse only for the current promoted compatible revision with fresh verified evidence and exact scope; stale superseded rejected revoked and incompatible revisions independently deny with reasons",
        "Typed API and CLI share ownership authorization persistence and failure rules; never modify caller source execute content expand TaskContract scope or create direct dependencies between Foundations",
        "Require current full project-bound prerequisite acceptance before P6 lifecycle start; persist and reload actual F4 iteration certification and completion; durable HOST_TEST certification binds P6 criteria source dependencies and measured evidence with explicit freshness invalidation"};
    const QList<QStringList> cases{
        {"identity-roundtrip", "missing-actor", "forged-actor", "wrong-owner"},
        {"authorized-action", "stranger-action", "foreign-project", "scope-mismatch", "traversal", "linked-escape", "altered-grant", "expired-grant", "revoked-grant"},
        {"exact-byte-ingestion", "incomplete-provenance", "source-race", "duplicate-identity", "content-never-executed"},
        {"measured-validation", "failed-check", "missing-check", "stale-review", "caller-pass-rejected"},
        {"legal-transitions", "illegal-transitions", "rejected-history", "revoked-history", "superseded-history"},
        {"restart-roundtrip", "schema-rejection", "concurrent-write", "replay", "interrupted-write", "retained-corruption"},
        {"content-tamper", "metadata-tamper", "evidence-tamper", "missing-record", "duplicate-record", "conflicting-record"},
        {"exact-compatibility", "language-mismatch", "toolchain-mismatch", "interface-mismatch", "dependency-mismatch", "missing-dependency", "dependency-cycle"},
        {"evidence-retrieval", "restart-evidence", "deleted-evidence", "stale-evidence", "foreign-evidence", "failed-attempt-preservation"},
        {"authorized-reuse", "stale-reuse", "superseded-reuse", "rejected-reuse", "revoked-reuse", "incompatible-reuse"},
        {"api-cli-equivalence", "foundation-independence", "unrelated-byte-preservation", "scope-not-expanded"},
        {"missing-prerequisite", "real-lifecycle", "certificate-readback", "altered-certificate", "criteria-freshness", "source-freshness", "dependency-freshness", "evidence-freshness"}};
    QJsonArray criteria;
    for (int i = 0; i < requirements.size(); ++i)
        criteria.append(QJsonObject{{"id", QString("P6-%1").arg(i + 1, 2, 10, QChar('0'))},
            {"requirement", requirements.at(i)}, {"testCases", array(cases.at(i))}});
    return {{"schemaVersion", 1}, {"baselineId", "P6-CODEBANK-REQ-1"}, {"subject", "P6"}, {"namespaceVersion", 2},
        {"ownerScope", "source-code"}, {"criteria", criteria},
        {"contract", QJsonObject{
            {"ownership", "P6 owns project-local bank material and immutable revision state; canonical project identity and exact task grants govern all access"},
            {"dependencies", QJsonArray{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"}},
            {"interfaces", QJsonArray{"ingest", "inspect", "list", "validate", "review", "promote", "reject", "revoke", "verifyEvidence", "authorizeReuse"}},
            {"storage", QJsonObject{{"root", "ARAMF_WORKER/code-bank"}, {"format", "versioned-json-and-raw-bytes"}, {"identity", "UUID-and-immutable-revision-ordinal"}, {"integrity", "SHA256"}}},
            {"evidence", "Retain actual raw validator reports and provenance durably with immutable identifiers; use existing F1 physical audit adapter without changing Foundation ownership"},
            {"lifecycle", "External P6 coordinator composes Foundation predicates and F4 public lifecycle; current prerequisite acceptance then measured HOST_TEST certification then completion"}}},
        {"engineeringDefaults", QJsonArray{
            QJsonObject{{"id", "reuse-project"}, {"value", "same-project-unless-explicit-exact-cross-project-grant"}, {"rationale", "No implicit cross-project authority"}},
            QJsonObject{{"id", "review-role"}, {"value", "owner-or-explicitly-nominated-reviewer"}, {"rationale", "Review authority remains explicit"}},
            QJsonObject{{"id", "compatibility"}, {"value", "exact-declared-bindings"}, {"rationale", "No unverified compatibility inference"}},
            QJsonObject{{"id", "ingestion"}, {"value", "local-bytes-without-execution-or-network"}, {"rationale", "Ingestion preserves material and provenance only"}},
            QJsonObject{{"id", "terminal-revision"}, {"value", "new-revision-required"}, {"rationale", "Terminal evidence and history stay immutable"}}}}};
}

namespace {
QJsonObject updateIsolationObservation(const QByteArray& output)
{
    QJsonObject payload; int count = 0;
    for (const auto& line : output.split('\n')) {
        const QByteArray marker = "UPDATE-ISOLATION-ACCEPTANCE ";
        if (line.startsWith(marker)) { ++count; payload = QJsonDocument::fromJson(line.mid(marker.size())).object(); }
    }
    const QString id = payload.value("runId").toString(), result = payload.value("resultRoot").toString(),
        legacy = payload.value("legacyPath").toString(), application = payload.value("applicationName").toString();
    if (count != 1 || payload.value("schemaVersion").toInt() != 1 || QUuid(id).isNull()
        || !QDir::isAbsolutePath(result) || !QDir::isAbsolutePath(legacy) || !result.contains(id)
        || !application.contains(id) || !legacy.contains(application)
        || payload.value("completed").toInt() != 310 || payload.value("passed").toInt() != 310
        || payload.value("failed").toInt(-1) != 0 || !payload.value("failedIds").isArray() || !payload.value("failedIds").toArray().isEmpty()
        || !payload.value("ioErrors").isArray() || !payload.value("ioErrors").toArray().isEmpty()) return {};
    for (const auto& flag : {"createOnlyVerified", "artifactReadbackVerified", "repositoryArtifactsUnchanged", "legacyIdentityVerified"})
        if (!payload.value(flag).toBool()) return {};
    for (const auto& name : {"UPDATE runner retains all 310 original assertions", "UPDATE runner isolates result and legacy paths", "UPDATE runner preserves create-only results and reports failed IDs"})
        if (payload.value("assertions").toObject().value(name) != "PASS") return {};
    return payload;
}
QJsonObject measuredProfile(const QJsonObject& contract)
{
    const auto request = contract.value("request").toObject();
    auto profile = WorkerTaskServices::acceptanceProfile(request.value("acceptanceProfile").toString());
    if (profile.isEmpty() && request.value("acceptanceProfile").toString().isEmpty()) {
        for (const QString& id : {QStringLiteral("task-certification-operations/v1"), QStringLiteral("task-p6-requirements-operations/v1"), QStringLiteral("task-p6-activation-governance/v1"), QStringLiteral("task-scope-registration-operations/v1"), QStringLiteral("task-android-profiles-members/v1")}) {
            const auto candidate = WorkerTaskServices::acceptanceProfile(id);
            if (semantic(request.value("definitionOfDone")) == semantic(candidate.value("requirements"))) profile = candidate;
        }
    }
    if (profile.isEmpty() || semantic(request.value("definitionOfDone")) != semantic(profile.value("requirements"))) return {};
    for (const auto& file : strings(request.value("files"))) if (!strings(profile.value("files")).contains(file)) return {};
    return profile;
}
QJsonObject measuredAssertions(const QJsonObject& profile, const QString& check, const QJsonObject& record)
{
    QJsonObject assertions;
    int workerReports = 0;
    int updateReports = 0;
    if (check == "ctest" && profile.value("id") == "task-p6-activation-campaign-isolation/v2") {
        const auto observations = record.value("observations").toArray();
        if (observations.size() != 3) return {};
        const auto inventory = observations.at(1).toObject(), execution = observations.at(2).toObject();
        const auto tests = QJsonDocument::fromJson(QByteArray::fromBase64(inventory.value("stdoutBase64").toString().toLatin1())).object().value("tests").toArray();
        bool includesFoundation = false;
        for (const auto& value : tests) includesFoundation |= value.toObject().value("name") == "aramf_foundation_certification";
        const auto summary = QRegularExpression("100% tests passed, 0 tests failed out of ([1-9][0-9]*)").match(
            QString::fromUtf8(QByteArray::fromBase64(execution.value("stdoutBase64").toString().toLatin1())));
        // Shared measuredPass additionally validates fixed commands, real build,
        // command exits and the complete unique inventory before acceptance.
        assertions.insert("complete CTest includes successful Foundation certification",
            includesFoundation && summary.hasMatch() && summary.captured(1).toInt() == tests.size()
                && execution.value("normalExit").toBool() && execution.value("exitCode").toInt(-1) == 0 ? "PASS" : "FAIL");
    }
    for (const auto& observation : record.value("observations").toArray()) {
        const auto value = observation.toObject();
        if (value.value("kind") != "command") continue;
        if (check == "aramf_core_tests::--worker-tasks" && value.value("arguments") == QJsonArray{"--worker-tasks"}) {
            for (const auto& line : QByteArray::fromBase64(value.value("stdoutBase64").toString().toLatin1()).split('\n')) {
                const QByteArray marker = "WORKER-TASK-ACCEPTANCE ";
                if (!line.startsWith(marker)) continue;
                const auto payload = QJsonDocument::fromJson(line.mid(marker.size())).object();
                if (payload.value("profile") == profile.value("id")) { ++workerReports; assertions = payload.value("assertions").toObject(); }
            }
        } else if (check == "aramf_update_campaign" && value.value("arguments") == QJsonArray{}
            && QFileInfo(value.value("executable").toString()).completeBaseName() == "aramf_update_campaign_runner") {
            const auto payload = updateIsolationObservation(QByteArray::fromBase64(value.value("stdoutBase64").toString().toLatin1()));
            if (payload.isEmpty()) return {};
            ++updateReports; assertions = payload.value("assertions").toObject();
        } else if (check == "aramf_freshness_tests" && value.value("arguments") == QJsonArray{}
            && QFileInfo(value.value("executable").toString()).completeBaseName() == "aramf_freshness_tests") {
            for (const auto& line : QByteArray::fromBase64(value.value("stdoutBase64").toString().toLatin1()).split('\n')) {
                const QByteArray marker = "FRESHNESS-ACCEPTANCE ";
                if (!line.startsWith(marker)) continue;
                const auto payload = QJsonDocument::fromJson(line.mid(marker.size())).object();
                const QString id = payload.value("id").toString();
                if (!id.isEmpty()) assertions.insert(id, assertions.contains(id) ? QJsonValue("FAIL") : payload.value("status"));
            }
        }
    }
    return (check == "aramf_core_tests::--worker-tasks" && workerReports != 1)
        || (check == "aramf_update_campaign" && updateReports != 2) ? QJsonObject{} : assertions;
}
// A producer label is not evidence that its named check ran. Reconstruct the
// successful result from the fixed runner's complete observations. This also
// validates frozen records after disposable build directories are removed.
bool measuredPass(const ProjectModel& model, const QJsonObject& contract, const QJsonObject& record,
                  const QMap<QString, QJsonObject>& records, const QMap<QString, QJsonObject>& entries)
{
    const QString check = record.value("check").toString();
    const auto started = QDateTime::fromString(record.value("startedAt").toString(), Qt::ISODateWithMs);
    const auto finished = QDateTime::fromString(record.value("finishedAt").toString(), Qt::ISODateWithMs);
    if (record.value("status") != "PASS" || !record.value("dependenciesUnchanged").toBool()
        || !started.isValid() || !finished.isValid() || finished < started
        || !strings(record.value("observedCoverage")).contains(check)) return false;
    const auto observations = record.value("observations").toArray();
    if (observations.isEmpty()) return false;
    for (const auto& value : observations) if (value.toObject().value("status") != "PASS") return false;
    const auto command = [&](int position, const QString& executable, const QStringList& arguments, const QString& cwd) {
        if (position >= observations.size()) return false;
        const auto value = observations.at(position).toObject();
        const QString actualExecutable = value.value("executable").toString();
        const bool exactExecutable = QFileInfo(executable).isAbsolute() ? QDir::cleanPath(actualExecutable) == QDir::cleanPath(executable)
            : QFileInfo(actualExecutable).completeBaseName() == executable;
        return value.value("kind") == "command" && exactExecutable
            && value.value("arguments") == QJsonArray::fromStringList(arguments) && value.value("workingDirectory") == cwd
            && value.value("started").toBool() && value.value("finished").toBool() && value.value("normalExit").toBool()
            && value.value("exitCode").isDouble() && value.value("exitCode").toInt() == 0
            && QRegularExpression("^[a-f0-9]{64}$").match(value.value("executableFingerprint").toString()).hasMatch()
            && value.value("stdoutBase64").isString() && value.value("stderrBase64").isString();
    };
    const auto output = [&](int position) { return QByteArray::fromBase64(observations.at(position).toObject().value("stdoutBase64").toString().toLatin1()); };
    if (check == "acceptance-criteria") {
        const auto profile = measuredProfile(contract);
        if (profile.isEmpty() || observations.size() != 1) return false;
        const auto observation = observations.first().toObject();
        if (observation.value("kind") != "canonical-acceptance-profile" || observation.value("profile") != profile) return false;
        QJsonArray requiredEntries;
        for (const auto& prerequisite : strings(profile.value("checks"))) {
            if (!records.contains(prerequisite) || !entries.contains(prerequisite)
                || !measuredPass(model, contract, records.value(prerequisite), records, entries)) return false;
            requiredEntries.append(entries.value(prerequisite));
        }
        if (semantic(observation.value("evidence")) != semantic(requiredEntries)) return false;
        QJsonArray criteria;
        for (const auto& value : profile.value("criteria").toArray()) {
            auto criterion = value.toObject();
            const QString criterionCheck = criterion.value("check").toString();
            if (!strings(profile.value("checks")).contains(criterionCheck)) return false;
            const auto assertions = measuredAssertions(profile, criterionCheck, records.value(criterionCheck));
            for (const auto& name : strings(criterion.value("assertions"))) if (assertions.value(name) != "PASS") return false;
            criterion.insert("status", "PASS"); criteria.append(criterion);
        }
        return observation.value("criteria") == criteria;
    }
    if (QStringList{"memory-consistency", "cold-start", "worker-topology", "impacted-validation"}.contains(check)) {
        const auto value = observations.first().toObject(), report = value.value("report").toObject();
        if (!(observations.size() == 1 && value.value("kind") == "canonical-service" && value.value("check") == check
            && value.value("error").toString().isEmpty() && !report.isEmpty()
            && (report.value("status") == "PASS" || report.value("overallStatus") == "PASS"))) return false;
        QSet<QString> names;
        for (const auto& item : report.value("checks").toArray()) {
            const auto checked = item.toObject();
            if (checked.value("status") != "PASS" && checked.value("status") != "NOT_APPLICABLE") return false;
            if (checked.value("status") == "PASS") names.insert(checked.value("id").toString(checked.value("name").toString()));
        }
        QStringList required;
        if (check == "memory-consistency") {
            required = {"event-identifiers-unique", "legacy-event-id-integrity-exceptions", "sequence-order", "manifest-next-sequence", "manifest-event-count",
                "checkpoint-identifiers-unique", "checkpoint-fields-valid", "checkpoint-references-valid", "event-provenance-valid", "persisted-scope-validity", "configured-validation-coverage"};
            for (const auto& option : model.memoryConfiguration().validationOptions)
                required.append(option == "cold-start-validation" ? "cold-start-fresh" : option);
        }
        else if (check == "cold-start") {
            required = {"event-log-semantic-recovery", "decisions-semantic-recovery", "checkpoints-semantic-recovery",
                "manifest-event-recovery", "current-decisions-recovery", "latest-checkpoint-recovery",
                AramfPaths::Decisions, AramfPaths::FrameworkKnowledge, AramfPaths::CurrentState, AramfPaths::MemoryConfiguration,
                AramfPaths::MemoryContract, AramfPaths::Manifest, AramfPaths::EventIdIntegrityExceptions,
                AramfPaths::ProjectKnowledge, AramfPaths::CompactionManifest};
            const auto memoryConfig = readObject(pathFor(model, workerPath(model, "memory/memory-config.json")));
            const bool controlPlane = memoryConfig.value("requireControlPlane").toBool(true);
            const QString writerMode = memoryConfig.value("writerMode").toString("agent-direct");
            const bool recordingEnabled = writerMode == "agent-direct" || writerMode == "project-local-tool";
            if (controlPlane) required << AramfPaths::AgentInstructions << AramfPaths::ProjectStatus;
            if (QFileInfo::exists(pathFor(model, workerPath(model, "project.json")))) required << AramfPaths::ProjectConfiguration;
            if (QFileInfo::exists(pathFor(model, workerPath(model, "worker-manifest.json")))) required << AramfPaths::WorkerManifest;
            if (memoryConfig.isEmpty() || report.value("requireControlPlane") != controlPlane || report.value("recordingEnabled") != recordingEnabled
                || !report.value("contractConfigConsistent").toBool()
                || !QRegularExpression("^[a-f0-9]{64}$").match(report.value("fingerprint").toString()).hasMatch()) return false;
        }
        else required = {"project-identity", "worker-topology", "canonical-owners", "p1-context-index", "p1-context-freshness", "memory-consistency", "cold-start", "freshness"};
        for (const auto& name : required) if (!names.contains(name)) return false;
        if (check == "memory-consistency" || check == "cold-start")
            return report.value("errors").isArray() && report.value("errors").toArray().isEmpty();
        return report.value("schemaVersion").toInt() == 1 && report.value("fingerprint") == projectConfigurationFingerprint(model, model.generationOptions())
            && report.value("contextValid").toBool() && report.value("topologyValid").toBool();
    }
    if (check == "git-diff-check") return observations.size() == 2
        && command(0, "git", {"diff", "--check"}, root(model)) && command(1, "git", {"diff", "--cached", "--check"}, root(model));
    // strings() sorts; command arguments must retain their execution order.
    QStringList orderedBuildArgs;
    for (const auto& value : observations.first().toObject().value("arguments").toArray()) orderedBuildArgs.append(value.toString());
    if (orderedBuildArgs.size() < 4 || orderedBuildArgs.at(0) != "--build") return false;
    const QString build = orderedBuildArgs.at(1), relative = QDir(root(model)).relativeFilePath(build);
    if (!safePath(root(model), relative) || QDir::cleanPath(pathFor(model, relative)) != QDir::cleanPath(build)) return false;
    QStringList expectedBuild{"--build", build, "--parallel", "2"};
    const bool focused = QStringList{"aramf_core_tests::--worker-tasks", "aramf_core_tests::--context-coordination", "aramf_core_tests::--p3-execution"}.contains(check);
    if (focused) expectedBuild << "--target" << "aramf_core_tests";
    else if (check == "aramf_freshness_tests") expectedBuild << "--target" << "aramf_freshness_tests";
    else if (check == "aramf_update_campaign") expectedBuild << "--target" << "aramf_update_campaign_runner";
    else if (check == "test_250") expectedBuild << "--target" << "aramf_campaign_runner";
    else if (check == "test_550-automated") expectedBuild << "--target" << "aramf_release_validation_runner";
    else if (check == "application-startup") expectedBuild << "--target" << "aramf";
    if (!command(0, "cmake", expectedBuild, root(model))) return false;
    if (check == "production-build") return observations.size() == 1;
    if (check == "application-startup") {
        if (observations.size() != 2) return false;
        const auto value = observations.at(1).toObject();
        return value.value("kind") == "production-startup" && value.value("executable") == QDir(build).filePath("aramf.exe")
            && value.value("started").toBool() && value.value("responsive").toBool() && value.value("gracefulExit").toBool()
            && value.value("normalExit").toBool()
            && value.value("visibleOwnedWindows").toInt() > 0 && value.value("processId").toDouble() > 0
            && value.value("exitCode").isDouble() && value.value("exitCode").toInt() == 0
            && QRegularExpression("^[a-f0-9]{64}$").match(value.value("executableFingerprint").toString()).hasMatch();
    }
    if (QStringList{"ctest", "targeted-regression", "affected-focused-tests", "affected-workflow-tests", "persistence-regression", "aramf-worker-migration"}.contains(check)) {
        if (observations.size() != 3 || !command(1, "ctest", {"--test-dir", build, "--show-only=json-v1"}, root(model))
            || !command(2, "ctest", {"--test-dir", build, "--output-on-failure", "--no-tests=error"}, root(model))) return false;
        const auto tests = QJsonDocument::fromJson(output(1)).object().value("tests").toArray();
        QSet<QString> names;
        for (const auto& value : tests) { const auto test = value.toObject(); if (test.value("command").toArray().isEmpty()) return false; names.insert(test.value("name").toString()); }
        QStringList required;
        if (check == "affected-workflow-tests") required = {"aramf_template_tests", "aramf_workflow_tests"};
        else if (check == "persistence-regression") required = {"aramf_core_tests", "aramf_configuration_update"};
        else if (check != "ctest") required = {"aramf_core_tests"};
        for (const auto& name : required) if (!names.contains(name)) return false;
        const auto summary = QRegularExpression("100% tests passed, 0 tests failed out of ([1-9][0-9]*)").match(QString::fromUtf8(output(2)));
        return !tests.isEmpty() && names.size() == tests.size() && summary.hasMatch() && summary.captured(1).toInt() == tests.size();
    }
#ifdef Q_OS_WIN
    const QString suffix = ".exe";
#else
    const QString suffix;
#endif
    if (check == "aramf_update_campaign") {
        if (observations.size() != 3 || !command(1, QDir(build).filePath("aramf_update_campaign_runner" + suffix), {}, build)
            || !command(2, QDir(build).filePath("aramf_update_campaign_runner" + suffix), {}, build)) return false;
        const auto first = updateIsolationObservation(output(1)), second = updateIsolationObservation(output(2));
        if (first.isEmpty() || second.isEmpty()) return false;
        for (const auto& key : {"runId", "resultRoot", "legacyPath", "applicationName"})
            if (first.value(key) == second.value(key)) return false;
        return true;
    }
    const QString target = focused ? "aramf_core_tests" : check == "aramf_freshness_tests" ? check : check == "test_250" ? "aramf_campaign_runner" : "aramf_release_validation_runner";
    if ((!focused && check != "aramf_freshness_tests" && check != "test_250" && check != "test_550-automated") || observations.size() != 2
        || !command(1, QDir(build).filePath(target + suffix), focused ? QStringList{check.section("::", 1)} : QStringList{}, build)) return false;
    if (check == "test_250") return output(1).contains("completed=250 passed=250 failed=0");
    if (check == "test_550-automated") return output(1).contains("automatedCompleted=250 automatedPass=250") && output(1).contains("failures=0");
    return !output(1).trimmed().isEmpty();
}
}

QJsonObject WorkerTaskServices::runCheck(const ProjectModel& model, const QJsonObject& suppliedContract,
                                        const QString& check, const QString& buildDirectory, int timeoutMilliseconds)
{
    const auto blocked = [](const QString& reason) { return QJsonObject{{"status", "BLOCKED"}, {"error", reason}}; };
    const auto contract = suppliedContract.contains("contract") ? suppliedContract.value("contract").toObject() : suppliedContract;
    const QSet<QString> internal{"memory-consistency", "cold-start", "worker-topology", "impacted-validation"};
    const QSet<QString> ctestChecks{"ctest", "targeted-regression", "affected-focused-tests", "affected-workflow-tests",
        "persistence-regression", "aramf-worker-migration"};
    const QSet<QString> focused{"aramf_core_tests::--worker-tasks", "aramf_core_tests::--context-coordination",
        "aramf_core_tests::--p3-execution"};
    auto profile = acceptanceProfile(contract.value("request").toObject().value("acceptanceProfile").toString());
    // An earlier ordinary READY source contract may explicitly name the exact
    // registered requirements before their producer exists. Recognition never
    // rewrites that immutable request, its baseline, or its permissions.
    if (profile.isEmpty() && contract.value("request").toObject().value("acceptanceProfile").toString().isEmpty()) {
        for (const QString& id : {QStringLiteral("task-certification-operations/v1"), QStringLiteral("task-p6-activation-governance/v1"), QStringLiteral("task-scope-registration-operations/v1"), QStringLiteral("task-android-profiles-members/v1")}) {
            const auto candidate = acceptanceProfile(id);
            if (semantic(contract.value("request").toObject().value("definitionOfDone")) == semantic(candidate.value("requirements"))) profile = candidate;
        }
    }
    const bool mappedAcceptance = check == "acceptance-criteria" && !profile.isEmpty();
    if (mappedAcceptance) {
        const auto request = WorkerTaskRequest::fromJson(contract.value("request").toObject());
        bool valid = semantic(array(request.definitionOfDone)) == semantic(profile.value("requirements"));
        for (const auto& path : request.files) valid &= strings(profile.value("files")).contains(path);
        if (!valid) return blocked("Acceptance profile requirements and exact source scope do not match");
    }
    if (!strings(contract.value("requiredEvidence")).contains(check)
        || (!mappedAcceptance && !internal.contains(check) && !ctestChecks.contains(check) && !focused.contains(check)
            && check != "git-diff-check" && check != "production-build" && check != "test_250" && check != "test_550-automated"
            && check != "application-startup" && check != "aramf_freshness_tests" && check != "aramf_update_campaign"))
        return blocked("No supported canonical executable/report mapping for this required check; semantic acceptance and physical certification cannot be inferred");
#ifndef Q_OS_WIN
    if (check == "application-startup") return blocked("Production window startup observation is currently supported only on Windows");
#endif
    if (timeoutMilliseconds < 1 || timeoutMilliseconds > 21600000) return blocked("Check timeout must be within 1..21600000 ms");
    const QString permission = check == "production-build" ? "run-builds"
        : internal.contains(check) || check == "git-diff-check" || mappedAcceptance ? "run-project-tools" : "run-tests";
    if (!model.aiConfiguration().permissions.contains(permission)
        || !model.aiConfiguration().permissions.contains("create-files")) return blocked("Canonical AI permissions do not permit this check and its output");
    if (!mappedAcceptance && !internal.contains(check) && check != "git-diff-check" && !model.aiConfiguration().permissions.contains("run-builds"))
        return blocked("Executable checks also require permission to build current source");
    const QString directory = evidenceDirectory(contract);
    if (directory.isEmpty() || !evidencePathAllowed(model, directory + "/probe.json"))
        return blocked("Evidence requires an ignored, untracked, non-resource build directory without links");
    const auto before = postflight(model, contract);
    if (before.value("status") != "PASS") return blocked("Current TaskContract boundary is not PASS");
    const QString dependency = before.value("evidenceFingerprints").toObject().value(check).toString();
    if (dependency.isEmpty()) return blocked("Canonical check dependency fingerprint is unavailable");
    const QString started = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QJsonArray observations;
    QStringList coveredChecks{check};
    bool passed = true;
    const auto command = [&](const QString& program, const QStringList& arguments, const QString& cwd) {
        QProcess process;
        process.setWorkingDirectory(cwd);
        const QString executable = QFileInfo(program).isAbsolute() ? program : QStandardPaths::findExecutable(program);
        const QString executableBefore = fileHash(executable);
        process.start(executable, arguments);
        const bool startedOk = !executable.isEmpty() && process.waitForStarted(10000);
        const bool finished = startedOk && process.waitForFinished(timeoutMilliseconds);
        if (startedOk && !finished) { process.kill(); process.waitForFinished(10000); }
        const QByteArray standardOutput = process.readAllStandardOutput(), standardError = process.readAllStandardError();
        const bool ok = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0
            && executableBefore != "MISSING" && executableBefore == fileHash(executable);
        observations.append(QJsonObject{{"kind", "command"}, {"executable", executable}, {"executableFingerprint", executableBefore},
            {"arguments", QJsonArray::fromStringList(arguments)}, {"workingDirectory", cwd}, {"started", startedOk},
            {"finished", finished}, {"normalExit", process.exitStatus() == QProcess::NormalExit}, {"exitCode", process.exitCode()},
            {"stdoutBase64", QString::fromLatin1(standardOutput.toBase64())}, {"stderrBase64", QString::fromLatin1(standardError.toBase64())},
            {"status", ok ? "PASS" : "FAIL"}});
        passed &= ok;
        return standardOutput;
    };
    if (mappedAcceptance) {
        const auto index = checkEvidence(model, contract, true);
        if (index.value("status") != "PASS") return blocked("Cannot inspect canonical acceptance evidence");
        QJsonArray relevant;
        QMap<QString, QJsonObject> acceptanceRecords;
        for (const auto& item : index.value("evidence").toArray()) {
            const auto entry = item.toObject();
            if (!strings(profile.value("checks")).contains(entry.value("check").toString())) continue;
            relevant.append(entry);
            acceptanceRecords.insert(entry.value("check").toString(), readObject(pathFor(model, entry.value("artifact").toString())));
        }
        const auto validated = postflight(model, contract, relevant);
        QSet<QString> acceptedChecks;
        for (const auto& entry : validated.value("acceptedEvidence").toArray()) acceptedChecks.insert(entry.toObject().value("check").toString());
        passed = validated.value("status") == "PASS";
        for (const auto& required : strings(profile.value("checks"))) passed &= acceptedChecks.contains(required);
        QJsonArray decisions;
        for (const auto& item : profile.value("criteria").toArray()) {
            auto criterion = item.toObject();
            const QString criterionCheck = criterion.value("check").toString();
            const auto assertions = measuredAssertions(profile, criterionCheck, acceptanceRecords.value(criterionCheck));
            bool criterionPass = strings(profile.value("checks")).contains(criterionCheck);
            for (const auto& assertion : strings(criterion.value("assertions"))) criterionPass &= assertions.value(assertion) == "PASS";
            criterion.insert("status", criterionPass ? "PASS" : "FAIL");
            decisions.append(criterion);
            passed &= criterionPass;
        }
        observations.append(QJsonObject{{"kind", "canonical-acceptance-profile"}, {"profile", profile}, {"criteria", decisions},
            {"evidence", relevant}, {"status", passed ? "PASS" : "FAIL"}});
    } else if (internal.contains(check)) {
        QString error;
        QJsonObject report;
        const auto previousSuffix = AramfPaths::detail::workerSuffixOverride();
        AramfPaths::setRuntimeWorkerNameSuffix(model.workerNameSuffix());
        if (check == "memory-consistency") report = ProjectMemory().validate(model.projectPath(), &error, false);
        else if (check == "cold-start") report = ProjectMemory().validateColdStart(model.projectPath(), &error);
        else {
            report = VerificationServices().verify(model, model.generationOptions(), false).summary;
        }
        passed = error.isEmpty() && (report.value("status") == "PASS" || report.value("overallStatus") == "PASS");
        AramfPaths::setRuntimeWorkerNameSuffix(previousSuffix);
        observations.append(QJsonObject{{"kind", "canonical-service"}, {"check", check}, {"report", report}, {"error", error},
            {"status", passed ? "PASS" : "FAIL"}});
    } else if (check == "git-diff-check") {
        command("git", {"diff", "--check"}, root(model));
        command("git", {"diff", "--cached", "--check"}, root(model));
    } else {
        const QString relative = QDir(root(model)).relativeFilePath(QDir(root(model)).absoluteFilePath(buildDirectory));
        const QString build = pathFor(model, relative);
        if (buildDirectory.isEmpty() || !safePath(root(model), relative) || !evidencePathAllowed(model, relative + "/probe.json"))
            return blocked("Build directory must be an ignored, non-resource directory inside this project");
        QFile cache(QDir(build).filePath("CMakeCache.txt"));
        if (!cache.open(QIODevice::ReadOnly)) return blocked("A configured CMake build is required");
        const QString cacheText = QString::fromUtf8(cache.readAll());
        const auto match = QRegularExpression("(?:^|\\n)CMAKE_HOME_DIRECTORY:INTERNAL=([^\\r\\n]+)").match(cacheText);
        if (!match.hasMatch() || QFileInfo(match.captured(1)).canonicalFilePath() != QFileInfo(root(model)).canonicalFilePath())
            return blocked("CMake build belongs to a different source checkout");
        // A configured cache alone does not bind an executable to today's
        // source. Build immediately before every executable observation and
        // retain that result too. Run the CLI from a separate copy on Windows
        // when its normal target is among the rebuilt executables.
        if ((ctestChecks.contains(check) || focused.contains(check) || check == "aramf_freshness_tests" || check == "aramf_update_campaign")
            && !QRegularExpression("(?:^|\\n)BUILD_TESTING:BOOL=(?:ON|TRUE|1)(?:\\r?\\n|$)").match(cacheText).hasMatch())
            return blocked("Current CMake configuration must enable tests; leftover test executables are not evidence");
        QStringList buildArguments{"--build", build, "--parallel", "2"};
        if (focused.contains(check)) buildArguments << "--target" << "aramf_core_tests";
        else if (check == "aramf_freshness_tests") buildArguments << "--target" << "aramf_freshness_tests";
        else if (check == "aramf_update_campaign") buildArguments << "--target" << "aramf_update_campaign_runner";
        else if (check == "test_250") buildArguments << "--target" << "aramf_campaign_runner";
        else if (check == "test_550-automated") buildArguments << "--target" << "aramf_release_validation_runner";
        else if (check == "application-startup") buildArguments << "--target" << "aramf";
        command("cmake", buildArguments, root(model));
        if (check == "application-startup" && passed) {
#ifdef Q_OS_WIN
            const QString executable = QDir(build).filePath("aramf.exe");
            const QString executableBefore = fileHash(executable);
            QProcess application;
            application.setWorkingDirectory(build);
            application.start(executable, {});
            const bool startedOk = application.waitForStarted(10000);
            HANDLE handle = startedOk ? OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                static_cast<DWORD>(application.processId())) : nullptr;
            QElapsedTimer startupTimer;
            startupTimer.start();
            const int startupTimeout = qMin(timeoutMilliseconds, 30000);
            DWORD idleError = handle ? ERROR_SUCCESS : GetLastError();
            const DWORD idle = handle ? WaitForInputIdle(handle, static_cast<DWORD>(qMin(startupTimeout, 1000))) : WAIT_FAILED;
            if (handle && idle == WAIT_FAILED) idleError = GetLastError();
            StartupWindowObservation observed;
            observed.processId = static_cast<DWORD>(application.processId());
            bool responsive = false;
            // This Qt GUI is built with the console subsystem. Windows can
            // report ERROR_NOT_GUI_PROCESS from WaitForInputIdle even after
            // its real top-level window appears. Observe the owned window
            // directly, and retain the idle API result only as a diagnostic.
            while (startedOk && application.state() != QProcess::NotRunning && startupTimer.elapsed() < startupTimeout) {
                observed.windows.clear();
                EnumWindows(collectStartupWindows, reinterpret_cast<LPARAM>(&observed));
                responsive = !observed.windows.isEmpty();
                for (const auto window : observed.windows) {
                    RECT bounds{};
                    DWORD_PTR response = 0;
                    responsive &= GetWindowRect(window, &bounds) && bounds.right > bounds.left && bounds.bottom > bounds.top
                        && SendMessageTimeoutW(window, WM_NULL, 0, 0, SMTO_ABORTIFHUNG | SMTO_BLOCK,
                            static_cast<UINT>(qMin<qint64>(2000, qMax<qint64>(1, startupTimeout - startupTimer.elapsed()))), &response) != 0;
                }
                if (responsive) break;
                application.waitForFinished(static_cast<int>(qMin<qint64>(50, qMax<qint64>(1, startupTimeout - startupTimer.elapsed()))));
            }
            // Only windows owned by this newly started process may be closed.
            for (const auto window : observed.windows) PostMessageW(window, WM_CLOSE, 0, 0);
            const bool closed = startedOk && application.waitForFinished(qMin(timeoutMilliseconds, 10000));
            if (startedOk && !closed) { application.kill(); application.waitForFinished(10000); }
            if (handle) CloseHandle(handle);
            const bool startupPass = startedOk && responsive && closed
                && application.exitStatus() == QProcess::NormalExit && application.exitCode() == 0
                && executableBefore != "MISSING" && executableBefore == fileHash(executable);
            observations.append(QJsonObject{{"kind", "production-startup"}, {"executable", executable},
                {"executableFingerprint", executableBefore}, {"processId", static_cast<double>(observed.processId)},
                {"started", startedOk}, {"inputIdle", idle == 0}, {"visibleOwnedWindows", observed.windows.size()},
                {"inputIdleResult", static_cast<double>(idle)}, {"inputIdleError", static_cast<double>(idleError)},
                {"startupObservationMilliseconds", startupTimer.elapsed()},
                {"responsive", responsive}, {"gracefulExit", closed}, {"exitCode", application.exitCode()},
                {"normalExit", application.exitStatus() == QProcess::NormalExit},
                {"stdoutBase64", QString::fromLatin1(application.readAllStandardOutput().toBase64())},
                {"stderrBase64", QString::fromLatin1(application.readAllStandardError().toBase64())},
                {"status", startupPass ? "PASS" : "FAIL"}});
            passed &= startupPass;
#endif
        } else if (check != "production-build" && passed && ctestChecks.contains(check)) {
            const auto inventory = command("ctest", {"--test-dir", build, "--show-only=json-v1"}, root(model));
            const auto testList = QJsonDocument::fromJson(inventory).object().value("tests").toArray();
            QSet<QString> names;
            for (const auto& test : testList) names.insert(test.toObject().value("name").toString());
            const auto covers = [&](const QString& label) {
                QStringList requiredNames;
                if (label == "affected-workflow-tests") requiredNames = {"aramf_template_tests", "aramf_workflow_tests"};
                else if (label == "persistence-regression") requiredNames = {"aramf_core_tests", "aramf_configuration_update"};
                else if (label != "ctest") requiredNames = {"aramf_core_tests"};
                bool covered = !testList.isEmpty();
                for (const auto& name : requiredNames) covered &= names.contains(name);
                return covered;
            };
            passed &= !testList.isEmpty();
            passed &= covers(check);
            for (const auto& label : strings(contract.value("requiredEvidence")))
                if (ctestChecks.contains(label) && covers(label) && !coveredChecks.contains(label)) coveredChecks.append(label);
            // These aggregate labels conservatively execute the complete real
            // suite; no arbitrary caller command can satisfy a named check.
            if (passed) {
                const auto output = command("ctest", {"--test-dir", build, "--output-on-failure", "--no-tests=error"}, root(model));
                const auto summary = QRegularExpression("100% tests passed, 0 tests failed out of ([1-9][0-9]*)").match(QString::fromUtf8(output));
                passed &= summary.hasMatch() && summary.captured(1).toInt() == testList.size();
            }
        } else if (check != "production-build" && passed) {
#ifdef Q_OS_WIN
            const QString suffix = ".exe";
#else
            const QString suffix;
#endif
            QString target = "aramf_core_tests";
            QStringList arguments;
            if (focused.contains(check)) arguments << check.section("::", 1);
            else target = check == "aramf_freshness_tests" ? check : check == "aramf_update_campaign" ? "aramf_update_campaign_runner" : check == "test_250" ? "aramf_campaign_runner" : "aramf_release_validation_runner";
            const auto output = command(QDir(build).filePath(target + suffix), arguments, build);
            if (check == "aramf_update_campaign") {
                const auto first = updateIsolationObservation(output);
                const auto second = updateIsolationObservation(command(QDir(build).filePath(target + suffix), {}, build));
                passed &= !first.isEmpty() && !second.isEmpty();
                for (const auto& key : {"runId", "resultRoot", "legacyPath", "applicationName"}) passed &= first.value(key) != second.value(key);
            } else if (check == "test_250") passed &= output.contains("completed=250 passed=250 failed=0");
            else if (check == "test_550-automated") passed &= output.contains("automatedCompleted=250 automatedPass=250") && output.contains("failures=0");
            else passed &= !output.trimmed().isEmpty();
        }
    }
    const auto after = postflight(model, contract);
    QJsonArray entries;
    QJsonObject primaryRecord;
    QString primaryIndex;
    bool allPassed = passed;
    const QString observationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (const auto& covered : coveredChecks) {
        // All covered labels use fingerprints observed before the one real
        // execution. A later caller cannot attach a new label retroactively.
        const QString coveredDependency = before.value("evidenceFingerprints").toObject().value(covered).toString();
        const bool unchanged = after.value("status") == "PASS" && !coveredDependency.isEmpty()
            && after.value("evidenceFingerprints").toObject().value(covered).toString() == coveredDependency;
        const bool coveredPass = passed && unchanged;
        allPassed &= coveredPass;
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString relative = directory + '/' + id + "/record.json";
        if (!evidencePathAllowed(model, relative) || !QDir().mkpath(QFileInfo(pathFor(model, relative)).absolutePath()))
            return blocked("Cannot create isolated immutable check output");
        QJsonObject record{{"schemaVersion", 1}, {"producer", "WorkerTaskServices/run-check/v1"}, {"check", covered},
            {"contractId", contract.value("contractId")}, {"projectId", model.projectId()}, {"projectRoot", root(model)},
            {"dependencyFingerprint", coveredDependency}, {"dependenciesUnchanged", unchanged}, {"startedAt", started},
            {"finishedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"recordId", id},
            {"observationId", observationId}, {"observedCoverage", array(coveredChecks)},
            {"observations", observations}, {"status", coveredPass ? "PASS" : "FAIL"}};
        const auto contents = QJsonDocument(record).toJson(QJsonDocument::Indented);
        if (!writeNew(pathFor(model, relative), contents)) return blocked("Immutable check record could not be persisted and read back");
        const QJsonObject entry{{"check", covered}, {"artifact", relative}, {"artifactFingerprint", digest(contents)}};
        const QJsonObject index{{"schemaVersion", 1}, {"contractId", contract.value("contractId")}, {"evidence", QJsonArray{entry}}};
        const QString indexPath = directory + '/' + id + "/index.json";
        if (!writeNew(pathFor(model, indexPath), QJsonDocument(index).toJson(QJsonDocument::Indented)))
            return blocked("Check record retained but its immutable index could not be persisted");
        entries.append(entry);
        if (covered == check) { primaryRecord = record; primaryIndex = indexPath; }
    }
    return {{"status", allPassed ? "PASS" : "FAIL"}, {"record", primaryRecord}, {"evidence", entries}, {"index", primaryIndex}};
}

QJsonObject WorkerTaskServices::checkEvidence(const ProjectModel& model, const QJsonObject& suppliedContract, bool acceptancePrerequisitesOnly)
{
    const auto contract = suppliedContract.contains("contract") ? suppliedContract.value("contract").toObject() : suppliedContract;
    const QString directory = evidenceDirectory(contract);
    if (directory.isEmpty() || !evidencePathAllowed(model, directory + "/probe.json") || postflight(model, contract).value("status") != "PASS")
        return {{"status", "BLOCKED"}, {"error", "Cannot collect evidence for an invalid contract or output directory"}};
    QMap<QString, QJsonObject> latest;
    QMap<QString, QString> timestamps;
    QDirIterator records(pathFor(model, directory), {"record.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (records.hasNext()) {
        const QString record = records.next();
        if (!QFileInfo::exists(QFileInfo(record).absolutePath() + "/index.json"))
            return {{"status", "BLOCKED"}, {"error", "An unindexed check observation is retained; collection cannot skip a possibly newer failure"}};
    }
    QDirIterator iterator(pathFor(model, directory), {"index.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString indexPath = QDir(root(model)).relativeFilePath(iterator.next());
        if (!evidencePathAllowed(model, indexPath)) return {{"status", "BLOCKED"}, {"error", "Unsafe evidence index"}};
        const auto index = readObject(pathFor(model, indexPath));
        if (index.value("contractId") != contract.value("contractId") || index.value("evidence").toArray().size() != 1)
            return {{"status", "BLOCKED"}, {"error", "Invalid immutable check index"}};
        const auto entry = index.value("evidence").toArray().first().toObject();
        const QString artifact = entry.value("artifact").toString();
        const auto record = readObject(pathFor(model, artifact));
        const QString check = entry.value("check").toString();
        if (artifact != QFileInfo(indexPath).path() + "/record.json" || !evidencePathAllowed(model, artifact)
            || fileHash(pathFor(model, artifact)) != entry.value("artifactFingerprint")
            || record.value("producer") != "WorkerTaskServices/run-check/v1" || record.value("check") != check
            || record.value("contractId") != contract.value("contractId") || record.value("projectId") != model.projectId()
            || record.value("projectRoot") != root(model) || !strings(contract.value("requiredEvidence")).contains(check)
            || !QDateTime::fromString(record.value("finishedAt").toString(), Qt::ISODateWithMs).isValid())
            return {{"status", "BLOCKED"}, {"error", "Check evidence is corrupt, tampered or bound to another contract/project"}};
        const QString timestamp = record.value("finishedAt").toString() + '/' + record.value("recordId").toString();
        if (!timestamps.contains(check) || timestamps.value(check) < timestamp) {
            timestamps.insert(check, timestamp);
            latest.insert(check, entry);
        }
    }
    if (acceptancePrerequisitesOnly) {
        const auto checks = strings(measuredProfile(contract).value("checks"));
        if (checks.isEmpty()) return {{"status", "BLOCKED"}, {"error", "Registered acceptance prerequisites are unavailable"}};
        for (auto it = latest.begin(); it != latest.end();) {
            if (!checks.contains(it.key())) it = latest.erase(it); else ++it;
        }
    }
    QJsonArray entries;
    QMap<QString, QJsonObject> measured;
    for (auto it = latest.begin(); it != latest.end(); ++it)
        measured.insert(it.key(), readObject(pathFor(model, it.value().value("artifact").toString())));
    for (auto it = measured.begin(); it != measured.end(); ++it)
        if (it.value().value("status") == "PASS" && !measuredPass(model, contract, it.value(), measured, latest))
            return {{"status", "BLOCKED"}, {"error", "Named PASS lacks complete successful canonical producer observations: " + it.key()}};
    for (const auto& entry : latest) entries.append(entry);
    return {{"status", "PASS"}, {"contractId", contract.value("contractId")}, {"evidence", entries},
        {"selection", "Latest immutable observation per check; earlier PASS and FAIL records remain retained"}};
}

// Commit single-use consumption only after the exact task is fully verified.

// A lock serializes issuance/consumption. Failed or incomplete postflight leaves
// the grant unconsumed but cannot authorize another task. Future preparation and
// postflight reject the persisted consumption event rather than trusting flags.
namespace {
const QString operationKind = "canonical-certification-operation/v1";
const QStringList prerequisiteSubjects{"F1", "F2", "F3", "F4", "P1", "P2", "P3", "P4", "P5"};
QStringList operationContextPaths()
{
    return {"ARAMF_WORKER/context/context-index.json", "ARAMF_WORKER/context/compressed-context.json",
        "ARAMF_WORKER/context/freshness.json", "ARAMF_WORKER/context/agent-adapters.json"};
}
bool operationSafePath(const ProjectModel& model, const QString& path)
{
    if (!safePath(root(model), path)) return false;
    QFileInfo part(pathFor(model, path));
    while (part.absoluteFilePath() != root(model) && part.absoluteFilePath() != part.absolutePath()) {
        if (part.isSymLink() || part.isJunction()) return false;
        part = QFileInfo(part.absolutePath());
    }
    return true;
}
QString operationRevision(const ProjectModel& model)
{
    QByteArray bytes;
    return git(root(model), {"rev-parse", "HEAD^{commit}"}, &bytes) ? QString::fromUtf8(bytes).trimmed() : QString{};
}
QJsonObject operationSources(const ProjectModel& model, QJsonArray& errors)
{
    QJsonObject result;

    for (const auto& subject : prerequisiteSubjects) {
        QString source, contract, error;
        if (!CertificationSourceManifestProvider::fingerprint(root(model), subject, &source, &error)
            || !CertificationContractManifestProvider::fingerprint(subject, root(model), &contract, &error))
            issue(errors, "OPERATION_SOURCE_INVALID", error, subject);
        result.insert(subject, QJsonObject{{"sourceFingerprint", source}, {"contractFingerprint", contract}});
    }
    return result;
}
QJsonObject operationShape(const ProjectModel& model, const QJsonObject& intent, const QString& id, QJsonArray& errors)
{
    const QSet<QString> keys{"kind", "subjects", "semanticChanges", "reason"};
    for (auto it = intent.begin(); it != intent.end(); ++it)
        if (!keys.contains(it.key())) issue(errors, "OPERATION_INTENT_INVALID", "Caller output paths, results, fingerprints and authority are not accepted.", it.key());
    const QString kind = intent.value("kind").toString();
    const bool revalidation = kind == "revalidate-current/v1";
    if ((!revalidation && kind != "accept-current-integration/v1") || intent.value("reason").toString().trimmed().isEmpty()
        || !QRegularExpression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(id).hasMatch()
        || worker(model) != "ARAMF_WORKER") {
        issue(errors, "OPERATION_INTENT_INVALID", "A supported operation, reason, canonical UUID and standard Worker are required.");
        return {};
    }
    const auto subjects = strings(intent.value("subjects"));
    const auto semantics = intent.value("semanticChanges").toObject();
    if (!intent.value("subjects").isArray() || !intent.value("semanticChanges").isObject()
        || subjects.size() != intent.value("subjects").toArray().size()
        || (revalidation && (subjects.isEmpty() || semantics.keys() != subjects))
        || (!revalidation && (!subjects.isEmpty() || !semantics.isEmpty())))
        issue(errors, "OPERATION_INTENT_INVALID", "Exact unique subjects and explicit semantic-change booleans are required for revalidation only.");
    QJsonObject outputs, ids;
    const QString directory = "ARAMF_WORKER/certification/operations/" + id;
    outputs.insert(directory + "/software-evidence.json", "WorkerTaskServices");
    outputs.insert(directory + "/receipt.json", "WorkerTaskServices");
    outputs.insert(directory + "/seal.json", "WorkerTaskServices");
    outputs.insert(directory + "/recovery.json", "WorkerTaskServices");
    outputs.insert(directory + "/recovery-seal.json", "WorkerTaskServices");
    for (const auto& path : operationContextPaths()) outputs.insert(path, "ContextCoordinationService");
    for (const auto& subject : subjects) {
        if (!prerequisiteSubjects.contains(subject) || !semantics.value(subject).isBool()) {
            issue(errors, "OPERATION_SUBJECT_INVALID", "Subject or semantic classification is outside the canonical prerequisite set.", subject);
            continue;
        }
        const QString recordId = "reval-" + QUuid::createUuidV5(QUuid(id), (kind + '/' + subject).toUtf8()).toString(QUuid::WithoutBraces);
        ids.insert(subject, recordId);
        const QString prefix = "ARAMF_WORKER/certification/revalidation/" + subject + '/' + recordId;
        outputs.insert(prefix + "/evidence.json", "CertificationRevalidationService");
        outputs.insert(prefix + "/revalidation.json", "CertificationRevalidationService");
    }
    if (revalidation) outputs.insert("ARAMF_WORKER/certification/current-freshness-state.json", "CertificationRevalidationService");
    else {
        const QString saved = QDir(root(model)).relativeFilePath(model.projectFilePath());
        if (!saved.endsWith(".aramf.json") || !QFileInfo::exists(pathFor(model, saved)))
            issue(errors, "OPERATION_MODEL_INVALID", "A contained existing saved project is required.");
        outputs.insert(saved, "ProjectPersistence");
        outputs.insert("ARAMF_WORKER/project.json", "FoundationCertificationService");
        outputs.insert("ARAMF_WORKER/memory/cold-start-validation.json", "EvidenceStorage");
        outputs.insert("ARAMF_WORKER/certification/integration/" + id + "/acceptance.json", "FoundationCertificationCampaign");
    }
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (!operationSafePath(model, it.key())) issue(errors, "OPERATION_PATH_UNSAFE", "Canonical operation output follows a link or escapes its project.", it.key());
    return {{"outputs", outputs}, {"ids", ids}, {"proof", directory + "/software-evidence.json"},
        {"receipt", directory + "/receipt.json"}, {"seal", directory + "/seal.json"}, {"subjects", array(subjects)}};
}
QJsonObject operationSnapshot(const ProjectModel& model, QJsonArray& errors)
{
    WorkerResourceObservation observation;
    return snapshot(model, errors);
}
bool operationBoundary(const ProjectModel& model, const QJsonObject& before, const QJsonObject& after,
                       const QStringList& allowed, QJsonArray& errors)
{
    QStringList paths = before.keys() + after.keys(); paths.removeDuplicates();
    for (const auto& path : paths) {
        if (before.value(path) == after.value(path) || path.startsWith("external-manifest:")) continue;
        if (path.startsWith("external:")) {
            const QString id = path.mid(9);
            const auto oldManifest = before.value("external-manifest:" + id).toObject();
            const auto newManifest = after.value("external-manifest:" + id).toObject();
            QString resourceRoot;
            for (const auto& resource : model.resources()) if (resource.id == id) resourceRoot = pathFor(model, resource.location);
            bool covered = QFileInfo(resourceRoot).isDir() && oldManifest.value("valid").toBool() && newManifest.value("valid").toBool()
                && oldManifest.value("policy") == newManifest.value("policy");
            const auto oldEntries = oldManifest.value("entries").toObject(), newEntries = newManifest.value("entries").toObject();
            QStringList children = oldEntries.keys() + newEntries.keys(); children.removeDuplicates();
            for (const auto& child : children) {
                if (oldEntries.value(child) == newEntries.value(child)) continue;
                const QString relative = QDir(root(model)).relativeFilePath(QDir(resourceRoot).filePath(child));
                const auto oldEntry = oldEntries.value(child).toObject(), newEntry = newEntries.value(child).toObject();
                if (oldEntry.isEmpty() && newEntry.value("type") == "folder") {
                    bool ancestor = false;
                    for (const auto& output : allowed) ancestor |= output.startsWith(relative + '/');
                    covered &= ancestor;
                } else covered &= allowed.contains(relative) && operationSafePath(model, relative)
                    && before.value(relative).toString("MISSING") == oldEntry.value("sha256").toString("MISSING")
                    && after.value(relative).toString("MISSING") == newEntry.value("sha256").toString("MISSING");
            }
            if (covered) continue;
        } else if (allowed.contains(path) && after.value(path).toString("MISSING") != "MISSING") continue;
        issue(errors, "OPERATION_SCOPE_VIOLATION", "Actual change is outside the exact canonical operation output set.", path);
    }
    return state(errors).startsWith("READY");
}
QJsonObject operationRecordBinding(const QJsonObject& contract, const QJsonObject& shape, const QString& proofHash,
                                   const QString& subject = {})
{
    QJsonObject value{{"operationId", contract.value("operationId")}, {"contractId", contract.value("contractId")},
        {"kind", contract.value("intent").toObject().value("kind")},
        {"softwareEvidence", QJsonObject{{"reference", shape.value("proof")}, {"fingerprint", proofHash}}}};
    if (!subject.isEmpty()) {
        value.insert("subject", subject);
        const auto sources = contract.value("sourceBindings").toObject().value(subject).toObject();
        value.insert("sourceFingerprint", sources.value("sourceFingerprint"));
        value.insert("contractFingerprint", sources.value("contractFingerprint"));
    } else value.insert("projectBinding", contract.value("projectBinding"));
    return value;
}
void validateFrozenSoftwareProof(const ProjectModel& model, const QJsonObject& software, QJsonArray& errors)
{
    // Reports attest the configuration observed when the real checks ran.
    // A later explicitly governed requirements operation may change that
    // configuration; its current products are validated independently below.
    ProjectModel observedModel;
    const ProjectModel* reportModel = &model;
    if (software.contains("softwareModel")) {
        QString error;
        if (!software.value("softwareModel").isObject()
            || !ProjectPersistence().fromJson(&observedModel, software.value("softwareModel").toObject(), &error)
            || observedModel.projectId() != model.projectId() || root(observedModel) != root(model)
            || worker(observedModel) != worker(model)
            || fingerprint(ProjectPersistence().configuration(observedModel))
                != software.value("softwareContract").toObject().value("binding").toObject().value("modelFingerprint")) {
            issue(errors, "OPERATION_SOFTWARE_PROOF_INVALID", "Frozen report configuration is malformed or belongs to another project.");
            return;
        }
        reportModel = &observedModel;
    }
    const auto softwareContract = software.value("softwareContract").toObject();
    const auto softwareReport = software.value("postflight").toObject();
    const auto records = software.value("records").toObject();
    QMap<QString, QJsonObject> measuredRecords, measuredEntries;
    if (softwareReport.value("status") != "PASS" || softwareReport.value("completionState") != "VERIFIED"
        || !softwareReport.value("missingEvidence").toArray().isEmpty() || softwareReport.value("contractId") != softwareContract.value("contractId"))
        issue(errors, "OPERATION_SOFTWARE_PROOF_INVALID", "Frozen software proof is incomplete or unbound.");
    for (const auto& entryValue : softwareReport.value("acceptedEvidence").toArray()) {
        const auto entry = entryValue.toObject();
        const auto captured = records.value(entry.value("artifact").toString()).toObject();
        const auto bytes = QByteArray::fromBase64(captured.value("bytesBase64").toString().toLatin1());
        const auto record = QJsonDocument::fromJson(bytes).object();
        const QString check = entry.value("check").toString();
        if (measuredRecords.contains(check) || check != record.value("check"))
            issue(errors, "OPERATION_SOFTWARE_PROOF_INVALID", "Frozen software proof contains duplicate or mismatched named checks.");
        measuredRecords.insert(check, record); measuredEntries.insert(check, entry);
        if (digest(bytes) != captured.value("fingerprint") || digest(bytes) != entry.value("artifactFingerprint")
            || record.value("contractId") != softwareContract.value("contractId") || record.value("status") != "PASS"
            || record.value("projectId") != model.projectId() || record.value("projectRoot") != root(model)
            || record.value("producer") != "WorkerTaskServices/run-check/v1" || !record.value("dependenciesUnchanged").toBool()
            || record.value("dependencyFingerprint") != softwareReport.value("evidenceFingerprints").toObject().value(record.value("check").toString()))
            issue(errors, "OPERATION_SOFTWARE_PROOF_INVALID", "Frozen measured software record changed or lost its original binding.");
    }
    auto originalSoftware = softwareContract; originalSoftware.remove("contractId");
    if (fingerprint(originalSoftware) != softwareContract.value("contractId") || measuredProfile(softwareContract).isEmpty())
        issue(errors, "OPERATION_SOFTWARE_PROOF_INVALID", "Original software contract or registered acceptance mapping is invalid.");
    for (const auto& check : strings(softwareContract.value("requiredEvidence"))) {
        if (check == "diff-boundary") continue;
        if (!measuredRecords.contains(check) || !measuredPass(*reportModel, softwareContract, measuredRecords.value(check), measuredRecords, measuredEntries))
            issue(errors, "OPERATION_SOFTWARE_PROOF_INVALID", "A required measured successful check is missing or semantically invalid.", check);
    }
}

const QString requirementsKind = "canonical-p6-requirements-operation/v1";
const QString requirementsProducer = "WorkerTaskServices/p6-requirements-operation/v1";
QStringList requirementsVerificationPaths()
{
    return {"ARAMF_WORKER/verification/latest-validation.json", "ARAMF_WORKER/verification/verification-result.json",
        "ARAMF_WORKER/memory/memory-consistency-validation.json"};
}
QByteArray rawBytes(const QString& path)
{
    QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
bool requirementsProposal(const ProjectModel& model, const QJsonObject& intent, ProjectModel* proposed, QJsonArray& errors)
{
    const QSet<QString> allowed{"kind", "baselineVersion", "agentId", "reason"};
    for (auto it = intent.begin(); it != intent.end(); ++it)
        if (!allowed.contains(it.key())) issue(errors, "REQUIREMENTS_INTENT_INVALID", "Only the fixed baseline version and actual agent provenance are accepted; caller model paths authority and results are forbidden.", it.key());
    if (intent.value("kind") != "p6-requirements/v1" || intent.value("baselineVersion") != "P6-CODEBANK-REQ-1"
        || !QRegularExpression("^[A-Za-z0-9_./-]{1,128}$").match(intent.value("agentId").toString()).hasMatch()
        || QStringList{"none", "human", "Admin Morgan Lindbom"}.contains(intent.value("agentId").toString())
        || intent.value("reason").toString().trimmed().isEmpty() || intent.value("reason").toString().size() > 1024)
        issue(errors, "REQUIREMENTS_INTENT_INVALID", "A fixed P6 baseline version, non-human actual agent identity and commissioning reason are required.");
    const auto original = ProjectPersistence().toJson(model);
    const auto academic = original.value("academic").toObject();
    if (academic.value("enabled").toBool() || academic.value("thesisDocumentation").toObject().value("enabled").toBool()
        || academic.value("reportDocumentation").toObject().value("enabled").toBool())
        issue(errors, "REQUIREMENTS_CONFIGURATION_INVALID", "This narrow operation preserves disabled academic outputs; active documents need a separately governed output plan.");
    if (worker(model) != "ARAMF_WORKER" || !model.ruleConfiguration().projectScopes.contains("source-code")
        || !model.ruleConfiguration().scopeMetadata.value("source-code").isObject())
        issue(errors, "REQUIREMENTS_SCOPE_INVALID", "The existing active source-code scope must own the P6 requirements metadata.");
    QString error;
    if (!ProjectPersistence().fromJson(proposed, original, &error)) issue(errors, "REQUIREMENTS_CONFIGURATION_INVALID", error);
    proposed->setProjectPath(model.projectPath()); proposed->setProjectFilePath(model.projectFilePath());
    auto rules = proposed->ruleConfiguration(); auto metadata = rules.scopeMetadata.value("source-code").toObject();
    auto baseline = WorkerTaskServices::p6RequirementsBaseline();
    baseline.insert("provenance", QJsonObject{{"actor", "agent"}, {"agentId", intent.value("agentId")},
        {"tool", requirementsProducer}, {"source", intent.value("reason")},
        {"classification", "Commissioned requirements with explicitly recorded agent engineering defaults"}});
    metadata.insert("p6CodeBankRequirements", baseline); rules.scopeMetadata.insert("source-code", metadata); proposed->setRuleConfiguration(rules);
    return state(errors).startsWith("READY");
}
QJsonObject requirementsShape(const ProjectModel& model, const QJsonObject& intent, const QString& id,
                              ProjectModel* proposed, QJsonArray& errors)
{
    requirementsProposal(model, intent, proposed, errors);
    if (!QRegularExpression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(id).hasMatch())
        issue(errors, "REQUIREMENTS_INTENT_INVALID", "A canonical service allocated operation UUID is required.");
    const QString directory = "ARAMF_WORKER/verification/requirements/" + id;
    QJsonObject outputs;
    for (const auto& path : ConfigurationUpdateService::effectiveOutputFiles(*proposed)) {
        if (path.endsWith("/Thesis") || path.endsWith("/Report") || QFileInfo(pathFor(model, path)).isDir())
            issue(errors, "REQUIREMENTS_OUTPUT_INVALID", "Only exact regular producer files can be authorized, never directories.", path);
        outputs.insert(path, "ConfigurationUpdateService");
    }
    const QString saved = QDir(root(model)).relativeFilePath(model.projectFilePath());
    if (!saved.endsWith(".aramf.json") || !QFileInfo(pathFor(model, saved)).isFile())
        issue(errors, "REQUIREMENTS_CONFIGURATION_INVALID", "An exact existing project-local saved configuration is required.");
    outputs.insert(saved, "ProjectPersistence");
    for (const QString& path : {QStringLiteral("memory/decisions.md"), QStringLiteral("memory/event-log.jsonl"),
        QStringLiteral("memory/memory-manifest.json"), QStringLiteral("memory/current-state.md"), QStringLiteral("memory/metrics.json"),
        QStringLiteral("memory/memory-consistency-validation.json"), QStringLiteral("memory/cold-start-validation.json")})
        outputs.insert(workerPath(model, path), "ProjectMemory");
    for (const auto& path : operationContextPaths()) outputs.insert(path, "ContextCoordinationService");
    for (const auto& path : requirementsVerificationPaths()) outputs.insert(path, "VerificationServices");
    for (const QString& name : {QStringLiteral("proof.json"), QStringLiteral("receipt.json"), QStringLiteral("seal.json")})
        outputs.insert(directory + '/' + name, "WorkerTaskServices");
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (!operationSafePath(model, it.key())) issue(errors, "REQUIREMENTS_OUTPUT_INVALID", "A producer output escapes this project or follows a link.", it.key());
    const auto baseline = proposed->ruleConfiguration().scopeMetadata.value("source-code").toObject().value("p6CodeBankRequirements").toObject();
    const QString baselineHash = digest(QJsonDocument(baseline).toJson(QJsonDocument::Compact));
    return {{"outputs", outputs}, {"proof", directory + "/proof.json"}, {"receipt", directory + "/receipt.json"}, {"seal", directory + "/seal.json"},
        {"baseline", baseline}, {"baselineFingerprint", baselineHash}, {"decisionId", "p6-code-bank-requirements-" + id},
        {"decisionTopic", "Canonical P6 Code Bank requirements"},
        {"decisionSummary", "Commissioned P6 Canonical Code Bank baseline P6-CODEBANK-REQ-1 is registered in rules.scopeMetadata[source-code].p6CodeBankRequirements; SHA256 " + baselineHash
            + ". Its twelve criteria cover identity, exact authority, ingestion, validation, transitions, persistence, integrity, compatibility, evidence, reuse, boundaries and lifecycle certification. Project-local storage, same-project default reuse, nominated review, exact compatibility and no execution/network ingestion are agent engineering defaults. This decision authorizes no implementation, protected CMake change or lifecycle start."}};
}
QJsonObject deriveRequirementsOperation(const ProjectModel& model, const QJsonObject& intent,
                                        const QJsonObject& suppliedVerification, const QString& id)
{
    QJsonArray errors; ProjectModel proposed;
    const auto shape = requirementsShape(model, intent, id, &proposed, errors); const auto outputs = shape.value("outputs").toObject();
    validateCanonical(model, errors);
    const auto original = ProjectPersistence().toJson(model);
    ProjectModel saved; QString error;
    if (!ProjectPersistence().load(&saved, model.projectFilePath(), &error) || ProjectPersistence().toJson(saved) != original)
        issue(errors, "REQUIREMENTS_CONFIGURATION_INVALID", "The supplied model must exactly match its current saved canonical configuration.");
    if (model.processVersionState().hasActiveProcess || model.processVersionState().hasActiveStructure
        || !model.processVersionState().foundationIntegrationValid)
        issue(errors, "REQUIREMENTS_LIFECYCLE_BLOCKED", "Completed current prerequisite integration and no active lifecycle are required.");
    if (!QFileInfo::exists(pathFor(model, "ARAMF_WORKER/context/task-dag.json")))
        issue(errors, "REQUIREMENTS_CONTEXT_BLOCKED", "The existing task DAG must be preserved before any operation.");
    if (model.ruleConfiguration().scopeMetadata.value("source-code").toObject().contains("p6CodeBankRequirements"))
        issue(errors, "REQUIREMENTS_REPLAY", "This first-baseline operation cannot replace an existing requirements baseline.");
    for (const QString& permission : {QStringLiteral("read-project-files"), QStringLiteral("modify-files"), QStringLiteral("create-files")})
        if (!model.aiConfiguration().permissions.contains(permission)) issue(errors, "AGENT_PERMISSION_MISSING", "Required canonical operation permission is absent.", permission);
    if (!model.memoryConfiguration().maintenanceOptions.contains("record-decisions"))
        issue(errors, "REQUIREMENTS_DECISION_BLOCKED", "Canonical decision recording must already be enabled.");
    QJsonObject prefixes;
    for (const QString& path : {QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"), QStringLiteral("ARAMF_WORKER/memory/decisions.md")})
        prefixes.insert(path, QString::fromLatin1(rawBytes(pathFor(model, path)).toBase64()));
    const auto decisionBytes = rawBytes(pathFor(model, "ARAMF_WORKER/memory/decisions.md"));
    auto normalized = decisionBytes; normalized.replace("\r\n", "\n");
#ifdef Q_OS_WIN
    normalized.replace("\n", "\r\n");
#endif
    if (normalized != decisionBytes || decisionBytes.isEmpty())
        issue(errors, "REQUIREMENTS_HISTORY_ENCODING", "The existing decision producer cannot preserve these exact decision bytes; resolve encoding through a separately scoped workflow.");
    for (const QString& key : {QStringLiteral("proof"), QStringLiteral("receipt"), QStringLiteral("seal")})
        if (QFileInfo::exists(pathFor(model, shape.value(key).toString()))) issue(errors, "REQUIREMENTS_REPLAY", "An immutable planned destination already exists.", shape.value(key).toString());
    const auto currentPreview = ConfigurationUpdateService::preview(model);
    const auto preview = ConfigurationUpdateService::preview(proposed);
    if (!currentPreview.success || !currentPreview.noChange || !preview.success || preview.removalBlocked || preview.noChange)
        issue(errors, "REQUIREMENTS_CONFIGURATION_INVALID", "Canonical configuration must be current and the pure preview must contain only the new P6 baseline addition: " + preview.error);
    const auto snapshotBefore = operationSnapshot(model, errors); const auto sources = operationSources(model, errors);
    const auto verification = suppliedVerification.contains("contract") ? suppliedVerification.value("contract").toObject() : suppliedVerification;
    QJsonObject proof;
    if (state(errors).startsWith("READY")) {
        if (verification.value("contractKind") != operationKind || verification.value("intent").toObject().value("kind") != "accept-current-integration/v1")
            issue(errors, "REQUIREMENTS_PREREQUISITE_INVALID", "The current completed exact integration operation is required before baseline registration.");
        else {
            const auto report = WorkerTaskServices::postflightCertificationOperation(model, verification);
            if (report.value("completionState") != "VERIFIED") issue(errors, "REQUIREMENTS_PREREQUISITE_INVALID", "Current prerequisite integration operation is not VERIFIED.");
            else proof = verification.value("softwareProof").toObject();
        }
    }
    if (sources != operationSources(model, errors) || snapshotBefore != operationSnapshot(model, errors))
        issue(errors, "REQUIREMENTS_BASE_CHANGED", "Source or project baseline changed while preparing the exact operation.");
    QJsonArray permissions;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        permissions.append(QJsonObject{{"path", it.key()}, {"owner", it.value()}, {"actions", QJsonArray{"READ", it.key().startsWith("ARAMF_WORKER/verification/requirements/") ? "GENERATE_ONCE" : "UPDATE_DERIVED"}}});
    QJsonObject contract{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractKind", requirementsKind}, {"operationId", id}, {"intent", intent},
        {"binding", binding(model)}, {"projectBinding", QJsonObject{{"projectId", model.projectId()}, {"projectRoot", root(model)}, {"workerIdentity", worker(model)}}},
        {"beforeModel", original}, {"proposedModel", ProjectPersistence().toJson(proposed)}, {"preview", preview.plan},
        {"sourceBindings", sources}, {"sourceRevision", operationRevision(model)}, {"baseline", snapshotBefore}, {"historyPrefixes", prefixes},
        {"verificationContract", verification}, {"softwareProof", proof}, {"requirements", shape.value("baseline")}, {"requirementsFingerprint", shape.value("baselineFingerprint")},
        {"permittedFiles", array(outputs.keys())}, {"filePermissions", permissions},
        {"requiredEvidence", QJsonArray{"canonical-configuration-readback", "exact-agent-decision", "sealed-operation-receipts", "current-software-source", "canonical-context", "exact-diff-boundary"}},
        {"preflight", QJsonObject{{"status", state(errors)}, {"errors", errors}}}, {"completionState", "NOT_STARTED"}};
    contract.insert("contractId", fingerprint(contract)); return contract;
}
QJsonObject deriveCertificationOperation(const ProjectModel& model, const QJsonObject& intent,
                                        const QJsonObject& suppliedVerification, const QString& id)
{
    QJsonArray errors;
    const auto shape = operationShape(model, intent, id, errors);
    const auto outputs = shape.value("outputs").toObject();
    const bool revalidation = intent.value("kind") == "revalidate-current/v1";
    validateCanonical(model, errors);
    const auto permissions = model.aiConfiguration().permissions;
    if (!permissions.contains("read-project-files") || !permissions.contains("modify-files") || !permissions.contains("create-files"))
        issue(errors, "AGENT_PERMISSION_MISSING", "Canonical operation preparation requires read, modify and create permissions.");
    const auto lifecycle = processVersionStateToJson(model.processVersionState());
    if (model.processVersionState().hasActiveProcess || model.processVersionState().hasActiveStructure)
        issue(errors, "OPERATION_LIFECYCLE_BLOCKED", "Canonical prerequisite operations require no active lifecycle.");
    if (!QFileInfo::exists(pathFor(model, "ARAMF_WORKER/context/task-dag.json")))
        issue(errors, "OPERATION_CONTEXT_BLOCKED", "Existing task DAG must be preserved; initialize it through its separate canonical scope first.");
    if (!revalidation && !model.processVersionState().foundationIntegrationValid)
        issue(errors, "OPERATION_LIFECYCLE_BLOCKED", "This exact operation cannot change the integration lifecycle projection.");
    const QJsonObject identity{{"projectId", model.projectId()}, {"projectRoot", QFileInfo(root(model)).canonicalFilePath()}, {"workerIdentity", worker(model)}};
    const auto canonicalProject = readObject(pathFor(model, workerPath(model, "project.json")));
    if (canonicalProject.value("projectId") != model.projectId() || canonicalProject.value("workerIdentity") != worker(model))
        issue(errors, "OPERATION_IDENTITY_INVALID", "Canonical saved-model and Worker identities disagree.");
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        const bool createOnly = it.key().contains("/operations/") || it.key().contains("/revalidation/") || it.key().contains("/integration/");
        if (createOnly && QFileInfo::exists(pathFor(model, it.key())))
            issue(errors, "OPERATION_REPLAY", "A reserved create-only output already exists.", it.key());
    }
    const auto baseline = operationSnapshot(model, errors);
    const auto sources = operationSources(model, errors); // Before software observations.
    for (const auto& subject : prerequisiteSubjects) {
        QString historyError; QJsonObject latest;
        CertificationRevalidationService::latest(root(model), subject, &latest, &historyError);
        if (!historyError.isEmpty()) issue(errors, "OPERATION_HISTORY_INVALID", historyError, subject);
    }
    const QString revision = operationRevision(model);
    if (revision.size() != 40) issue(errors, "OPERATION_REVISION_INVALID", "A real current Git revision is required.");
    const auto selected = strings(shape.value("subjects"));
    for (const auto& subject : selected) {
        for (const auto& dependency : CertificationFreshnessService::dependencyManifest(subject).directDependencies)
            if (!selected.contains(dependency.subject)
                && CertificationFreshnessService::evaluate(dependency.subject, root(model)).status != CertificationFreshnessStatus::Fresh)
                issue(errors, "OPERATION_DEPENDENCY_STALE", "A stale prerequisite must be an earlier step in this exact batch.", dependency.subject);
        if (QStringList{"F2", "F3", "F4"}.contains(subject)) {
            QString error;
            if (!CertificationRevalidationService::validateCurrentSource(root(model), subject, revision, &error))
                issue(errors, "OPERATION_COMMITTED_SOURCE_REQUIRED", error, subject);
        }
    }
    const auto verification = suppliedVerification.contains("contract") ? suppliedVerification.value("contract").toObject() : suppliedVerification;
    QJsonObject proof;
    if (state(errors).startsWith("READY")) {
        if (!revalidation) {
            if (verification.value("contractKind") != operationKind || verification.value("intent").toObject().value("kind") != "revalidate-current/v1")
                issue(errors, "OPERATION_VERIFICATION_INVALID", "Integration requires the completed exact prerequisite revalidation contract.");
            else {
                const auto report = WorkerTaskServices::postflightCertificationOperation(model, verification);
                if (report.value("completionState") != "VERIFIED") issue(errors, "OPERATION_VERIFICATION_INVALID", "Prerequisite operation postflight is not currently VERIFIED.");
                else proof = verification.value("softwareProof").toObject();
            }
        } else if (verification.value("contractKind") == requirementsKind) {
            const auto report = WorkerTaskServices::postflightRequirementsOperation(model, verification);
            if (report.value("completionState") != "VERIFIED" || verification.value("sourceBindings") != sources)
                issue(errors, "OPERATION_VERIFICATION_INVALID", "Requirements lineage must be currently VERIFIED with unchanged covered source and original software proof.");
            else proof = verification.value("softwareProof").toObject();
        } else if (verification.contains("contractKind")) {
            issue(errors, "OPERATION_VERIFICATION_INVALID", "Revalidation requires current measured software or exact verified requirements lineage.");
        } else {
            const auto index = WorkerTaskServices::checkEvidence(model, verification);
            const auto report = WorkerTaskServices::postflight(model, verification, index.value("evidence").toArray());
            const auto required = strings(verification.value("requiredEvidence"));
            {
            WorkerResourceObservation coverageObservation;
            for (const QString& check : {QStringLiteral("ctest"), QStringLiteral("production-build")}) {
                const auto dependencies = strings(verification.value("evidenceDependencies").toObject().value(check));
                for (const auto& subject : prerequisiteSubjects)
                    for (const auto& file : CertificationSourceManifestProvider::manifest(subject).files) {
                        bool covered = dependencies.contains(file);
                        for (const auto& resource : model.resources()) {
                            if (!dependencies.contains("external:" + resource.id)) continue;
                            const QString location = pathFor(model, resource.location);
                            const QString child = QDir(location).relativeFilePath(pathFor(model, file));
                            const auto observed = WorkerContextResolver::resourceSnapshot(model, resource.id);
                            covered |= observed.value("valid").toBool() && observed.value("entries").toObject().value(child).toObject().value("sha256")
                                == fileHash(pathFor(model, file));
                            covered |= QFileInfo(location).canonicalFilePath() == QFileInfo(pathFor(model, file)).canonicalFilePath()
                                && observed.value("valid").toBool();
                        }
                        if (!covered) issue(errors, "OPERATION_SOURCE_UNCOVERED", "Measured build and CTest dependencies must cover every prerequisite source file.", check + ':' + file);
                    }
            }
            }
            if (index.value("status") != "PASS" || report.value("status") != "PASS" || report.value("completionState") != "VERIFIED"
                || !report.value("missingEvidence").toArray().isEmpty()
                || !required.contains("acceptance-criteria") || !required.contains("ctest") || !required.contains("production-build"))
                issue(errors, "OPERATION_VERIFICATION_INVALID", "Complete current measured acceptance, production build and full CTest evidence are required.");
            else {
                QJsonObject records;
                for (const auto& item : report.value("acceptedEvidence").toArray()) {
                    const auto entry = item.toObject();
                    const QString path = entry.value("artifact").toString();
                    QFile file(pathFor(model, path));
                    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
                    const auto record = QJsonDocument::fromJson(bytes).object();
                    if (digest(bytes) != entry.value("artifactFingerprint") || record.value("producer") != "WorkerTaskServices/run-check/v1"
                        || record.value("status") != "PASS" || !record.value("dependenciesUnchanged").toBool())
                        issue(errors, "OPERATION_VERIFICATION_INVALID", "Measured software record failed exact readback.", path);
                    records.insert(path, QJsonObject{{"fingerprint", digest(bytes)}, {"bytesBase64", QString::fromLatin1(bytes.toBase64())}});
                }
                proof = {{"softwareContract", verification}, {"softwareModel", ProjectPersistence().toJson(model)}, {"postflight", report}, {"records", records}};
            }
        }
    }
    if (sources != operationSources(model, errors) || baseline != operationSnapshot(model, errors))
        issue(errors, "OPERATION_INPUT_CHANGED", "Inputs changed while canonical preparation collected software evidence.");
    QJsonArray filePermissions;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        filePermissions.append(QJsonObject{{"path", it.key()}, {"writer", it.value()}, {"owner", "canonical-service"},
            {"action", it.key().contains("/operations/") || it.key().contains("/revalidation/") || it.key().contains("/integration/") ? "GENERATE_ONCE" : "UPDATE_DERIVED"}});
    QJsonObject result{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractKind", operationKind},
        {"operationId", id}, {"intent", intent}, {"verificationContract", verification}, {"softwareProof", proof},
        {"binding", binding(model)}, {"projectBinding", identity}, {"lifecycle", lifecycle}, {"sourceRevision", revision},
        {"sourceBindings", sources}, {"baseline", baseline},
        {"freshnessBase", readObject(pathFor(model, "ARAMF_WORKER/certification/current-freshness-state.json"))},
        {"permittedFiles", array(outputs.keys())}, {"filePermissions", filePermissions},
        {"impact", QJsonObject{{"affectedSubjects", revalidation ? shape.value("subjects") : QJsonValue(array(prerequisiteSubjects))},
            {"canonicalProducers", outputs}, {"lifecycleMutation", false}, {"sourceMutation", false}}},
        {"risk", QJsonObject{{"level", "CRITICAL"}, {"reasons", QJsonArray{"governance", "persistence"}}}},
        {"validation", QJsonObject{{"route", "canonical-certification-operation/v1"},
            {"policyFingerprint", fileHash(pathFor(model, "ARAMF_WORKER/routing/validation-policy.json"))},
            {"softwarePrerequisite", "complete measured acceptance, production build and full CTest"},
            {"completionChecks", QJsonArray{"canonical-operation-receipts", "current-source-and-freshness", "canonical-context", "exact-diff-boundary"}}}},
        {"negativeConstraints", QJsonArray{"Only the named canonical producers may write the exact planned outputs.",
            "No source, original evidence, certificate history, or unrelated derived state may change.",
            "No lifecycle activation or certification issuance is authorized by this operation."}},
        {"requiredEvidence", QJsonArray{"canonical-operation-receipts", "current-source-and-freshness", "canonical-context", "exact-diff-boundary"}},
        {"preflight", QJsonObject{{"status", state(errors)}, {"errors", errors}}}, {"completionState", "NOT_STARTED"}};
    result.insert("contractId", fingerprint(result));
    return result;
}
}

QJsonObject WorkerTaskServices::prepareCertificationOperation(const ProjectModel& model, const QJsonObject& intent,
                                                             const QJsonObject& verifiedContract)
{
    return deriveCertificationOperation(model, intent, verifiedContract, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject WorkerTaskServices::applyCertificationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    const auto blocked = [](const QString& error) { return QJsonObject{{"status", "BLOCKED"}, {"error", error}}; };
    if (contract.value("contractKind") != operationKind || !contract.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("An exact READY canonical operation contract is required");
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Canonical certification operation is busy");
    const auto currentPlan = deriveCertificationOperation(model, contract.value("intent").toObject(),
        contract.value("verificationContract").toObject(), contract.value("operationId").toString());
    if (currentPlan != contract || !currentPlan.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("The READY plan, software proof, source, identity or exact baseline is no longer current");
    QJsonArray errors;
    const auto shape = operationShape(model, contract.value("intent").toObject(), contract.value("operationId").toString(), errors);
    const QString proofPath = shape.value("proof").toString();
    const QByteArray proofBytes = QJsonDocument(QJsonObject{{"producer", "WorkerTaskServices/certification-operation/v1"},
        {"operationContract", contract}}).toJson(QJsonDocument::Compact) + '\n';
    QDir().mkpath(QFileInfo(pathFor(model, proofPath)).absolutePath());
    if (!writeNew(pathFor(model, proofPath), proofBytes)) {
        ContextCoordinationService::generate(model);
        return blocked("Immutable operation software proof could not be created and read back; partial bytes retained");
    }
    const QString proofHash = digest(proofBytes);
    QString failure;
    QJsonArray results;
    const auto unchanged = [&]() {
        return contract.value("sourceBindings").toObject() == operationSources(model, errors)
            && contract.value("sourceRevision") == operationRevision(model)
            && contract.value("binding").toObject() == binding(model)
            && state(errors).startsWith("READY");
    };
    const bool revalidation = contract.value("intent").toObject().value("kind") == "revalidate-current/v1";
    if (revalidation) {
        for (const auto& subject : strings(shape.value("subjects"))) {
            if (!unchanged()) { failure = "Source or canonical authority changed during the operation"; break; }
            const auto binding = operationRecordBinding(contract, shape, proofHash, subject);
            const QJsonObject regression{{"status", "PASS"}, {"semanticChange", contract.value("intent").toObject().value("semanticChanges").toObject().value(subject)},
                {"reason", contract.value("intent").toObject().value("reason")}, {"softwareEvidence", binding.value("softwareEvidence")},
                {"verifiedTaskContractId", contract.value("softwareProof").toObject().value("postflight").toObject().value("contractId")}};
            CertificationRevalidationResult result;
            if (!CertificationRevalidationService::revalidate(root(model), subject, regression, &result, &failure,
                    shape.value("ids").toObject().value(subject).toString(), binding)) break;
            results.append(result.toJson());
            if (!unchanged() || CertificationFreshnessService::evaluate(subject, root(model)).status != CertificationFreshnessStatus::Fresh) {
                failure = "Applied revalidation failed current source/freshness readback"; break;
            }
        }
    } else {
        QJsonObject result;
        if (!unchanged()) failure = "Source or canonical authority changed before integration acceptance";
        else if (FoundationCertificationCampaign::acceptIntegration(root(model), model.projectFilePath(), &result, &failure,
            contract.value("operationId").toString(), operationRecordBinding(contract, shape, proofHash))) {
            results.append(result);
            // The full external gate regenerates F1 cold-start validation. It is
            // an explicitly scoped apply operation, never a postflight read.
            if (!FoundationCertificationCampaign::eligible(root(model), model.projectFilePath(), &failure)
                && failure.isEmpty()) failure = "Current full P6 eligibility failed";
        }
    }
    QJsonObject hashes;
    const auto outputs = shape.value("outputs").toObject();
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (it.key() != shape.value("receipt").toString() && it.key() != shape.value("seal").toString() && !operationContextPaths().contains(it.key()))
            hashes.insert(it.key(), fileHash(pathFor(model, it.key())));
    const QJsonObject receipt{{"producer", "WorkerTaskServices/certification-operation/v1"}, {"operationId", contract.value("operationId")},
        {"p6Eligibility", revalidation ? "NOT_APPLICABLE" : failure.isEmpty() ? "PASS" : "FAIL"},
        {"contractId", contract.value("contractId")}, {"outcome", failure.isEmpty() ? "CANONICAL_OPERATIONS_APPLIED" : "FAILED"},
        {"error", failure}, {"outputs", hashes}, {"results", results}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!writeNew(pathFor(model, shape.value("receipt").toString()), QJsonDocument(receipt).toJson(QJsonDocument::Indented))) {
        ContextCoordinationService::generate(model);
        return blocked("Partial operation retained; immutable receipt could not be written");
    }
    const QJsonObject seal{{"producer", "WorkerTaskServices/certification-operation/v1"}, {"contractId", contract.value("contractId")},
        {"operationId", contract.value("operationId")}, {"receiptFingerprint", fileHash(pathFor(model, shape.value("receipt").toString()))}};
    if (!writeNew(pathFor(model, shape.value("seal").toString()), QJsonDocument(seal).toJson())) {
        ContextCoordinationService::generate(model);
        return blocked("Partial operation retained; immutable receipt seal could not be written");
    }
    // Receipt precedes context generation because it belongs to the Worker
    // resource manifest. Context cannot exclude its own authoritative inputs.
    const auto context = ContextCoordinationService::generate(model);
    if (!context.value("success").toBool()) return blocked("Operation retained; canonical context refresh failed");
    const auto report = postflightCertificationOperation(model, contract);
    return {{"status", report.value("completionState") == "VERIFIED" ? "PASS" : "FAIL"}, {"postflight", report},
        {"receipt", shape.value("receipt")}, {"error", failure}};
}

QJsonObject WorkerTaskServices::recoverCertificationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    const auto blocked = [](const QString& message) { return QJsonObject{{"status", "BLOCKED"}, {"error", message}}; };
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Canonical certification operation is busy");
    auto original = contract; original.remove("contractId");
    QJsonArray errors;
    const auto shape = operationShape(model, contract.value("intent").toObject(), contract.value("operationId").toString(), errors);
    const auto outputs = shape.value("outputs").toObject();
    const QString directory = QFileInfo(shape.value("receipt").toString()).path();
    const QString recoveryPath = directory + "/recovery.json", recoverySealPath = directory + "/recovery-seal.json";
    if (contract.value("contractKind") != operationKind || fingerprint(original) != contract.value("contractId")
        || !contract.value("preflight").toObject().value("status").toString().startsWith("READY")
        || contract.value("permittedFiles") != array(outputs.keys()) || contract.value("binding").toObject() != binding(model)
        || contract.value("lifecycle") != processVersionStateToJson(model.processVersionState())
        || contract.value("sourceBindings").toObject() != operationSources(model, errors)
        || readObject(pathFor(model, shape.value("proof").toString())).value("operationContract") != contract
        || QFileInfo::exists(pathFor(model, recoverySealPath)))
        return blocked("Recovery requires the unchanged original READY operation and its retained proof; an existing terminal seal cannot be replaced");
    const auto receipt = readObject(pathFor(model, shape.value("receipt").toString()));
    const auto seal = readObject(pathFor(model, shape.value("seal").toString()));
    if (receipt.value("outcome") == "CANONICAL_OPERATIONS_APPLIED"
        && seal.value("receiptFingerprint") == fileHash(pathFor(model, shape.value("receipt").toString())))
        return blocked("A sealed completed operation cannot be reclassified as an interrupted failure");
    const auto before = operationSnapshot(model, errors);
    operationBoundary(model, contract.value("baseline").toObject(), before, outputs.keys(), errors);
    if (!state(errors).startsWith("READY")) return blocked("Recovery refuses changes outside the original exact producer scope");
    QJsonObject hashes;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (it.key() != recoveryPath && it.key() != recoverySealPath && !operationContextPaths().contains(it.key()))
            hashes.insert(it.key(), fileHash(pathFor(model, it.key())));
    const QJsonObject failure{{"producer", "WorkerTaskServices/certification-operation/v1"}, {"operationId", contract.value("operationId")},
        {"contractId", contract.value("contractId")}, {"outcome", "FAILED"},
        {"error", "Recovered interrupted canonical operation; retained partial outputs are not successful evidence"},
        {"outputs", hashes}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    const auto atomicNew = [&](const QString& relative, const QByteArray& bytes) {
        const QString path = pathFor(model, relative);
        if (QFileInfo::exists(path)) return false;
        QTemporaryFile staged(QFileInfo(path).absolutePath() + "/.recovery-XXXXXX");
        if (!staged.open() || staged.write(bytes) != bytes.size() || !staged.flush()) return false;
        staged.close();
        if (!staged.rename(path)) return false;
        staged.setAutoRemove(false);
        return fileHash(path) == digest(bytes);
    };
    if (QFileInfo::exists(pathFor(model, recoveryPath))) {
        auto retained = readObject(pathFor(model, recoveryPath)), expected = failure;
        retained.remove("timestamp"); expected.remove("timestamp");
        if (retained != expected) return blocked("An existing unsealed recovery record does not match exact retained inputs");
    } else if (!atomicNew(recoveryPath, QJsonDocument(failure).toJson()))
        return blocked("Recovery failure record could not be atomically created; retained bytes remain untouched");
    const QJsonObject terminalSeal{{"producer", "WorkerTaskServices/certification-operation/v1"}, {"operationId", contract.value("operationId")},
        {"contractId", contract.value("contractId")}, {"receiptFingerprint", fileHash(pathFor(model, recoveryPath))}};
    if (!atomicNew(recoverySealPath, QJsonDocument(terminalSeal).toJson()))
        return blocked("Recovery record retained; terminal seal could not be created");
    const auto context = ContextCoordinationService::generate(model);
    return {{"status", context.value("success").toBool() ? "RECOVERED_FAILURE" : "BLOCKED"}, {"completionState", "BLOCKED"},
        {"recovery", recoveryPath}, {"error", failure.value("error")}, {"context", context}};
}

QJsonObject WorkerTaskServices::postflightCertificationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    QJsonArray errors;
    auto unsignedContract = contract; unsignedContract.remove("contractId");
    if (contract.value("contractKind") != operationKind || contract.value("contractId") != fingerprint(unsignedContract)
        || !contract.value("preflight").toObject().value("status").toString().startsWith("READY"))
        issue(errors, "OPERATION_CONTRACT_INVALID", "Original READY operation contract is invalid or tampered.");
    const auto shape = operationShape(model, contract.value("intent").toObject(), contract.value("operationId").toString(), errors);
    const auto outputs = shape.value("outputs").toObject();
    if (contract.value("permittedFiles") != array(outputs.keys()) || contract.value("binding").toObject() != binding(model)
        || contract.value("lifecycle") != processVersionStateToJson(model.processVersionState()))
        issue(errors, "OPERATION_AUTHORITY_INVALID", "Operation permissions, project authority or lifecycle differ from the prepared contract.");
    if (contract.value("sourceBindings").toObject() != operationSources(model, errors))
        issue(errors, "OPERATION_SOURCE_STALE", "Current source or contract bytes differ from the measured operation proof.");
    const auto proof = readObject(pathFor(model, shape.value("proof").toString()));
    const auto receipt = readObject(pathFor(model, shape.value("receipt").toString()));
    const auto seal = readObject(pathFor(model, shape.value("seal").toString()));
    if (QFileInfo::exists(pathFor(model, QFileInfo(shape.value("receipt").toString()).path() + "/recovery.json")))
        issue(errors, "OPERATION_FAILED_RECOVERED", "A terminal recovery preserves a failed attempt; it is never successful operation evidence.");
    if (proof.value("producer") != "WorkerTaskServices/certification-operation/v1" || proof.value("operationContract") != contract
        || receipt.value("producer") != "WorkerTaskServices/certification-operation/v1"
        || receipt.value("contractId") != contract.value("contractId") || receipt.value("operationId") != contract.value("operationId")
        || receipt.value("outcome") != "CANONICAL_OPERATIONS_APPLIED"
        || seal.value("producer") != "WorkerTaskServices/certification-operation/v1"
        || seal.value("contractId") != contract.value("contractId") || seal.value("operationId") != contract.value("operationId")
        || seal.value("receiptFingerprint") != fileHash(pathFor(model, shape.value("receipt").toString())))
        issue(errors, "OPERATION_RECEIPT_INVALID", "Exact immutable operation proof and successful canonical receipts are required.");
    QJsonObject expectedHashes;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (it.key() != shape.value("receipt").toString() && it.key() != shape.value("seal").toString() && !operationContextPaths().contains(it.key()))
            expectedHashes.insert(it.key(), fileHash(pathFor(model, it.key())));
    if (receipt.value("outputs") != expectedHashes)
        issue(errors, "OPERATION_RECEIPT_INVALID", "A canonical output changed or the exact receipt output set differs.");
    validateFrozenSoftwareProof(model, contract.value("softwareProof").toObject(), errors);
    const QString proofHash = fileHash(pathFor(model, shape.value("proof").toString()));
    const bool revalidation = contract.value("intent").toObject().value("kind") == "revalidate-current/v1";
    if (revalidation) {
        auto expectedState = contract.value("freshnessBase").toObject();
        auto expectedSubjects = expectedState.value("subjects").toObject();
        for (const auto& subject : strings(shape.value("subjects"))) {
            const QString prefix = "ARAMF_WORKER/certification/revalidation/" + subject + '/' + shape.value("ids").toObject().value(subject).toString();
            const auto record = readObject(pathFor(model, prefix + "/revalidation.json"));
            const auto evidence = readObject(pathFor(model, prefix + "/evidence.json"));
            const auto expected = operationRecordBinding(contract, shape, proofHash, subject);
            QJsonObject latest; QString latestError;
            const bool isLatest = CertificationRevalidationService::latest(root(model), subject, &latest, &latestError)
                && latestError.isEmpty() && latest.value("revalidationId") == shape.value("ids").toObject().value(subject);
            if (record.value("operationBinding") != expected || evidence.value("operationBinding") != expected
                || !isLatest
                || record.value("subject") != subject || record.value("revalidationId") != shape.value("ids").toObject().value(subject)
                || record.value("evidenceArtifact") != prefix + "/evidence.json"
                || record.value("evidenceFingerprint") != fileHash(pathFor(model, prefix + "/evidence.json"))
                || record.value("semanticChange") != contract.value("intent").toObject().value("semanticChanges").toObject().value(subject)
                || CertificationFreshnessService::evaluate(subject, root(model)).status != CertificationFreshnessStatus::Fresh)
                issue(errors, "OPERATION_REVALIDATION_INVALID", "Current exact revalidation receipt, source, evidence or semantics failed.", subject);
            expectedSubjects.insert(subject, record);
        }
        expectedState.insert("schemaVersion", 1); expectedState.insert("subjects", expectedSubjects);
        if (readObject(pathFor(model, "ARAMF_WORKER/certification/current-freshness-state.json")) != expectedState)
            issue(errors, "OPERATION_STATE_INVALID", "Canonical freshness state must merge only the exact planned subject receipts.");
    } else {
        const QString path = "ARAMF_WORKER/certification/integration/" + contract.value("operationId").toString() + "/acceptance.json";
        const auto acceptance = readObject(pathFor(model, path));
        QString error;
        bool exactAcceptance = acceptance.value("acceptanceType") == "AUTHORITATIVE_ACCEPTANCE" && acceptance.value("result") == "PASS";
        for (const auto& subject : prerequisiteSubjects) {
            const auto current = CertificationFreshnessService::evaluate(subject, root(model));
            const auto accepted = acceptance.value("freshness").toObject().value(subject).toObject();
            exactAcceptance &= current.status == CertificationFreshnessStatus::Fresh
                && acceptance.value("certificates").toObject().value(subject) == current.historicalCertificateId
                && accepted.value("currentSourceFingerprint") == current.currentSourceFingerprint
                && accepted.value("currentContractFingerprint") == current.currentContractFingerprint
                && accepted.value("currentEvidenceFingerprint") == current.currentEvidenceFingerprint;
        }
        if (acceptance.value("operationBinding") != operationRecordBinding(contract, shape, proofHash)
            || acceptance.value("projectBinding") != contract.value("projectBinding")
            || !exactAcceptance || receipt.value("p6Eligibility") != "PASS"
            || !model.processVersionState().isP6Eligible(&error))
            issue(errors, "OPERATION_INTEGRATION_INVALID", "Exact current acceptance and sealed full-eligibility observation failed: " + error);
    }
    validateCanonical(model, errors);
    const auto current = operationSnapshot(model, errors);
    const QJsonObject expectedContext{
        {"ARAMF_WORKER/context/context-index.json", ContextCoordinationService::buildIndex(model)},
        {"ARAMF_WORKER/context/compressed-context.json", ContextCoordinationService::compress(model)},
        {"ARAMF_WORKER/context/freshness.json", ContextCoordinationService::freshness(model)},
        {"ARAMF_WORKER/context/agent-adapters.json", ContextCoordinationService::adapterDescriptors()}};
    for (auto it = expectedContext.begin(); it != expectedContext.end(); ++it) {
        auto actual = readObject(pathFor(model, it.key())); actual.remove("_file");
        auto expected = it.value().toObject(); expected.remove("_file");
        if (actual != expected)
            issue(errors, "OPERATION_CONTEXT_INVALID", "Exact context output differs from its canonical producer.", it.key());
    }
    operationBoundary(model, contract.value("baseline").toObject(), current, outputs.keys(), errors);
    const bool valid = state(errors).startsWith("READY");
    return {{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractId", contract.value("contractId")},
        {"status", valid ? "PASS" : "FAIL"}, {"completionState", valid ? "VERIFIED" : "BLOCKED"}, {"errors", errors},
        {"missingEvidence", valid ? QJsonArray{} : QJsonArray{"canonical-operation-receipts"}},
        {"permittedFiles", array(outputs.keys())}, {"sourceBindings", contract.value("sourceBindings")}, {"receipt", shape.value("receipt")}};
}


namespace {
const QString activationKind = "canonical-p6-activation-operation/v1";
const QString activationProducer = "WorkerTaskServices/p6-activation-operation/v1";
bool readActivationModel(const QString& path, ProjectModel* model, QString* error)
{
    // load() may persist a legacy migration. Activation inspection must never
    // migrate or save, including when the caller's saved input is malformed.
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(rawBytes(path), &parseError);
    auto object = document.object(); const auto version = object.value("schemaVersion");
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || !version.isDouble()
        || version.toDouble() != ProjectPersistence::currentSchemaVersion()) {
        if (error) *error = "Activation requires an existing current-schema saved model; migration is a separate operation";
        return false;
    }
    object.insert("projectFilePath", path);
    return ProjectPersistence().fromJson(model, object, error);
}
QDateTime activationTime(const QJsonValue& value)
{
    const auto text = value.toString();
    const auto time = QDateTime::fromString(text, Qt::ISODateWithMs);
    return text.endsWith('Z') && time.isValid() && time.toUTC().toString(Qt::ISODateWithMs) == text ? time : QDateTime{};
}
QJsonObject activationRequirements(const ProjectModel& model, QJsonArray& errors)
{
    const auto baseline = model.ruleConfiguration().scopeMetadata.value("source-code").toObject().value("p6CodeBankRequirements").toObject();
    auto criteria = baseline; criteria.remove("provenance");
    const auto provenance = baseline.value("provenance").toObject();
    if (!model.ruleConfiguration().projectScopes.contains("source-code") || criteria != WorkerTaskServices::p6RequirementsBaseline()
        || provenance.value("actor") != "agent" || provenance.value("agentId").toString().isEmpty()
        || provenance.value("tool") != requirementsProducer)
        issue(errors, "ACTIVATION_REQUIREMENTS_INVALID", "Complete canonically registered P6 requirements and honest agent provenance are required.");
    return baseline;
}
QJsonObject activationPrerequisites(const ProjectModel& model, QJsonArray& errors)
{
    QJsonObject result;
    for (const auto& subject : prerequisiteSubjects) {
        const auto current = CertificationFreshnessService::evaluate(subject, root(model));
        if (current.status != CertificationFreshnessStatus::Fresh)
            issue(errors, "ACTIVATION_PREREQUISITE_STALE", "Each current prerequisite must independently evaluate FRESH.", subject);
        result.insert(subject, QJsonObject{{"certificateId", current.historicalCertificateId},
            {"sourceFingerprint", current.currentSourceFingerprint}, {"contractFingerprint", current.currentContractFingerprint},
            {"evidenceFingerprint", current.currentEvidenceFingerprint}, {"dependencyManifestFingerprint", current.dependencyManifestFingerprint}});
    }
    return result;
}
bool activationProjection(const ProjectModel& model, ProjectModel* proposed, QJsonArray& errors)
{
    const auto& lifecycle = model.processVersionState(); QString error;
    if (worker(model) != "ARAMF_WORKER" || lifecycle.namespaceVersion != 2 || lifecycle.hasActiveProcess || lifecycle.hasActiveStructure
        || lifecycle.nextIdentifier() != "P6.1.0.0.0" || !lifecycle.isValid(&error) || !lifecycle.isP6Eligible(&error))
        issue(errors, "ACTIVATION_LIFECYCLE_INVALID", "Exact idle canonical P6 entry and explicit F4 eligibility are required: " + error);
    const auto original = ProjectPersistence().toJson(model);
    if (!ProjectPersistence().fromJson(proposed, original, &error)) issue(errors, "ACTIVATION_MODEL_INVALID", error);
    proposed->setProjectPath(model.projectPath()); proposed->setProjectFilePath(model.projectFilePath());
    if (!state(errors).startsWith("READY")) return false;
    if (!proposed->startNextProcess(&error)) issue(errors, "ACTIVATION_PROJECTION_INVALID", error);
    auto before = original, after = ProjectPersistence().toJson(*proposed);
    before.remove("processVersion"); after.remove("processVersion");
    if (before != after || proposed->processVersionState().activeIdentifier() != "P6.1.1.0.0"
        || proposed->processVersionState().nextIdentifier() != "P7.1.0.0.0")
        issue(errors, "ACTIVATION_PROJECTION_INVALID", "Only the canonical initial P6 start projection is permitted.");
    return state(errors).startsWith("READY");
}
QJsonObject activationShape(const ProjectModel& model, const QJsonObject& intent, const QString& id, QJsonArray& errors)
{
    const QSet<QString> allowed{"kind", "agentId", "reason", "validitySeconds"};
    for (auto it = intent.begin(); it != intent.end(); ++it)
        if (!allowed.contains(it.key())) issue(errors, "ACTIVATION_INTENT_INVALID", "Caller paths, permissions, timestamps, results and authority are forbidden.", it.key());
    const auto duration = intent.value("validitySeconds");
    if (intent.value("kind") != "activate-p6/v1" || !QRegularExpression("^[A-Za-z0-9_./-]{1,128}$").match(intent.value("agentId").toString()).hasMatch()
        || QStringList{"none", "human"}.contains(intent.value("agentId").toString()) || intent.value("reason").toString().trimmed().isEmpty()
        || intent.value("reason").toString().size() > 1024
        || (!duration.isUndefined() && (!duration.isDouble() || duration.toDouble() != duration.toInt() || duration.toInt() < 1 || duration.toInt() > 3600))
        || !QRegularExpression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(id).hasMatch())
        issue(errors, "ACTIVATION_INTENT_INVALID", "A fixed activation intent, actual agent, reason and bounded integer lifetime are required.");
    const auto original = ProjectPersistence().toJson(model);
    const auto academic = original.value("academic").toObject();
    if (academic.value("enabled").toBool() || academic.value("thesisDocumentation").toObject().value("enabled").toBool()
        || academic.value("reportDocumentation").toObject().value("enabled").toBool()
        || original.value("certification").toObject().value("enabled").toBool())
        issue(errors, "ACTIVATION_OUTPUT_INVALID", "This narrow start requires disabled academic and certification generation.");
    const QString directory = "ARAMF_WORKER/verification/activation/" + id;
    QJsonObject outputs;
    for (const auto& path : ConfigurationUpdateService::effectiveOutputFiles(model)) {
        if (path.endsWith("/Thesis") || path.endsWith("/Report") || QFileInfo(pathFor(model, path)).isDir())
            issue(errors, "ACTIVATION_OUTPUT_INVALID", "Only finite exact regular producer files are permitted.", path);
        outputs.insert(path, "ConfigurationUpdateService");
    }
    const QString saved = QDir(root(model)).relativeFilePath(model.projectFilePath());
    if (!saved.endsWith(".aramf.json") || !QFileInfo(pathFor(model, saved)).isFile())
        issue(errors, "ACTIVATION_MODEL_INVALID", "An existing contained saved project is required.");
    outputs.insert(saved, "ProjectPersistence");
    for (const QString& path : {QStringLiteral("memory/event-log.jsonl"), QStringLiteral("memory/memory-manifest.json"),
        QStringLiteral("memory/current-state.md"), QStringLiteral("memory/metrics.json"), QStringLiteral("memory/memory-consistency-validation.json"),
        QStringLiteral("memory/cold-start-validation.json"), QStringLiteral("PROJECT_STATUS.md")}) outputs.insert(workerPath(model, path), "ProjectMemory");
    for (const auto& path : operationContextPaths()) outputs.insert(path, "ContextCoordinationService");
    for (const auto& path : requirementsVerificationPaths()) outputs.insert(path, "VerificationServices");
    for (const QString& name : {QStringLiteral("proof.json"), QStringLiteral("receipt.json"), QStringLiteral("seal.json")}) outputs.insert(directory + '/' + name, "WorkerTaskServices");
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (!operationSafePath(model, it.key())) issue(errors, "ACTIVATION_OUTPUT_INVALID", "Unsafe canonical operation output.", it.key());
    return {{"outputs", outputs}, {"proof", directory + "/proof.json"}, {"receipt", directory + "/receipt.json"}, {"seal", directory + "/seal.json"}};
}
QJsonArray activationPermissions(const QJsonObject& outputs)
{
    QJsonArray permissions;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        permissions.append(QJsonObject{{"path", it.key()}, {"owner", it.value()}, {"actions", QJsonArray{"READ",
            it.key().startsWith("ARAMF_WORKER/verification/activation/") ? "GENERATE_ONCE" : "UPDATE_DERIVED"}}});
    return permissions;
}
QJsonObject activationAcceptance(const ProjectModel& model, const QJsonObject& integration, const QJsonObject& prerequisites, QJsonArray& errors)
{
    const QString reference = "ARAMF_WORKER/certification/integration/" + integration.value("operationId").toString() + "/acceptance.json";
    const auto record = readObject(pathFor(model, reference));
    const QJsonObject identity{{"projectId", model.projectId()}, {"projectRoot", QFileInfo(root(model)).canonicalFilePath()}, {"workerIdentity", worker(model)}};
    if (!operationSafePath(model, reference) || record.value("acceptanceType") != "AUTHORITATIVE_ACCEPTANCE"
        || record.value("result") != "PASS" || record.value("projectBinding") != identity)
        issue(errors, "ACTIVATION_ACCEPTANCE_INVALID", "Current authoritative integration must bind this exact project.");
    for (const auto& subject : prerequisiteSubjects) {
        const auto current = prerequisites.value(subject).toObject(); const auto accepted = record.value("freshness").toObject().value(subject).toObject();
        if (record.value("certificates").toObject().value(subject) != current.value("certificateId")
            || accepted.value("currentSourceFingerprint") != current.value("sourceFingerprint")
            || accepted.value("currentContractFingerprint") != current.value("contractFingerprint")
            || accepted.value("currentEvidenceFingerprint") != current.value("evidenceFingerprint"))
            issue(errors, "ACTIVATION_ACCEPTANCE_INVALID", "Integration does not bind current prerequisite identities and fingerprints.", subject);
    }
    QJsonArray shapeErrors; const auto shape = operationShape(model, integration.value("intent").toObject(), integration.value("operationId").toString(), shapeErrors);
    if (!shapeErrors.isEmpty() || record.value("operationBinding") != operationRecordBinding(integration, shape, fileHash(pathFor(model, shape.value("proof").toString()))))
        issue(errors, "ACTIVATION_ACCEPTANCE_INVALID", "Integration operation binding differs from its original sealed proof.");
    return {{"reference", reference}, {"fingerprint", fileHash(pathFor(model, reference))}, {"record", record}};
}
QJsonObject deriveActivationOperation(const ProjectModel& model, const QJsonObject& intent, const QJsonObject& supplied, const QString& id)
{
    QJsonArray errors; const auto shape = activationShape(model, intent, id, errors); const auto outputs = shape.value("outputs").toObject();
    validateCanonical(model, errors);
    const auto original = ProjectPersistence().toJson(model); ProjectModel saved, proposed; QString error;
    if (!readActivationModel(model.projectFilePath(), &saved, &error) || ProjectPersistence().toJson(saved) != original)
        issue(errors, "ACTIVATION_MODEL_INVALID", "The supplied model must match the saved canonical model.");
    activationProjection(model, &proposed, errors);
    const auto requirements = activationRequirements(model, errors);
    for (const QString& permission : {QStringLiteral("read-project-files"), QStringLiteral("modify-files"), QStringLiteral("create-files")})
        if (!model.aiConfiguration().permissions.contains(permission)) issue(errors, "ACTIVATION_PERMISSION_MISSING", "Canonical permission is absent.", permission);
    if (!model.memoryConfiguration().maintenanceOptions.contains("record-task-completion")
        || !model.memoryConfiguration().maintenanceOptions.contains("update-current-state")
        || !QFileInfo::exists(pathFor(model, "ARAMF_WORKER/context/task-dag.json")))
        issue(errors, "ACTIVATION_AUDIT_INVALID", "Existing task DAG, task recording and canonical current-state maintenance must already be available.");
    for (const QString& key : {QStringLiteral("proof"), QStringLiteral("receipt"), QStringLiteral("seal")})
        if (QFileInfo::exists(pathFor(model, shape.value(key).toString()))) issue(errors, "ACTIVATION_REPLAY", "A reserved immutable output already exists.");
    const auto currentPreview = ConfigurationUpdateService::preview(model), preview = ConfigurationUpdateService::preview(proposed);
    if (!currentPreview.success || !currentPreview.noChange || !preview.success || preview.noChange || preview.removalBlocked)
        issue(errors, "ACTIVATION_PREVIEW_INVALID", "Only the current canonical start delta may be synchronized: " + preview.error);
    const auto baseline = operationSnapshot(model, errors), sources = operationSources(model, errors);
    const auto prerequisites = activationPrerequisites(model, errors);
    const auto integration = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    QJsonObject integrationReport, lineage; QJsonArray shapeErrors;
    const auto integrationShape = operationShape(model, integration.value("intent").toObject(), integration.value("operationId").toString(), shapeErrors);
    if (integration.value("contractKind") != operationKind || integration.value("intent").toObject().value("kind") != "accept-current-integration/v1" || !shapeErrors.isEmpty())
        issue(errors, "ACTIVATION_INTEGRATION_INVALID", "The current completed typed integration operation is required.");
    else if (state(errors).startsWith("READY")) {
        integrationReport = WorkerTaskServices::postflightCertificationOperation(model, integration);
        if (integrationReport.value("status") != "PASS" || integrationReport.value("completionState") != "VERIFIED")
            issue(errors, "ACTIVATION_INTEGRATION_INVALID", "Current exact integration postflight must be VERIFIED.");
    }
    const auto software = integration.value("softwareProof").toObject();
    if (!QStringList{"task-p6-activation-governance/v1", "task-p6-activation-campaign-isolation/v1", "task-p6-activation-campaign-isolation/v2"}.contains(
            measuredProfile(software.value("softwareContract").toObject()).value("id").toString()))
        issue(errors, "ACTIVATION_SOFTWARE_INVALID", "Current measured activation governance acceptance is required.");
    const auto acceptance = activationAcceptance(model, integration, prerequisites, errors);
    for (const QString& key : {QStringLiteral("proof"), QStringLiteral("receipt"), QStringLiteral("seal")}) {
        const QString path = integrationShape.value(key).toString();
        if (path.isEmpty() || !operationSafePath(model, path)) issue(errors, "ACTIVATION_INTEGRATION_INVALID", "Unsafe integration receipt path.");
        else lineage.insert(path, fileHash(pathFor(model, path)));
    }
    const auto receipt = readObject(pathFor(model, integrationShape.value("receipt").toString()));
    const auto anchor = activationTime(receipt.value("timestamp"));
    const int duration = intent.value("validitySeconds").toInt(3600); const auto expires = anchor.addSecs(duration);
    const auto now = QDateTime::currentDateTimeUtc();
    if (!anchor.isValid() || anchor > now || !expires.isValid() || expires <= now)
        issue(errors, "ACTIVATION_EXPIRED", "A valid nonfuture sealed integration receipt and unexpired bounded start window are required.");
    if (sources != operationSources(model, errors) || baseline != operationSnapshot(model, errors))
        issue(errors, "ACTIVATION_BASE_CHANGED", "Current sources or baseline changed during preparation.");
    QJsonObject result{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractKind", activationKind}, {"operationId", id}, {"intent", intent},
        {"binding", binding(model)}, {"projectBinding", QJsonObject{{"projectId", model.projectId()}, {"projectRoot", root(model)}, {"workerIdentity", worker(model)}}},
        {"beforeModel", original}, {"proposedModel", ProjectPersistence().toJson(proposed)}, {"preview", preview.plan},
        {"sourceRevision", operationRevision(model)}, {"sourceBindings", sources}, {"prerequisiteBindings", prerequisites}, {"baseline", baseline},
        {"requirements", requirements}, {"verificationContract", integration}, {"integrationPostflight", integrationReport}, {"integrationArtifacts", lineage},
        {"softwareProof", software}, {"acceptance", acceptance}, {"integrationReceiptTimestamp", receipt.value("timestamp")},
        {"expiresAt", expires.toUTC().toString(Qt::ISODateWithMs)}, {"historyPrefix", QString::fromLatin1(rawBytes(pathFor(model, "ARAMF_WORKER/memory/event-log.jsonl")).toBase64())},
        {"permittedFiles", array(outputs.keys())}, {"filePermissions", activationPermissions(outputs)},
        {"requiredEvidence", QJsonArray{"full-foundation-eligibility", "explicit-f4-p6-start", "canonical-start-save-reload", "sealed-activation-receipts", "exact-scope-postflight"}},
        {"preflight", QJsonObject{{"status", state(errors)}, {"errors", errors}}}, {"completionState", "NOT_STARTED"}};
    result.insert("contractId", fingerprint(result)); return result;
}
QJsonObject activationOutputHashes(const ProjectModel& model, const QJsonObject& shape)
{
    QJsonObject hashes; const auto outputs = shape.value("outputs").toObject();
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (it.key() != shape.value("receipt").toString() && it.key() != shape.value("seal").toString()
            && !operationContextPaths().contains(it.key()) && !requirementsVerificationPaths().contains(it.key()))
            hashes.insert(it.key(), fileHash(pathFor(model, it.key())));
    return hashes;
}
}

QJsonObject WorkerTaskServices::prepareActivationOperation(const ProjectModel& model, const QJsonObject& intent, const QJsonObject& integration)
{
    return deriveActivationOperation(model, intent, integration, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject WorkerTaskServices::applyActivationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    const auto blocked = [](const QString& error) { return QJsonObject{{"status", "BLOCKED"}, {"error", error}}; };
    if (contract.value("contractKind") != activationKind || !contract.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("An exact READY activation operation is required");
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0); if (!lock.tryLock(0)) return blocked("Canonical project operation is busy");
    const auto current = deriveActivationOperation(model, contract.value("intent").toObject(), contract.value("verificationContract").toObject(), contract.value("operationId").toString());
    if (current != contract || !current.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("Exact activation plan, expiry, proof, prerequisite or baseline is no longer current");
    QJsonArray errors; const auto shape = activationShape(model, contract.value("intent").toObject(), contract.value("operationId").toString(), errors);
    if (QDateTime::currentDateTimeUtc() >= activationTime(contract.value("expiresAt"))) return blocked("Activation expired before its first write");
    QDir().mkpath(QFileInfo(pathFor(model, shape.value("proof").toString())).absolutePath());
    if (!writeNew(pathFor(model, shape.value("proof").toString()), QJsonDocument(QJsonObject{{"producer", activationProducer}, {"operationContract", contract}}).toJson()))
        return blocked("Immutable activation proof could not be created; any partial bytes remain retained");
    QString failure, gateError, startedAt; QJsonArray stages; QJsonObject gates, auditRecord;
    const bool foundation = FoundationCertificationCampaign::eligible(root(model), model.projectFilePath(), &gateError);
    const QJsonObject foundationGate{{"success", foundation}, {"error", gateError}}; gates.insert("foundationEligibility", foundationGate);
    stages.append(QJsonObject{{"stage", "foundation-eligibility"}, {"success", foundation}, {"error", gateError}});
    if (!foundation) failure = "Full Foundation eligibility failed: " + gateError;
    ProjectModel active; QString error;
    if (failure.isEmpty() && (!readActivationModel(model.projectFilePath(), &active, &error) || ProjectPersistence().toJson(active) != contract.value("beforeModel")))
        failure = "Canonical model changed before actual start: " + error;
    if (failure.isEmpty()) {
        gateError.clear(); const auto& lifecycle = active.processVersionState();
        const bool f4 = lifecycle.isP6Eligible(&gateError) && lifecycle.namespaceVersion == 2 && !lifecycle.hasActiveProcess
            && !lifecycle.hasActiveStructure && lifecycle.nextIdentifier() == "P6.1.0.0.0";
        gates.insert("f4Eligibility", QJsonObject{{"success", f4}, {"error", gateError}});
        stages.append(QJsonObject{{"stage", "f4-eligibility"}, {"success", f4}, {"error", gateError}});
        if (!f4) failure = "Explicit F4 P6 start gate failed: " + gateError;
    }
    if (failure.isEmpty()) {
        const auto sources = operationSources(active, errors), prerequisites = activationPrerequisites(active, errors);
        const auto accepted = activationAcceptance(active, contract.value("verificationContract").toObject(), prerequisites, errors);
        auto expected = contract.value("baseline").toObject();
        operationBoundary(active, expected, operationSnapshot(active, errors), {shape.value("proof").toString(), "ARAMF_WORKER/memory/cold-start-validation.json"}, errors);
        const auto now = QDateTime::currentDateTimeUtc();
        if (sources != contract.value("sourceBindings") || prerequisites != contract.value("prerequisiteBindings") || accepted != contract.value("acceptance")
            || operationRevision(active) != contract.value("sourceRevision") || !state(errors).startsWith("READY")
            || now < activationTime(contract.value("integrationReceiptTimestamp")) || now >= activationTime(contract.value("expiresAt")))
            failure = "Frozen inputs or activation window changed immediately before the actual start";
        else {
            startedAt = now.toString(Qt::ISODateWithMs);
            const bool started = active.startNextProcess(&error);
            stages.append(QJsonObject{{"stage", "start"}, {"success", started}, {"startedAt", startedAt}, {"error", error}});
            if (!started || ProjectPersistence().toJson(active) != contract.value("proposedModel")) failure = "Actual F4 start differs from the exact initial projection: " + error;
        }
    }
    if (failure.isEmpty()) {
        const auto preview = ConfigurationUpdateService::preview(active);
        if (!preview.success || preview.plan != contract.value("preview")) failure = "Actual start configuration preview changed";
        else {
            const auto update = ConfigurationUpdateService::apply(active, preview.plan.value("validationFingerprint").toString());
            stages.append(QJsonObject{{"stage", "configuration"}, {"success", update.success}, {"saved", update.configurationSaved},
                {"derivedSynchronized", update.derivedSynchronized}, {"verified", update.verified}, {"rolledBack", update.rolledBack}, {"error", update.error}});
            if (!update.success || !update.configurationSaved || !update.derivedSynchronized || !update.verified) failure = "Canonical activation save failed: " + update.error;
        }
    }
    if (failure.isEmpty()) {
        const QJsonObject provenance{{"actor", "agent"}, {"agentId", contract.value("intent").toObject().value("agentId")}, {"tool", activationProducer}};
        const QJsonObject fields{{"task", "Start P6 through canonical lifecycle"}, {"status", "PASS"}, {"scope", "project"}, {"provenance", provenance},
            {"operationId", contract.value("operationId")}, {"contractId", contract.value("contractId")}, {"startedAt", startedAt},
            {"lifecycle", processVersionStateToJson(active.processVersionState())}, {"summary", "P6 initial lifecycle started and saved after current full Foundation and explicit F4 gates; implementation and completion remain separate."}};
        error.clear(); const bool recorded = ProjectMemory().recordOperation(root(model), "task-start", fields, &auditRecord, &error);
        stages.append(QJsonObject{{"stage", "audit"}, {"success", recorded}, {"error", error}});
        if (!recorded) failure = "Started configuration retained; canonical audit failed: " + error;
    }
    ProjectModel reloaded; error.clear(); const bool loaded = readActivationModel(model.projectFilePath(), &reloaded, &error);
    if (!loaded && failure.isEmpty()) failure = "Activation result reload failed: " + error;
    const auto readback = loaded ? ProjectPersistence().toJson(reloaded) : QJsonObject{};
    const bool safeReadback = loaded && reloaded.projectId() == model.projectId() && root(reloaded) == root(model) && worker(reloaded) == worker(model)
        && (readback == contract.value("beforeModel") || readback == contract.value("proposedModel"));
    if (loaded && !safeReadback) failure = "Unexpected saved activation state retained; no producer may follow a changed project identity or unplanned model";
    const QJsonObject receipt{{"producer", activationProducer}, {"operationId", contract.value("operationId")}, {"contractId", contract.value("contractId")},
        {"outcome", failure.isEmpty() ? "P6_STARTED" : "FAILED"}, {"error", failure}, {"stages", stages}, {"gates", gates}, {"startedAt", startedAt},
        {"observedLifecycle", loaded ? processVersionStateToJson(reloaded.processVersionState()) : QJsonObject{}}, {"auditRecord", auditRecord},
        {"outputs", activationOutputHashes(model, shape)}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!writeNew(pathFor(model, shape.value("receipt").toString()), QJsonDocument(receipt).toJson())) return blocked("Activation retained; immutable receipt write failed");
    const QJsonObject seal{{"producer", activationProducer}, {"operationId", contract.value("operationId")}, {"contractId", contract.value("contractId")},
        {"receiptFingerprint", fileHash(pathFor(model, shape.value("receipt").toString()))}};
    if (!writeNew(pathFor(model, shape.value("seal").toString()), QJsonDocument(seal).toJson())) return blocked("Activation retained; immutable seal write failed");
    if (!safeReadback) return blocked("Activation retained; canonical result could not safely reload within the exact planned models");
    const auto context = ContextCoordinationService::generate(reloaded);
    const auto verified = VerificationServices().verify(reloaded, reloaded.generationOptions());
    if (!context.value("success").toBool() || verified.overallStatus != VerificationStatus::Pass) return blocked("Activation retained; final canonical synchronization failed");
    const auto report = postflightActivationOperation(reloaded, contract);
    return {{"status", failure.isEmpty() && report.value("completionState") == "VERIFIED" ? "PASS" : "FAIL"},
        {"postflight", report}, {"receipt", shape.value("receipt")}, {"error", failure}};
}

QJsonObject WorkerTaskServices::postflightActivationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    QJsonArray errors; auto unsignedContract = contract; unsignedContract.remove("contractId");
    ProjectModel before, projected, saved; QString error;
    if (!ProjectPersistence().fromJson(&before, contract.value("beforeModel").toObject(), &error)
        || before.projectId() != model.projectId() || root(before) != root(model) || worker(before) != worker(model))
        issue(errors, "ACTIVATION_CONTRACT_INVALID", "Original activation model identity is malformed or foreign.");
    before.setProjectFilePath(model.projectFilePath());
    activationProjection(before, &projected, errors);
    const auto shape = activationShape(before, contract.value("intent").toObject(), contract.value("operationId").toString(), errors);
    const auto outputs = shape.value("outputs").toObject();
    const QJsonObject identity{{"projectId", model.projectId()}, {"projectRoot", root(model)}, {"workerIdentity", worker(model)}};
    if (contract.value("contractKind") != activationKind || contract.value("contractId") != fingerprint(unsignedContract)
        || !contract.value("preflight").toObject().value("status").toString().startsWith("READY") || contract.value("projectBinding") != identity
        || contract.value("binding").toObject().value("modelFingerprint") != fingerprint(ProjectPersistence().configuration(before))
        || contract.value("binding").toObject().value("projectId") != model.projectId()
        || contract.value("binding").toObject().value("projectRoot") != root(model)
        || contract.value("binding").toObject().value("workerIdentity") != worker(model)
        || contract.value("permittedFiles") != array(outputs.keys()) || contract.value("filePermissions") != activationPermissions(outputs)
        || contract.value("proposedModel") != ProjectPersistence().toJson(projected) || contract.value("proposedModel") != ProjectPersistence().toJson(model))
        issue(errors, "ACTIVATION_CONTRACT_INVALID", "Exact initial activation authority, identity or lifecycle projection differs.");
    if (!readActivationModel(model.projectFilePath(), &saved, &error) || ProjectPersistence().toJson(saved) != ProjectPersistence().toJson(model))
        issue(errors, "ACTIVATION_MODEL_INVALID", "Current saved activation result differs from supplied model.");
    const auto prerequisites = activationPrerequisites(model, errors);
    if (contract.value("sourceRevision") != operationRevision(model) || contract.value("sourceBindings") != operationSources(model, errors)
        || contract.value("prerequisiteBindings") != prerequisites || contract.value("requirements") != activationRequirements(model, errors)
        || contract.value("acceptance") != activationAcceptance(model, contract.value("verificationContract").toObject(), prerequisites, errors))
        issue(errors, "ACTIVATION_PREREQUISITE_INVALID", "Frozen sources, complete requirements or authoritative acceptance changed.");
    const auto integration = contract.value("verificationContract").toObject();
    if (contract.value("softwareProof") != integration.value("softwareProof") || contract.value("integrationPostflight").toObject().value("status") != "PASS"
        || contract.value("integrationPostflight").toObject().value("completionState") != "VERIFIED"
        || contract.value("integrationPostflight").toObject().value("contractId") != integration.value("contractId"))
        issue(errors, "ACTIVATION_INTEGRATION_INVALID", "Original verified integration lineage differs.");
    const auto lineage = contract.value("integrationArtifacts").toObject();
    QJsonArray shapeErrors; const auto integrationShape = operationShape(before, integration.value("intent").toObject(), integration.value("operationId").toString(), shapeErrors);
    QJsonObject expectedLineage;
    for (const QString& key : {QStringLiteral("proof"), QStringLiteral("receipt"), QStringLiteral("seal")}) {
        const QString path = integrationShape.value(key).toString(); expectedLineage.insert(path, fileHash(pathFor(model, path)));
    }
    if (!shapeErrors.isEmpty() || lineage != expectedLineage) issue(errors, "ACTIVATION_INTEGRATION_INVALID", "Original integration proof or sealed receipts changed.");
    validateFrozenSoftwareProof(model, contract.value("softwareProof").toObject(), errors);
    const auto proof = readObject(pathFor(model, shape.value("proof").toString())), receipt = readObject(pathFor(model, shape.value("receipt").toString())), seal = readObject(pathFor(model, shape.value("seal").toString()));
    if (proof.value("producer") != activationProducer || proof.value("operationContract") != contract || receipt.value("producer") != activationProducer
        || receipt.value("operationId") != contract.value("operationId") || receipt.value("contractId") != contract.value("contractId")
        || receipt.value("outcome") != "P6_STARTED" || !receipt.value("error").toString().isEmpty()
        || receipt.value("observedLifecycle") != processVersionStateToJson(model.processVersionState())
        || seal.value("producer") != activationProducer || seal.value("operationId") != contract.value("operationId") || seal.value("contractId") != contract.value("contractId")
        || seal.value("receiptFingerprint") != fileHash(pathFor(model, shape.value("receipt").toString())) || receipt.value("outputs") != activationOutputHashes(model, shape))
        issue(errors, "ACTIVATION_RECEIPT_INVALID", "Exact successful immutable activation proof, receipts and output bytes are required.");
    const auto anchor = activationTime(contract.value("integrationReceiptTimestamp")), expires = activationTime(contract.value("expiresAt")), started = activationTime(receipt.value("startedAt"));
    const int duration = contract.value("intent").toObject().value("validitySeconds").toInt(3600);
    if (!anchor.isValid() || !expires.isValid() || !started.isValid() || duration < 1 || duration > 3600 || expires != anchor.addSecs(duration)
        || started < anchor || started >= expires
        || readObject(pathFor(model, integrationShape.value("receipt").toString())).value("timestamp") != contract.value("integrationReceiptTimestamp"))
        issue(errors, "ACTIVATION_TIME_INVALID", "Actual start must be inside the original sealed integration receipt window.");
    const QStringList names{"foundation-eligibility", "f4-eligibility", "start", "configuration", "audit"};
    const auto stages = receipt.value("stages").toArray(); bool actualStages = stages.size() == names.size();
    for (int i = 0; i < stages.size() && i < names.size(); ++i) actualStages &= stages.at(i).toObject().value("stage") == names.at(i) && stages.at(i).toObject().value("success").toBool();
    const auto gates = receipt.value("gates").toObject();
    if (!actualStages || !gates.value("foundationEligibility").toObject().value("success").toBool() || !gates.value("f4Eligibility").toObject().value("success").toBool()
        || stages.size() < 4 || stages.at(2).toObject().value("startedAt") != receipt.value("startedAt") || !stages.at(3).toObject().value("verified").toBool()
        || !stages.at(3).toObject().value("saved").toBool() || !stages.at(3).toObject().value("derivedSynchronized").toBool()
        || !model.processVersionState().isP6Eligible(&error)) issue(errors, "ACTIVATION_GATES_INVALID", "Both actual gates, canonical start and persisted configuration stages must pass.");
    const auto prefix = QByteArray::fromBase64(contract.value("historyPrefix").toString().toLatin1());
    const auto events = rawBytes(pathFor(model, "ARAMF_WORKER/memory/event-log.jsonl")); QJsonObject event; int appended = 0;
    for (const auto& line : events.mid(prefix.size()).split('\n')) if (!line.trimmed().isEmpty()) { ++appended; event = QJsonDocument::fromJson(line).object(); }
    const QJsonObject provenance{{"actor", "agent"}, {"agentId", contract.value("intent").toObject().value("agentId")}, {"tool", activationProducer}};
    int matching = 0; QString eventsError;
    for (const auto& item : ProjectMemory().events(root(model), &eventsError)) if (item.value("operationId") == contract.value("operationId")) ++matching;
    if (prefix.isEmpty() || !events.startsWith(prefix) || appended != 1 || matching != 1 || !eventsError.isEmpty()
        || event.value("eventType") != "TASK_STARTED" || event.value("task") != "Start P6 through canonical lifecycle" || event.value("provenance") != provenance
        || event.value("contractId") != contract.value("contractId") || event.value("startedAt") != receipt.value("startedAt")
        || event.value("lifecycle") != processVersionStateToJson(model.processVersionState()) || event.value("eventId") != receipt.value("auditRecord").toObject().value("eventId"))
        issue(errors, "ACTIVATION_AUDIT_INVALID", "Exactly one real canonical start event with unchanged historical prefix is required.");
    for (const QString& path : {QStringLiteral("ARAMF_WORKER/context/task-dag.json"), QStringLiteral("ARAMF_WORKER/memory/decisions.md"),
        QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl")})
        if (contract.value("baseline").toObject().value(path) != fileHash(pathFor(model, path)))
            issue(errors, "ACTIVATION_HISTORY_INVALID", "Activation preserves the original task DAG, decisions and certificate ledger bytes.", path);
    validateCanonical(model, errors);
    const auto generated = GenerationServices::derivedTaskArtifacts(model, model.generationOptions());
    for (auto it = generated.begin(); it != generated.end(); ++it) {
        auto actual = readObject(pathFor(model, it.key())), expected = it.value().toObject(); actual.remove("_file"); expected.remove("_file");
        if (actual != expected) issue(errors, "ACTIVATION_DERIVED_INVALID", "Canonical generated output differs from the active model.", it.key());
    }
    const auto preview = ConfigurationUpdateService::preview(model);
    const auto verified = VerificationServices().verify(model, model.generationOptions(), false);
    auto summary = readObject(pathFor(model, requirementsVerificationPaths().at(0))), report = readObject(pathFor(model, requirementsVerificationPaths().at(1)));
    summary.remove("_file"); report.remove("_file"); report.remove("checkedAt"); auto expectedReport = verified.evidence; expectedReport.remove("checkedAt");
    if (!preview.success || !preview.noChange || verified.overallStatus != VerificationStatus::Pass || summary != verified.summary || report != expectedReport)
        issue(errors, "ACTIVATION_VALIDATION_INVALID", "Current pure validation and canonical saved generation must agree.");
    QString memoryError;
    auto expectedMemory = ProjectMemory().validate(root(model), &memoryError, false);
    auto actualMemory = readObject(pathFor(model, "ARAMF_WORKER/memory/memory-consistency-validation.json"));
    for (const QString& key : {QStringLiteral("checkedAt"), QStringLiteral("_file")}) { expectedMemory.remove(key); actualMemory.remove(key); }
    if (!memoryError.isEmpty() || expectedMemory.value("status") != "PASS" || actualMemory != expectedMemory)
        issue(errors, "ACTIVATION_VALIDATION_INVALID", "Current pure memory validation must match the exact retained canonical report.");
    const QJsonObject context{{operationContextPaths().at(0), ContextCoordinationService::buildIndex(model)}, {operationContextPaths().at(1), ContextCoordinationService::compress(model)},
        {operationContextPaths().at(2), ContextCoordinationService::freshness(model)}, {operationContextPaths().at(3), ContextCoordinationService::adapterDescriptors()}};
    for (auto it = context.begin(); it != context.end(); ++it) {
        auto actual = readObject(pathFor(model, it.key())), expected = it.value().toObject(); actual.remove("_file"); expected.remove("_file");
        if (actual != expected) issue(errors, "ACTIVATION_CONTEXT_INVALID", "Current context must match its canonical producer.", it.key());
    }
    operationBoundary(model, contract.value("baseline").toObject(), operationSnapshot(model, errors), outputs.keys(), errors);
    const bool valid = state(errors).startsWith("READY");
    return {{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractId", contract.value("contractId")}, {"status", valid ? "PASS" : "FAIL"},
        {"completionState", valid ? "VERIFIED" : "BLOCKED"}, {"errors", errors}, {"missingEvidence", valid ? QJsonArray{} : QJsonArray{"canonical-activation-receipts"}},
        {"lifecycle", processVersionStateToJson(model.processVersionState())}, {"gates", receipt.value("gates")}, {"receipt", shape.value("receipt")}};
}

namespace {
const QString scopeRegistrationKind = "canonical-scope-registration-operation/v1";
const QString scopeRegistrationProducer = "WorkerTaskServices/scope-registration-operation/v1";
const QString scopeRegistrationProfile = "task-scope-registration-operations/v1";

bool readScopeRegistrationModel(const QString& path, ProjectModel* model, QString* error)
{
    // load() can save a migration. Every scope-operation read, including CLI
    // loading and apply readback, must inspect the current saved schema only.
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(rawBytes(path), &parseError);
    auto object = document.object(); const auto version = object.value("schemaVersion");
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || !version.isDouble()
        || version.toDouble() != ProjectPersistence::currentSchemaVersion()) {
        if (error) *error = "Scope registration requires an existing current-schema saved model; migration is a separate operation";
        return false;
    }
    // Match load()'s selected-file semantics without writing that file.
    object.insert("projectFilePath", path);
    return ProjectPersistence().fromJson(model, object, error);
}

QJsonObject unwrappedContract(const QJsonObject& value)
{
    return value.contains("contract") ? value.value("contract").toObject() : value;
}

QJsonObject scopeRegistrationShape(const ProjectModel& model, const QJsonObject& intent,
                                   const QJsonObject& original, const QString& id,
                                   ProjectModel* proposed, QJsonArray& errors)
{
    const QSet<QString> keys{"kind", "scope", "files", "agentId", "reason", "validityMinutes"};
    const auto intentKeys = intent.keys();
    const auto task = WorkerTaskRequest::fromJson(original.value("request").toObject());
    auto unsignedOriginal = original; unsignedOriginal.remove("contractId");
    const QString scope = intent.value("scope").toString();
    const auto files = strings(intent.value("files"));
    const int minutes = intent.value("validityMinutes").toInt();
    if (QSet<QString>(intentKeys.cbegin(), intentKeys.cend()) != keys || intent.value("kind") != "add-scope-files/v1"
        || !QRegularExpression("^[A-Za-z0-9_./-]{1,128}$").match(intent.value("agentId").toString()).hasMatch()
        || QStringList{"none", "human", "Admin Morgan Lindbom"}.contains(intent.value("agentId").toString())
        || intent.value("reason").toString().trimmed().isEmpty() || intent.value("reason").toString().size() > 1024
        || minutes < 1 || minutes > 1440 || intent.value("validityMinutes").toDouble() != minutes
        || !intent.value("files").isArray() || files.isEmpty() || files.size() != intent.value("files").toArray().size()
        || !QRegularExpression("^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$").match(id).hasMatch())
        issue(errors, "SCOPE_REGISTRATION_INTENT_INVALID", "Exact declarative file additions, actual agent provenance and 1..1440 validity minutes are required.");
    if (original.value("schemaVersion").toInt() != 1 || original.value("authority") != "DERIVED"
        || original.contains("contractKind") || fingerprint(unsignedOriginal) != original.value("contractId")
        || original.value("preflight").toObject().value("status") != "BLOCKED"
        || original.value("binding").toObject().value("projectId") != model.projectId()
        || original.value("binding").toObject().value("projectRoot") != root(model)
        || original.value("binding").toObject().value("workerIdentity") != worker(model)
        || task.destructive || !task.relocations.isEmpty() || !task.reauthorizationId.isEmpty() || !task.protectedGrantId.isEmpty())
        issue(errors, "SCOPE_REGISTRATION_ORIGIN_INVALID", "The exact original ordinary blocked task from this project is required.");
    auto rules = model.ruleConfiguration();
    if (!QStringList{"source-code", "tests"}.contains(scope) || !rules.projectScopes.contains(scope)
        || !task.scopes.contains(scope) || !rules.scopeMetadata.value(scope).isObject())
        issue(errors, "SCOPE_REGISTRATION_SCOPE_INVALID", "Use an existing active source-code or tests scope already declared by the linked task.");
    QStringList missing;
    for (const auto& file : task.files) {
        bool mapped = false;
        for (const auto& metadata : rules.scopeMetadata) mapped |= strings(metadata.toObject().value("files")).contains(file);
        if (!mapped) missing.append(file);
    }
    if (array(missing) != array(files)) issue(errors, "SCOPE_REGISTRATION_FILES_INVALID", "Register exactly the missing paths of the linked task, with no already mapped or unrelated files.");
    const auto policy = WorkerTaskServices::mutationPolicy(model);
    QSet<QString> folded;
    for (const auto& file : files) {
        const auto owner = fileRole(model, file, policy);
        const QString suffix = QFileInfo(file).suffix().toLower();
        const QString prefix = scope == "tests" ? "tests/" : "src/";
        bool conflict = false;
        for (const auto& metadata : rules.scopeMetadata)
            for (const auto& mapped : strings(metadata.toObject().value("files"))) conflict |= mapped.compare(file, Qt::CaseInsensitive) == 0;
        if (!task.files.contains(file) || !file.startsWith(prefix) || file.contains('\\') || file.contains('*') || file.contains('?')
            || file.contains('[') || file.contains(']') || QDir::cleanPath(file) != file || !operationSafePath(model, file)
            || !QStringList{"h", "hpp", "c", "cc", "cpp", "cxx"}.contains(suffix)
            || QFileInfo(pathFor(model, file)).isDir() || owner.value("owner") != "user"
            || strings(owner.value("actions")).contains("NEVER_TOUCH") || folded.contains(file.toCaseFolded()) || conflict)
            issue(errors, "SCOPE_REGISTRATION_PATH_INVALID", "Only unique unmapped ordinary source files in the exact task may be registered.", file);
        folded.insert(file.toCaseFolded());
    }
    for (const auto& value : original.value("preflight").toObject().value("errors").toArray()) {
        const auto error = value.toObject();
        if (error.value("severity") == "WARNING") continue;
        if (!QStringList{"FILE_OWNERSHIP_UNKNOWN", "TASK_SCOPE_VIOLATION"}.contains(error.value("code").toString()))
            issue(errors, "SCOPE_REGISTRATION_ORIGIN_INVALID", "Registration cannot repair a task blocked for a different governance reason.");
    }
    QString error;
    if (!ProjectPersistence().fromJson(proposed, ProjectPersistence().toJson(model), &error))
        issue(errors, "SCOPE_REGISTRATION_MODEL_INVALID", error);
    proposed->setProjectPath(model.projectPath()); proposed->setProjectFilePath(model.projectFilePath());
    auto metadata = rules.scopeMetadata.value(scope).toObject();
    metadata.insert("files", array(strings(metadata.value("files")) + files));
    rules.scopeMetadata.insert(scope, metadata); proposed->setRuleConfiguration(rules);
    // This operation has no document, certificate, migration or bootstrap authority.
    const auto state = ConfigurationUpdateService::canonicalState(model);
    const auto academic = state.value("academic").toObject();
    if (worker(model) != "ARAMF_WORKER" || model.certificationConfiguration().enabled || academic.value("enabled").toBool()
        || !model.generationOptions().generateRouting || !model.generationOptions().generateMemory)
        issue(errors, "SCOPE_REGISTRATION_CONFIGURATION_INVALID", "Existing standard Worker routing and memory with inactive document/certificate generation are required.");
    const QString saved = QDir(root(model)).relativeFilePath(model.projectFilePath());
    if (!saved.endsWith(".aramf.json") || !QFileInfo(pathFor(model, saved)).isFile() || !operationSafePath(model, saved))
        issue(errors, "SCOPE_REGISTRATION_CONFIGURATION_INVALID", "The selected saved configuration must be a regular project-local file.");
    const QString directory = workerPath(model, "verification/scope-registration/" + id);
    QJsonObject outputs;
    for (const auto& path : ConfigurationUpdateService::effectiveOutputFiles(*proposed)) outputs.insert(path, "ConfigurationUpdateService");
    outputs.insert(saved, "ProjectPersistence");
    for (const auto& path : operationContextPaths()) outputs.insert(path, "ContextCoordinationService");
    for (const QString& path : {QStringLiteral("memory/event-log.jsonl"), QStringLiteral("memory/memory-manifest.json"),
        QStringLiteral("memory/metrics.json"), QStringLiteral("memory/current-state.md"), QStringLiteral("memory/cold-start-validation.json"),
        QStringLiteral("memory/memory-consistency-validation.json")}) outputs.insert(workerPath(model, path), "ProjectMemory");
    for (const QString& name : {QStringLiteral("proof.json"), QStringLiteral("receipt.json"), QStringLiteral("seal.json")}) outputs.insert(directory + '/' + name, scopeRegistrationProducer);
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (!operationSafePath(model, it.key()) || QFileInfo(pathFor(model, it.key())).isDir())
            issue(errors, "SCOPE_REGISTRATION_OUTPUT_INVALID", "Canonical outputs must be exact regular paths without links.", it.key());
    return {{"outputs", outputs}, {"proof", directory + "/proof.json"}, {"receipt", directory + "/receipt.json"}, {"seal", directory + "/seal.json"}};
}

QJsonObject scopeRegistrationSnapshot(const ProjectModel& model, const QJsonObject& original, QJsonArray& errors)
{
    auto observed = operationSnapshot(model, errors);
    for (const auto& file : strings(original.value("request").toObject().value("files")))
        if (operationSafePath(model, file)) observed.insert(file, fileHash(pathFor(model, file)));
    return observed;
}

QJsonObject scopeRegistrationSources(const ProjectModel& model, const QJsonObject& original, const QJsonObject& verification)
{
    QJsonObject observed;
    const auto paths = strings(original.value("request").toObject().value("files")) + strings(verification.value("taskDependencies"));
    for (const auto& file : paths)
        if ((file.startsWith("src/") || file.startsWith("tests/")) && operationSafePath(model, file)) observed.insert(file, fileHash(pathFor(model, file)));
    return observed;
}

QJsonObject deriveScopeRegistration(const ProjectModel& model, const QJsonObject& intent, const QJsonObject& original,
                                    const QJsonObject& verification, const QString& id)
{
    QJsonArray errors; ProjectModel proposed;
    const auto shape = scopeRegistrationShape(model, intent, original, id, &proposed, errors);
    validateCanonical(model, errors);
    ProjectModel saved; QString error;
    if (!readScopeRegistrationModel(model.projectFilePath(), &saved, &error)
        || ProjectPersistence().toJson(saved) != ProjectPersistence().toJson(model) || original.value("binding") != binding(model))
        issue(errors, "SCOPE_REGISTRATION_CONFIGURATION_INVALID", "The selected saved configuration and original task binding must still be current.");
    const auto currentPreview = ConfigurationUpdateService::preview(model), preview = ConfigurationUpdateService::preview(proposed);
    if (!currentPreview.success || !currentPreview.noChange || !preview.success || preview.noChange || preview.removalBlocked
        || !preview.plan.value("modified").toArray().isEmpty())
        issue(errors, "SCOPE_REGISTRATION_CONFIGURATION_INVALID", "Only the derived additive mapping diff may change the current generated configuration.");
    if (!QFileInfo(pathFor(model, workerPath(model, "context/task-dag.json"))).isFile())
        issue(errors, "SCOPE_REGISTRATION_CONFIGURATION_INVALID", "Existing task DAG must be preserved.");
    const auto freshTask = WorkerTaskServices::prepare(model, WorkerTaskRequest::fromJson(original.value("request").toObject()));
    for (const auto& value : freshTask.value("preflight").toObject().value("errors").toArray()) {
        const auto e = value.toObject();
        if (e.value("severity") != "WARNING" && !QStringList{"FILE_OWNERSHIP_UNKNOWN", "TASK_SCOPE_VIOLATION"}.contains(e.value("code").toString()))
            issue(errors, "SCOPE_REGISTRATION_ORIGIN_INVALID", "Current ordinary preparation has a blocker beyond the missing mappings.");
    }
    for (const QString& field : {QStringLiteral("permittedFiles"), QStringLiteral("requiredEvidence"), QStringLiteral("risk"),
        QStringLiteral("impact"), QStringLiteral("evidenceDependencies"), QStringLiteral("taskDependencies"), QStringLiteral("protectedGrant")})
        if (semantic(original.value(field)) != semantic(freshTask.value(field)))
            issue(errors, "SCOPE_REGISTRATION_ORIGIN_INVALID", "Original task authority differs from current canonical derivation.", field);
    for (const QString& key : {QStringLiteral("proof"), QStringLiteral("receipt"), QStringLiteral("seal")})
        if (QFileInfo::exists(pathFor(model, shape.value(key).toString()))) issue(errors, "SCOPE_REGISTRATION_REPLAY", "A planned immutable destination already exists.");
    const auto baseline = scopeRegistrationSnapshot(model, original, errors);
    const auto sources = scopeRegistrationSources(model, original, verification);
    QJsonObject proof; QString anchor;
    if (state(errors).startsWith("READY")) {
        const auto profile = measuredProfile(verification);
        const auto index = WorkerTaskServices::checkEvidence(model, verification);
        const auto report = WorkerTaskServices::postflight(model, verification, index.value("evidence").toArray());
        if (profile.value("id") != scopeRegistrationProfile || verification.contains("contractKind")
            || semantic(verification.value("request").toObject().value("files")) != semantic(profile.value("files"))
            || index.value("status") != "PASS" || report.value("completionState") != "VERIFIED")
            issue(errors, "SCOPE_REGISTRATION_VERIFICATION_INVALID", "Current complete measured verification of the exact registration service is required.");
        else {
            QJsonObject records;
            for (const auto& value : report.value("acceptedEvidence").toArray()) {
                const auto entry = value.toObject(); const QString path = entry.value("artifact").toString();
                const auto contents = rawBytes(pathFor(model, path)); const auto record = QJsonDocument::fromJson(contents).object();
                records.insert(path, QJsonObject{{"fingerprint", digest(contents)}, {"bytesBase64", QString::fromLatin1(contents.toBase64())}});
                if (record.value("finishedAt").toString() > anchor) anchor = record.value("finishedAt").toString();
            }
            proof = {{"softwareContract", verification}, {"softwareModel", ProjectPersistence().toJson(model)}, {"postflight", report}, {"records", records}};
            validateFrozenSoftwareProof(model, proof, errors);
        }
    }
    const auto issued = QDateTime::fromString(anchor, Qt::ISODateWithMs);
    const auto expires = issued.addSecs(intent.value("validityMinutes").toInt() * 60);
    if (state(errors).startsWith("READY")
        && (!issued.isValid() || issued > QDateTime::currentDateTimeUtc() || QDateTime::currentDateTimeUtc() >= expires))
        issue(errors, "SCOPE_REGISTRATION_EXPIRED", "Validity is bounded by the latest immutable successful software observation; rerun required evidence when expired.");
    if (sources != scopeRegistrationSources(model, original, verification) || baseline != scopeRegistrationSnapshot(model, original, errors))
        issue(errors, "SCOPE_REGISTRATION_BASE_CHANGED", "Inputs changed during read-only preparation.");
    QJsonObject result{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractKind", scopeRegistrationKind}, {"operationId", id},
        {"intent", intent}, {"originalTask", original}, {"verificationContract", verification}, {"softwareProof", proof},
        {"binding", binding(model)}, {"beforeModel", ProjectPersistence().toJson(model)}, {"proposedModel", ProjectPersistence().toJson(proposed)},
        {"preview", preview.plan}, {"baseline", baseline}, {"sourceBindings", sources}, {"sourceRevision", operationRevision(model)},
        {"historyPrefixBase64", QString::fromLatin1(rawBytes(pathFor(model, workerPath(model, "memory/event-log.jsonl"))).toBase64())},
        {"shape", shape}, {"permittedFiles", array(shape.value("outputs").toObject().keys())}, {"canonicalProducers", shape.value("outputs")},
        {"issuedAt", anchor}, {"expiresAt", expires.toString(Qt::ISODateWithMs)},
        {"requiredEvidence", QJsonArray{"canonical-configuration-readback", "exact-source-preservation", "immutable-receipts", "actual-agent-audit", "normal-successor-preparation"}},
        {"preflight", QJsonObject{{"status", state(errors)}, {"errors", errors}}}, {"completionState", "NOT_STARTED"}};
    result.insert("contractId", fingerprint(result)); return result;
}

QJsonObject scopeRegistrationOutputHashes(const ProjectModel& model, const QJsonObject& shape)
{
    QJsonObject hashes; const auto outputs = shape.value("outputs").toObject();
    for (auto it = outputs.begin(); it != outputs.end(); ++it) {
        const QString path = it.key();
        if (path == shape.value("receipt") || path == shape.value("seal") || operationContextPaths().contains(path)
            || path.endsWith("/memory-consistency-validation.json")
            || path.endsWith("/latest-validation.json") || path.endsWith("/verification-result.json")) continue;
        hashes.insert(path, fileHash(pathFor(model, path)));
    }
    return hashes;
}
}

QJsonObject WorkerTaskServices::prepareScopeRegistrationOperation(const ProjectModel& model, const QJsonObject& intent,
                                                                  const QJsonObject& blockedTask, const QJsonObject& verification)
{
    return deriveScopeRegistration(model, intent, unwrappedContract(blockedTask), unwrappedContract(verification), QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject WorkerTaskServices::applyScopeRegistrationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = unwrappedContract(supplied);
    const auto blocked = [](const QString& message) { return QJsonObject{{"status", "BLOCKED"}, {"error", message}}; };
    if (contract.value("contractKind") != scopeRegistrationKind || !contract.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("An exact READY scope-registration operation is required");
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0); if (!lock.tryLock(0)) return blocked("Canonical project operation is busy");
    const auto current = deriveScopeRegistration(model, contract.value("intent").toObject(), contract.value("originalTask").toObject(),
        contract.value("verificationContract").toObject(), contract.value("operationId").toString());
    if (current != contract || !current.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("Original task, measured software, exact outputs, validity or project baseline changed");
    const auto shape = contract.value("shape").toObject(); ProjectModel proposed; QString error;
    if (!ProjectPersistence().fromJson(&proposed, contract.value("proposedModel").toObject(), &error)) return blocked(error);
    proposed.setProjectPath(model.projectPath()); proposed.setProjectFilePath(model.projectFilePath());
    const auto issued = QDateTime::fromString(contract.value("issuedAt").toString(), Qt::ISODateWithMs);
    const auto expires = QDateTime::fromString(contract.value("expiresAt").toString(), Qt::ISODateWithMs);
    const auto withinValidity = [&](const QDateTime& observed) { return issued.isValid() && expires.isValid() && observed >= issued && observed < expires; };
    if (!withinValidity(QDateTime::currentDateTimeUtc())) return blocked("Registration expired before its first write");
    QDir().mkpath(QFileInfo(pathFor(model, shape.value("proof").toString())).absolutePath());
    if (!writeNew(pathFor(model, shape.value("proof").toString()), QJsonDocument(QJsonObject{{"producer", scopeRegistrationProducer}, {"operationContract", contract}}).toJson()))
        return blocked("Immutable proof could not be created; partial bytes are retained");
    QString failure;
    const auto applyStarted = QDateTime::currentDateTimeUtc();
    if (!withinValidity(applyStarted)) failure = "Registration expired before canonical configuration apply";
    else {
        const auto updated = ConfigurationUpdateService::apply(proposed, contract.value("preview").toObject().value("validationFingerprint").toString());
        if (!updated.success || !updated.configurationSaved || !updated.derivedSynchronized || !updated.verified) failure = "Canonical configuration update failed: " + updated.error;
    }
    QJsonObject audit;
    if (failure.isEmpty()) {
        const QJsonObject fields{{"status", "PASS"}, {"scope", "project"}, {"operationId", contract.value("operationId")}, {"contractId", contract.value("contractId")},
            {"originalTaskId", contract.value("originalTask").toObject().value("contractId")}, {"registeredScope", contract.value("intent").toObject().value("scope")},
            {"registeredFiles", contract.value("intent").toObject().value("files")},
            {"provenance", QJsonObject{{"actor", "agent"}, {"agentId", contract.value("intent").toObject().value("agentId")}, {"tool", scopeRegistrationProducer}}}};
        if (!ProjectMemory().appendEvent(root(model), "SCOPE_MAPPING_REGISTERED", "Register exact missing task source mappings through canonical configuration", fields, &error)) failure = "Canonical audit failed: " + error;
        else for (const auto& event : ProjectMemory().events(root(model), &error)) if (event.value("operationId") == contract.value("operationId")) audit = event;
    }
    ProjectModel reloaded; const bool loaded = readScopeRegistrationModel(model.projectFilePath(), &reloaded, &error);
    const bool expectedModel = loaded && ProjectPersistence().toJson(reloaded) == contract.value("proposedModel");
    if (!expectedModel && failure.isEmpty()) failure = "Saved model readback differs from the exact additions";
    if (expectedModel) {
        const auto cold = ProjectMemory().validateColdStart(root(model), &error);
        const auto memory = ProjectMemory().validate(root(model), &error);
        if ((!error.isEmpty() || cold.value("status") != "PASS" || memory.value("status") != "PASS") && failure.isEmpty())
            failure = "Canonical cold-start or memory refresh failed: " + error;
    }
    const auto completed = QDateTime::currentDateTimeUtc();
    if (!withinValidity(completed) && failure.isEmpty()) failure = "Registration completed outside its measured validity period";
    const QJsonObject receipt{{"producer", scopeRegistrationProducer}, {"operationId", contract.value("operationId")}, {"contractId", contract.value("contractId")},
        {"outcome", failure.isEmpty() ? "SCOPE_FILES_REGISTERED" : "FAILED"}, {"error", failure}, {"audit", audit},
        {"outputs", scopeRegistrationOutputHashes(model, shape)}, {"applyStartedAt", applyStarted.toString(Qt::ISODateWithMs)},
        {"timestamp", completed.toString(Qt::ISODateWithMs)}};
    if (!writeNew(pathFor(model, shape.value("receipt").toString()), QJsonDocument(receipt).toJson())
        || !writeNew(pathFor(model, shape.value("seal").toString()), QJsonDocument(QJsonObject{{"producer", scopeRegistrationProducer},
            {"contractId", contract.value("contractId")}, {"receiptFingerprint", fileHash(pathFor(model, shape.value("receipt").toString()))}}).toJson()))
        return blocked("Registration outputs retained; immutable receipt or seal failed");
    if (!expectedModel) return blocked(failure);
    const auto context = ContextCoordinationService::generate(reloaded);
    const auto validation = VerificationServices().verify(reloaded, reloaded.generationOptions());
    if (!error.isEmpty() || !context.value("success").toBool()
        || validation.overallStatus != VerificationStatus::Pass) return blocked("Registration retained; final canonical synchronization failed: " + error);
    const auto report = postflightScopeRegistrationOperation(reloaded, contract);
    return {{"status", failure.isEmpty() && report.value("completionState") == "VERIFIED" ? "PASS" : "FAIL"}, {"error", failure}, {"postflight", report}, {"receipt", shape.value("receipt")}};
}

QJsonObject WorkerTaskServices::postflightScopeRegistrationOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = unwrappedContract(supplied); auto unsignedContract = contract; unsignedContract.remove("contractId");
    QJsonArray errors; ProjectModel before, proposed; QString error;
    if (!ProjectPersistence().fromJson(&before, contract.value("beforeModel").toObject(), &error)) issue(errors, "SCOPE_REGISTRATION_CONTRACT_INVALID", error);
    before.setProjectPath(model.projectPath()); before.setProjectFilePath(model.projectFilePath());
    const auto shape = scopeRegistrationShape(before, contract.value("intent").toObject(), contract.value("originalTask").toObject(), contract.value("operationId").toString(), &proposed, errors);
    const auto software = contract.value("softwareProof").toObject();
    const auto verification = contract.value("verificationContract").toObject();
    const auto profile = measuredProfile(verification);
    ProjectModel initialSaved; QString savedReadError;
    if (!readScopeRegistrationModel(model.projectFilePath(), &initialSaved, &savedReadError)
        || ProjectPersistence().toJson(initialSaved) != ProjectPersistence().toJson(model))
        issue(errors, "SCOPE_REGISTRATION_CONFIGURATION_INVALID", "Current-schema saved model readback must match the inspected model.");
    if (contract.value("contractKind") != scopeRegistrationKind || fingerprint(unsignedContract) != contract.value("contractId")
        || !contract.value("preflight").toObject().value("status").toString().startsWith("READY")
        || contract.value("shape") != shape || contract.value("permittedFiles") != array(shape.value("outputs").toObject().keys())
        || contract.value("canonicalProducers") != shape.value("outputs") || contract.value("proposedModel") != ProjectPersistence().toJson(proposed)
        || ProjectPersistence().toJson(model) != ProjectPersistence().toJson(proposed)
        || contract.value("binding").toObject().value("projectId") != model.projectId()
        || contract.value("binding").toObject().value("projectRoot") != root(model)
        || contract.value("sourceRevision") != operationRevision(model)
        || contract.value("beforeModel") != software.value("softwareModel") || software.value("softwareContract") != verification
        || profile.value("id") != scopeRegistrationProfile || semantic(verification.value("request").toObject().value("files")) != semantic(profile.value("files"))
        || verification.value("binding") != contract.value("binding") || contract.value("originalTask").toObject().value("binding") != contract.value("binding")
        || contract.value("binding").toObject().value("modelFingerprint") != fingerprint(ProjectPersistence().configuration(before))
        || contract.value("sourceBindings") != scopeRegistrationSources(model, contract.value("originalTask").toObject(), contract.value("verificationContract").toObject()))
        issue(errors, "SCOPE_REGISTRATION_CONTRACT_INVALID", "Operation identity, exact additions, unchanged sources or canonical outputs differ.");
    QString anchor;
    const auto retainedRecords = software.value("records").toObject();
    for (auto it = retainedRecords.begin(); it != retainedRecords.end(); ++it) {
        const auto captured = it.value().toObject();
        if (!operationSafePath(model, it.key()) || fileHash(pathFor(model, it.key())) != captured.value("fingerprint"))
            issue(errors, "SCOPE_REGISTRATION_VERIFICATION_INVALID", "Original measured evidence bytes must remain retained unchanged.", it.key());
        const auto record = QJsonDocument::fromJson(QByteArray::fromBase64(captured.value("bytesBase64").toString().toLatin1())).object();
        if (record.value("finishedAt").toString() > anchor) anchor = record.value("finishedAt").toString();
    }
    const auto expires = QDateTime::fromString(anchor, Qt::ISODateWithMs).addSecs(contract.value("intent").toObject().value("validityMinutes").toInt() * 60);
    if (contract.value("issuedAt") != anchor || contract.value("expiresAt") != expires.toString(Qt::ISODateWithMs))
        issue(errors, "SCOPE_REGISTRATION_EXPIRED", "Validity must derive from the unchanged immutable software observation.");
    const auto baseline = contract.value("baseline").toObject();
    const auto dependencies = verification.value("evidenceDependencies").toObject();
    for (auto it = dependencies.begin(); it != dependencies.end(); ++it) {
        QJsonObject observed;
        for (const auto& path : strings(it.value())) observed.insert(path, baseline.value(path).toString("MISSING"));
        if (software.value("postflight").toObject().value("evidenceFingerprints").toObject().value(it.key()) != fingerprint(observed))
            issue(errors, "SCOPE_REGISTRATION_VERIFICATION_INVALID", "Before-state differs from the dependencies actually measured by the software producer.", it.key());
    }
    const auto proof = readObject(pathFor(model, shape.value("proof").toString()));
    const auto receipt = readObject(pathFor(model, shape.value("receipt").toString()));
    const auto seal = readObject(pathFor(model, shape.value("seal").toString()));
    if (proof != QJsonObject{{"producer", scopeRegistrationProducer}, {"operationContract", contract}}
        || receipt.value("producer") != scopeRegistrationProducer || receipt.value("contractId") != contract.value("contractId")
        || receipt.value("operationId") != contract.value("operationId") || receipt.value("outcome") != "SCOPE_FILES_REGISTERED"
        || !receipt.value("error").toString().isEmpty() || receipt.value("outputs") != scopeRegistrationOutputHashes(model, shape)
        || seal != QJsonObject{{"producer", scopeRegistrationProducer}, {"contractId", contract.value("contractId")},
            {"receiptFingerprint", fileHash(pathFor(model, shape.value("receipt").toString()))}})
        issue(errors, "SCOPE_REGISTRATION_RECEIPT_INVALID", "Immutable proof, successful receipt, seal or output readback differs.");
    const auto completed = QDateTime::fromString(receipt.value("timestamp").toString(), Qt::ISODateWithMs);
    const auto applyStarted = QDateTime::fromString(receipt.value("applyStartedAt").toString(), Qt::ISODateWithMs);
    if (!applyStarted.isValid() || applyStarted < QDateTime::fromString(contract.value("issuedAt").toString(), Qt::ISODateWithMs)
        || !completed.isValid() || completed < applyStarted
        || completed >= QDateTime::fromString(contract.value("expiresAt").toString(), Qt::ISODateWithMs))
        issue(errors, "SCOPE_REGISTRATION_EXPIRED", "The operation did not complete within its measured validity period.");
    validateFrozenSoftwareProof(model, software, errors);
    const auto history = rawBytes(pathFor(model, workerPath(model, "memory/event-log.jsonl")));
    const auto prefix = QByteArray::fromBase64(contract.value("historyPrefixBase64").toString().toLatin1());
    const auto audit = receipt.value("audit").toObject(); int matched = 0;
    for (const auto& event : ProjectMemory().events(root(model), &error)) if (event.value("operationId") == contract.value("operationId")) { ++matched; if (event != audit) matched += 2; }
    const QJsonObject provenance{{"actor", "agent"}, {"agentId", contract.value("intent").toObject().value("agentId")}, {"tool", scopeRegistrationProducer}};
    if (prefix.isEmpty() || !history.startsWith(prefix) || matched != 1 || !error.isEmpty() || audit.value("eventType") != "SCOPE_MAPPING_REGISTERED"
        || audit.value("provenance") != provenance || audit.value("contractId") != contract.value("contractId")
        || audit.value("originalTaskId") != contract.value("originalTask").toObject().value("contractId")
        || audit.value("registeredFiles") != contract.value("intent").toObject().value("files")
        || audit.value("registeredScope") != contract.value("intent").toObject().value("scope") || audit.value("status") != "PASS")
        issue(errors, "SCOPE_REGISTRATION_AUDIT_INVALID", "Original append-only history and exact actual-agent registration audit are required.");
    validateCanonical(model, errors);
    const auto verified = VerificationServices().verify(model, model.generationOptions(), false);
    auto summary = readObject(pathFor(model, workerPath(model, "verification/latest-validation.json"))); summary.remove("_file");
    auto detailed = readObject(pathFor(model, workerPath(model, "verification/verification-result.json"))); detailed.remove("_file"); detailed.remove("checkedAt");
    auto expectedDetail = verified.evidence; expectedDetail.remove("checkedAt");
    if (verified.overallStatus != VerificationStatus::Pass || summary != verified.summary || detailed != expectedDetail)
        issue(errors, "SCOPE_REGISTRATION_READBACK_INVALID", "Persisted verification differs from current canonical service observations.");
    auto expectedMemory = ProjectMemory().validate(root(model), &error, false);
    auto actualMemory = readObject(pathFor(model, workerPath(model, "memory/memory-consistency-validation.json")));
    for (const auto& field : {"checkedAt", "_file"}) { expectedMemory.remove(field); actualMemory.remove(field); }
    if (!error.isEmpty() || expectedMemory.value("status") != "PASS" || actualMemory != expectedMemory)
        issue(errors, "SCOPE_REGISTRATION_READBACK_INVALID", "Current canonical memory validation or its persisted report differs.");
    const QJsonObject context{{operationContextPaths().at(0), ContextCoordinationService::buildIndex(model)},
        {operationContextPaths().at(1), ContextCoordinationService::compress(model)},
        {operationContextPaths().at(2), ContextCoordinationService::freshness(model)},
        {operationContextPaths().at(3), ContextCoordinationService::adapterDescriptors()}};
    for (auto it = context.begin(); it != context.end(); ++it) {
        auto actual = readObject(pathFor(model, it.key())), expected = it.value().toObject(); actual.remove("_file"); expected.remove("_file");
        if (actual != expected) issue(errors, "SCOPE_REGISTRATION_CONTEXT_INVALID", "Current context differs from its canonical producer.", it.key());
    }
    for (const QString& path : {QStringLiteral("context/task-dag.json"), QStringLiteral("certification/certificates.jsonl")}) {
        const auto relative = workerPath(model, path);
        if (contract.value("baseline").toObject().value(relative).toString("MISSING") != fileHash(pathFor(model, relative)))
            issue(errors, "SCOPE_REGISTRATION_HISTORY_INVALID", "Task DAG and certificate history cannot change.", relative);
    }
    ProjectModel saved;
    if (!readScopeRegistrationModel(model.projectFilePath(), &saved, &error) || ProjectPersistence().toJson(saved) != ProjectPersistence().toJson(model))
        issue(errors, "SCOPE_REGISTRATION_READBACK_INVALID", "Saved canonical model differs.");
    const auto successor = WorkerTaskServices::prepare(model, WorkerTaskRequest::fromJson(contract.value("originalTask").toObject().value("request").toObject()));
    if (!successor.value("preflight").toObject().value("status").toString().startsWith("READY"))
        issue(errors, "SCOPE_REGISTRATION_SUCCESSOR_BLOCKED", "A newly derived ordinary task remains blocked by current governance.");
    operationBoundary(model, contract.value("baseline").toObject(), scopeRegistrationSnapshot(model, contract.value("originalTask").toObject(), errors), shape.value("outputs").toObject().keys(), errors);
    const bool valid = state(errors).startsWith("READY");
    return {{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractId", contract.value("contractId")}, {"status", valid ? "PASS" : "FAIL"},
        {"completionState", valid ? "VERIFIED" : "BLOCKED"}, {"errors", errors}, {"successorPreflight", successor.value("preflight")},
        {"implementationAuthority", "Prepare and retain a separate ordinary READY TaskContract before source edits."}};
}

QJsonObject WorkerTaskServices::prepareRequirementsOperation(const ProjectModel& model, const QJsonObject& intent,
                                                            const QJsonObject& verifiedIntegration)
{
    return deriveRequirementsOperation(model, intent, verifiedIntegration, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QJsonObject WorkerTaskServices::applyRequirementsOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    const auto blocked = [](const QString& error) { return QJsonObject{{"status", "BLOCKED"}, {"error", error}}; };
    if (contract.value("contractKind") != requirementsKind || !contract.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("An exact READY requirements operation is required");
    QLockFile lock(QDir::tempPath() + "/aramf-certification-operation-" + digest(root(model).toUtf8()) + ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Canonical project operation is busy");
    const auto current = deriveRequirementsOperation(model, contract.value("intent").toObject(), contract.value("verificationContract").toObject(), contract.value("operationId").toString());
    if (current != contract || !current.value("preflight").toObject().value("status").toString().startsWith("READY"))
        return blocked("Exact READY proposal, source, identity, software proof or baseline changed");
    QJsonArray errors; ProjectModel proposed;
    const auto shape = requirementsShape(model, contract.value("intent").toObject(), contract.value("operationId").toString(), &proposed, errors);
    QDir().mkpath(QFileInfo(pathFor(model, shape.value("proof").toString())).absolutePath());
    if (!writeNew(pathFor(model, shape.value("proof").toString()), QJsonDocument(QJsonObject{{"producer", requirementsProducer}, {"operationContract", contract}}).toJson()))
        return blocked("Immutable requirements proof write failed; any partial bytes are retained");
    // New proof is an authoritative Worker resource input. The configuration
    // producer regenerates its context as part of its exact output transaction.
    QString failure; QJsonArray stages;
    const auto update = ConfigurationUpdateService::apply(proposed, contract.value("preview").toObject().value("validationFingerprint").toString());
    stages.append(QJsonObject{{"stage", "configuration"}, {"success", update.success}, {"saved", update.configurationSaved},
        {"derivedSynchronized", update.derivedSynchronized}, {"verified", update.verified}, {"rolledBack", update.rolledBack}, {"error", update.error}});
    if (!update.success || !update.configurationSaved || !update.derivedSynchronized || !update.verified) failure = "Canonical configuration stage failed: " + update.error;
    if (failure.isEmpty()) {
        const QJsonObject provenance{{"actor", "agent"}, {"agentId", contract.value("intent").toObject().value("agentId")}, {"tool", requirementsProducer}};
        QString error;
        const bool recorded = ProjectMemory().recordDecision(root(model), shape.value("decisionId").toString(), shape.value("decisionTopic").toString(),
            shape.value("decisionSummary").toString(), "current", {}, &error, provenance, "source-code");
        stages.append(QJsonObject{{"stage", "decision"}, {"success", recorded}, {"error", error}});
        if (!recorded) failure = "Canonical decision stage failed: " + error;
    }
    QJsonObject hashes; const auto outputs = shape.value("outputs").toObject();
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (it.key() != shape.value("receipt").toString() && it.key() != shape.value("seal").toString()
            && !operationContextPaths().contains(it.key()) && !requirementsVerificationPaths().contains(it.key()))
            hashes.insert(it.key(), fileHash(pathFor(model, it.key())));
    const QJsonObject receipt{{"producer", requirementsProducer}, {"contractId", contract.value("contractId")}, {"operationId", contract.value("operationId")},
        {"outcome", failure.isEmpty() ? "CANONICAL_REQUIREMENTS_APPLIED" : "FAILED"}, {"error", failure}, {"outputs", hashes}, {"stages", stages},
        {"requirementsFingerprint", shape.value("baselineFingerprint")}, {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    if (!writeNew(pathFor(model, shape.value("receipt").toString()), QJsonDocument(receipt).toJson()))
        return blocked("Requirements stage output retained; immutable receipt write failed");
    const QJsonObject seal{{"producer", requirementsProducer}, {"contractId", contract.value("contractId")}, {"operationId", contract.value("operationId")},
        {"receiptFingerprint", fileHash(pathFor(model, shape.value("receipt").toString()))}};
    if (!writeNew(pathFor(model, shape.value("seal").toString()), QJsonDocument(seal).toJson()))
        return blocked("Requirements stage output retained; immutable seal write failed");
    ProjectModel reloaded; QString error;
    if (!ProjectPersistence().load(&reloaded, model.projectFilePath(), &error)) return blocked("Retained operation could not reload saved model: " + error);
    const auto context = ContextCoordinationService::generate(reloaded);
    const auto verification = VerificationServices().verify(reloaded, reloaded.generationOptions());
    if (!context.value("success").toBool() || verification.overallStatus != VerificationStatus::Pass)
        return blocked("Retained requirements operation failed final canonical context or verification");
    const auto report = postflightRequirementsOperation(reloaded, contract);
    return {{"status", failure.isEmpty() && report.value("completionState") == "VERIFIED" ? "PASS" : "FAIL"},
        {"postflight", report}, {"receipt", shape.value("receipt")}, {"error", failure}};
}

QJsonObject WorkerTaskServices::postflightRequirementsOperation(const ProjectModel& model, const QJsonObject& supplied)
{
    const auto contract = supplied.contains("contract") ? supplied.value("contract").toObject() : supplied;
    QJsonArray errors; auto unsignedContract = contract; unsignedContract.remove("contractId");
    ProjectModel beforeModel, proposed; QString error;
    if (!ProjectPersistence().fromJson(&beforeModel, contract.value("beforeModel").toObject(), &error))
        issue(errors, "REQUIREMENTS_CONTRACT_INVALID", "Original canonical model is missing or malformed.");
    beforeModel.setProjectPath(model.projectPath()); beforeModel.setProjectFilePath(model.projectFilePath());
    const auto shape = requirementsShape(beforeModel, contract.value("intent").toObject(), contract.value("operationId").toString(), &proposed, errors);
    const auto outputs = shape.value("outputs").toObject();
    QJsonArray permissions;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        permissions.append(QJsonObject{{"path", it.key()}, {"owner", it.value()}, {"actions", QJsonArray{"READ", it.key().startsWith("ARAMF_WORKER/verification/requirements/") ? "GENERATE_ONCE" : "UPDATE_DERIVED"}}});
    const QJsonObject identity{{"projectId", model.projectId()}, {"projectRoot", root(model)}, {"workerIdentity", worker(model)}};
    if (contract.value("contractKind") != requirementsKind || fingerprint(unsignedContract) != contract.value("contractId")
        || !contract.value("preflight").toObject().value("status").toString().startsWith("READY")
        || contract.value("projectBinding") != identity || contract.value("permittedFiles") != array(outputs.keys())
        || contract.value("filePermissions") != permissions || contract.value("requirements") != shape.value("baseline")
        || contract.value("requirementsFingerprint") != shape.value("baselineFingerprint")
        || contract.value("proposedModel") != ProjectPersistence().toJson(proposed)
        || ProjectPersistence().toJson(model) != ProjectPersistence().toJson(proposed))
        issue(errors, "REQUIREMENTS_CONTRACT_INVALID", "Exact derived proposal identity authority or current saved configuration differs.");
    ProjectModel saved;
    if (!ProjectPersistence().load(&saved, model.projectFilePath(), &error) || ProjectPersistence().toJson(saved) != ProjectPersistence().toJson(model))
        issue(errors, "REQUIREMENTS_CONFIGURATION_INVALID", "Saved canonical configuration does not match the actual operation result.");
    if (contract.value("sourceBindings").toObject() != operationSources(model, errors)
        || contract.value("sourceRevision") != operationRevision(model))
        issue(errors, "REQUIREMENTS_SOURCE_STALE", "Current source differs from the measured software proof.");
    validateFrozenSoftwareProof(model, contract.value("softwareProof").toObject(), errors);
    const auto proof = readObject(pathFor(model, shape.value("proof").toString()));
    const auto receipt = readObject(pathFor(model, shape.value("receipt").toString()));
    const auto seal = readObject(pathFor(model, shape.value("seal").toString()));
    if (proof.value("producer") != requirementsProducer || proof.value("operationContract") != contract
        || receipt.value("producer") != requirementsProducer || receipt.value("contractId") != contract.value("contractId")
        || receipt.value("operationId") != contract.value("operationId") || receipt.value("outcome") != "CANONICAL_REQUIREMENTS_APPLIED"
        || !receipt.value("error").toString().isEmpty() || receipt.value("requirementsFingerprint") != shape.value("baselineFingerprint")
        || seal.value("producer") != requirementsProducer || seal.value("contractId") != contract.value("contractId")
        || seal.value("operationId") != contract.value("operationId") || seal.value("receiptFingerprint") != fileHash(pathFor(model, shape.value("receipt").toString())))
        issue(errors, "REQUIREMENTS_RECEIPT_INVALID", "Exact immutable successful configuration and decision receipts are required.");
    const auto stages = receipt.value("stages").toArray();
    if (stages.size() != 2 || stages.at(0).toObject().value("stage") != "configuration"
        || !stages.at(0).toObject().value("success").toBool() || !stages.at(0).toObject().value("verified").toBool()
        || stages.at(1).toObject().value("stage") != "decision" || !stages.at(1).toObject().value("success").toBool())
        issue(errors, "REQUIREMENTS_RECEIPT_INVALID", "Both actual canonical stages must have succeeded.");
    QJsonObject hashes;
    for (auto it = outputs.begin(); it != outputs.end(); ++it)
        if (it.key() != shape.value("receipt").toString() && it.key() != shape.value("seal").toString()
            && !operationContextPaths().contains(it.key()) && !requirementsVerificationPaths().contains(it.key()))
            hashes.insert(it.key(), fileHash(pathFor(model, it.key())));
    if (receipt.value("outputs") != hashes) issue(errors, "REQUIREMENTS_RECEIPT_INVALID", "A recorded exact output changed or the output set differs.");
    const auto prefixes = contract.value("historyPrefixes").toObject();
    for (auto it = prefixes.begin(); it != prefixes.end(); ++it)
        if (!rawBytes(pathFor(model, it.key())).startsWith(QByteArray::fromBase64(it.value().toString().toLatin1())))
            issue(errors, "REQUIREMENTS_HISTORY_INVALID", "An original history prefix was replaced.", it.key());
    QString eventsError; const auto events = ProjectMemory().events(root(model), &eventsError);
    const auto originalEventBytes = QByteArray::fromBase64(prefixes.value("ARAMF_WORKER/memory/event-log.jsonl").toString().toLatin1());
    const auto newEventBytes = rawBytes(pathFor(model, "ARAMF_WORKER/memory/event-log.jsonl")).mid(originalEventBytes.size());
    int appended = 0; QJsonObject decisionEvent;
    for (const auto& line : newEventBytes.split('\n')) if (!line.trimmed().isEmpty()) { ++appended; decisionEvent = QJsonDocument::fromJson(line).object(); }
    const QJsonObject provenance{{"actor", "agent"}, {"agentId", contract.value("intent").toObject().value("agentId")}, {"tool", requirementsProducer}};
    int matches = 0; for (const auto& event : events) if (event.value("decisionId") == shape.value("decisionId")) ++matches;
    if (!eventsError.isEmpty() || appended != 1 || matches != 1 || decisionEvent.value("eventType") != "DECISION_RECORDED"
        || decisionEvent.value("decisionId") != shape.value("decisionId") || decisionEvent.value("topic") != shape.value("decisionTopic")
        || decisionEvent.value("task") != shape.value("decisionSummary") || decisionEvent.value("decisionStatus") != "current"
        || decisionEvent.value("scope") != "source-code" || decisionEvent.value("provenance") != provenance)
        issue(errors, "REQUIREMENTS_DECISION_INVALID", "Exactly one service-rendered decision with actual agent provenance is required.");
    const auto originalDecisions = QByteArray::fromBase64(prefixes.value("ARAMF_WORKER/memory/decisions.md").toString().toLatin1());
    QByteArray expectedDecisions = originalDecisions;
    expectedDecisions += QString("\n## Decision Record: %1\n\n<!-- ARAMF-DECISION -->\n- Decision-ID: %1\n- Topic: %2\n- Scope: source-code\n- Status: current\n- Superseded-By: none\n- Summary: %3\n<!-- /ARAMF-DECISION -->\n")
        .arg(shape.value("decisionId").toString(), shape.value("decisionTopic").toString(), shape.value("decisionSummary").toString()).toUtf8();
#ifdef Q_OS_WIN
    expectedDecisions.replace("\r\n", "\n"); expectedDecisions.replace("\n", "\r\n");
#endif
    if (rawBytes(pathFor(model, "ARAMF_WORKER/memory/decisions.md")) != expectedDecisions)
        issue(errors, "REQUIREMENTS_DECISION_INVALID", "Decision content or original history differs from the canonical producer output.");
    if (contract.value("baseline").toObject().value("ARAMF_WORKER/context/task-dag.json") != fileHash(pathFor(model, "ARAMF_WORKER/context/task-dag.json"))
        || contract.value("baseline").toObject().value("ARAMF_WORKER/certification/certificates.jsonl") != fileHash(pathFor(model, "ARAMF_WORKER/certification/certificates.jsonl")))
        issue(errors, "REQUIREMENTS_HISTORY_INVALID", "Requirements registration cannot alter lifecycle task DAG or certificate history.");
    const auto route = WorkerContextResolver::resolve(pathFor(model, worker(model)), {"source-code"});
    bool baselineRouted = false;
    for (const auto& item : route.value("resolvedRoutes").toArray())
        baselineRouted |= item.toObject().value("id") == "source-code"
            && item.toObject().value("taskMetadata").toObject().value("p6CodeBankRequirements") == shape.value("baseline");
    if (!baselineRouted) issue(errors, "REQUIREMENTS_ROUTE_INVALID", "Saved canonical requirements are not exposed by the active source-code route.");
    validateCanonical(model, errors);
    const auto expectedArtifacts = GenerationServices::derivedTaskArtifacts(model, model.generationOptions());
    for (auto it = expectedArtifacts.begin(); it != expectedArtifacts.end(); ++it) {
        auto actual = readObject(pathFor(model, it.key())); actual.remove("_file"); auto expected = it.value().toObject(); expected.remove("_file");
        if (actual != expected) issue(errors, "REQUIREMENTS_DERIVED_INVALID", "Canonical derived product differs from its producer.", it.key());
    }
    const auto verified = VerificationServices().verify(model, model.generationOptions(), false);
    auto summary = readObject(pathFor(model, requirementsVerificationPaths().at(0))); summary.remove("_file");
    auto report = readObject(pathFor(model, requirementsVerificationPaths().at(1))); report.remove("_file"); report.remove("checkedAt");
    auto expectedReport = verified.evidence; expectedReport.remove("checkedAt");
    if (verified.overallStatus != VerificationStatus::Pass || summary != verified.summary || report != expectedReport)
        issue(errors, "REQUIREMENTS_VALIDATION_INVALID", "Current pure canonical validation and persisted producer reports disagree.");
    QString memoryError;
    auto expectedMemory = ProjectMemory().validate(root(model), &memoryError, false);
    auto actualMemory = readObject(pathFor(model, "ARAMF_WORKER/memory/memory-consistency-validation.json"));
    for (const QString& key : {QStringLiteral("checkedAt"), QStringLiteral("_file")}) { expectedMemory.remove(key); actualMemory.remove(key); }
    if (!memoryError.isEmpty() || expectedMemory.value("status") != "PASS" || actualMemory != expectedMemory)
        issue(errors, "REQUIREMENTS_VALIDATION_INVALID", "Current pure memory validation differs from the final canonical report.");
    operationBoundary(model, contract.value("baseline").toObject(), operationSnapshot(model, errors), outputs.keys(), errors);
    const bool valid = state(errors).startsWith("READY");
    return {{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractId", contract.value("contractId")},
        {"status", valid ? "PASS" : "FAIL"}, {"completionState", valid ? "VERIFIED" : "BLOCKED"}, {"errors", errors},
        {"missingEvidence", valid ? QJsonArray{} : QJsonArray{"canonical-requirements-receipts"}}, {"requirementsFingerprint", shape.value("baselineFingerprint")}};
}


namespace {
QStringList protectedGrantOutputs()
{
    return {"ARAMF_WORKER/memory/event-log.jsonl", "ARAMF_WORKER/memory/memory-manifest.json",
        "ARAMF_WORKER/memory/metrics.json", "ARAMF_WORKER/memory/current-state.md",
        "ARAMF_WORKER/memory/cold-start-validation.json", "ARAMF_WORKER/memory/memory-consistency-validation.json",
        "ARAMF_WORKER/context/context-index.json", "ARAMF_WORKER/context/compressed-context.json",
        "ARAMF_WORKER/context/freshness.json", "ARAMF_WORKER/context/agent-adapters.json"};
}
QJsonObject grantServiceProvenance()
{
    return {{"actor", "system"}, {"agentId", "none"}, {"tool", "WorkerTaskServices"}};
}
bool protectedMutationReady(const ProjectModel& model, QJsonObject* before, QString* error)
{
    if (!QFileInfo::exists(pathFor(model, "ARAMF_WORKER/context/task-dag.json"))) {
        *error = "Protected grant refresh requires an existing task DAG before mutation"; return false;
    }
    QJsonArray errors;
    for (const auto& path : protectedGrantOutputs())
        if (!operationSafePath(model, path)) issue(errors, "PROTECTED_GRANT_PATH_INVALID", "Unsafe exact recorder output.", path);
    validateCanonical(model, errors);
    if (!state(errors).startsWith("READY") || !currentGrantMemory(model, error)) {
        if (error->isEmpty()) *error = "Current canonical project validation is required before protected authority mutation";
        return false;
    }
    *before = operationSnapshot(model, errors);
    if (!state(errors).startsWith("READY")) { *error = "Cannot observe the exact protected authority baseline"; return false; }
    return true;
}
bool refreshProtectedMutation(const ProjectModel& model, QJsonObject* cold, QJsonObject* memory, QString* error, bool context)
{
    *cold = ProjectMemory().validateColdStart(root(model), error);
    if (!error->isEmpty() || cold->value("status") != "PASS") return false;
    *memory = ProjectMemory().validate(root(model), error, true);
    if (!error->isEmpty() || memory->value("status") != "PASS") return false;
    if (context && !ContextCoordinationService::generate(model).value("success").toBool()) {
        *error = "Canonical context refresh failed after protected authority mutation"; return false;
    }
    return true;
}
bool protectedMutationReadback(const ProjectModel& model, const QJsonObject& before, QString* error)
{
    QJsonArray errors; const auto after = operationSnapshot(model, errors);
    operationBoundary(model, before, after, protectedGrantOutputs(), errors);
    if (before.value("ARAMF_WORKER/context/task-dag.json") != after.value("ARAMF_WORKER/context/task-dag.json"))
        issue(errors, "PROTECTED_GRANT_DAG_CHANGED", "Protected grant recording cannot create or alter the task DAG.");
    validateCanonical(model, errors);
    if (!state(errors).startsWith("READY") || !currentGrantMemory(model, error)) {
        if (error->isEmpty()) *error = "Protected authority readback or exact output boundary failed";
        return false;
    }
    return true;
}

const QStringList repairProfileIds{"task-p6-activation-campaign-isolation/v1", "task-p6-activation-campaign-isolation/v2"};
const QString retainedCampaignPath = "test_update/campaign.json";
QString repairCompletionPath(const QString& id)
{
    const QUuid uuid(id);
    return !uuid.isNull() && uuid.toString(QUuid::WithoutBraces) == id
        ? "build/aramf-repair-authority/" + id + "/issuance.json" : QString{};
}
QJsonObject repairCompletion(const ProjectModel& model, const QJsonObject& grant,
                            const QJsonObject& authority, const QJsonObject& validation)
{
    return {{"schemaVersion", 1}, {"producer", "WorkerTaskServices/repair-authority/v2"}, {"status", "ISSUANCE_COMPLETED"},
        {"authorizationId", grant.value("authorizationId")}, {"projectId", model.projectId()}, {"projectRoot", root(model)},
        {"workerIdentity", worker(model)}, {"authorityEventId", authority.value("eventId")},
        {"validationEventId", validation.value("eventId")}, {"grantFingerprint", fingerprint(grant)}, {"validationTimestamp", validation.value("timestamp")}};
}
QStringList repairSourceFiles()
{
    return {"src/core/WorkerTaskServices.cpp", "tests/WorkerTaskTests.cpp", "test_update/runner/main.cpp",
        "src/foundations/F2/IdentityTrustFoundation.h", "src/foundations/F2/IdentityTrustFoundation.cpp",
        "tests/FoundationIndependenceTests.cpp", "src/core/CertificationFreshness.cpp", "tests/CertificationFreshnessTests.cpp"};
}
QStringList repairSourceFiles(const QString& profileId)
{
    if (!repairProfileIds.contains(profileId)) return {};
    auto files = repairSourceFiles();
    if (profileId == "task-p6-activation-campaign-isolation/v2") files << "tests/FoundationCertificationTests.cpp";
    return files;
}
bool exactKeys(const QJsonObject& object, const QStringList& expected)
{
    const auto keys = object.keys();
    return QSet<QString>(keys.cbegin(), keys.cend()) == QSet<QString>(expected.cbegin(), expected.cend());
}
QJsonObject retainedEvidenceFiles(const ProjectModel& model, const QJsonObject& contract, QString* error)
{
    QJsonObject files;
    const QString directory = evidenceDirectory(contract);
    if (directory.isEmpty() || !evidencePathAllowed(model, directory + "/probe.json")) {
        *error = "Origin evidence directory is unsafe"; return {};
    }
    QDirIterator entries(pathFor(model, directory), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        const QString path = QDir(root(model)).relativeFilePath(entries.next());
        if (!operationSafePath(model, path) || entries.fileInfo().isSymLink() || entries.fileInfo().isJunction()) {
            *error = "Origin evidence path is unsafe"; return {};
        }
        const QString hash = fileHash(pathFor(model, path));
        if (!QRegularExpression("^[a-f0-9]{64}$").match(hash).hasMatch()) { *error = "Origin evidence is unreadable"; return {}; }
        files.insert(path, hash);
    }
    return files;
}
bool readySourceAuthority(const ProjectModel& model, const QJsonObject& value)
{
    auto unsignedValue = value; unsignedValue.remove("contractId");
    if (value.value("schemaVersion").toInt() != 1 || value.value("authority") != "DERIVED"
        || value.value("contractId").toString() != fingerprint(unsignedValue)
        || !value.value("preflight").toObject().value("status").toString().startsWith("READY")
        || value.contains("reauthorization") || value.contains("contractKind")
        || value.value("binding").toObject().value("projectId") != model.projectId()
        || value.value("binding").toObject().value("projectRoot") != root(model)
        || value.value("binding").toObject().value("workerIdentity") != worker(model)) return false;
    const auto task = WorkerTaskRequest::fromJson(value.value("request").toObject());
    if (task.destructive || !task.protectedGrantId.isEmpty() || !task.relocations.isEmpty()) return false;
    const auto derived = WorkerTaskServices::prepare(model, task);
    return derived.value("preflight").toObject().value("status").toString().startsWith("READY")
        && semantic(derived.value("permittedFiles")) == semantic(value.value("permittedFiles"));
}

QJsonObject repairSuccessorGrant(const ProjectModel& model, const QString& id,
                                 const QJsonObject* successor, QString* error)
{
    const auto deny = [error](const QString& reason) { *error = reason; return QJsonObject{}; };
    const auto events = ProjectMemory().events(root(model), error);
    if (!error->isEmpty()) return {};
    QJsonObject authority, grant, validation, bound;
    int authorities = 0, validations = 0, bindings = 0, sameSuccessor = 0;
    for (const auto& event : events) {
        if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == id) {
            ++authorities; authority = event; grant = event.value("taskReauthorizationGrant").toObject();
        }
        if (event.value("authorizationId") == id) {
            if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED") { ++validations; validation = event; }
            if (event.value("eventType") == "TASK_REAUTHORIZATION_BOUND") { ++bindings; bound = event; }
            if (event.value("eventType") == "TASK_REAUTHORIZATION_REVOKED" || event.value("eventType") == "TASK_REAUTHORIZATION_CONSUMED")
                return deny("Repair authority was revoked or consumed");
        }
        if (successor && event.value("eventType") == "TASK_REAUTHORIZATION_BOUND"
            && event.value("contractId") == successor->value("contractId")) ++sameSuccessor;
    }
    const QStringList keys{"schemaVersion", "authorizationId", "projectId", "projectRoot", "workerIdentity", "task", "sourceFiles", "canonicalInputs",
        "operations", "approvedPathFingerprints", "binding", "issuedAt", "expiresAt", "originContractId", "originContractFingerprint",
        "originPostflightFingerprint", "origins", "sourceAuthorities", "retainedInputs", "delegatedOriginalInputs", "historyPrefixes", "instruction", "recordingAgentId", "issuanceCompletionPath"};
    const auto issued = QDateTime::fromString(grant.value("issuedAt").toString(), Qt::ISODateWithMs);
    const auto expires = QDateTime::fromString(grant.value("expiresAt").toString(), Qt::ISODateWithMs);
    const auto now = QDateTime::currentDateTimeUtc();
    const auto task = grant.value("task").toObject();
    const QString selectedProfile = task.value("acceptanceProfile").toString();
    const auto sourceFiles = strings(grant.value("sourceFiles"));
    if (authorities != 1 || validations != 1 || bindings != (successor ? 1 : 0) || (successor && sameSuccessor != 1)
        || !exactKeys(grant, keys) || grant.value("schemaVersion").toInt() != 2
        || authority.value("eventType") != "TASK_REPAIR_SCOPE_AUTHORIZED"
        || !IdentityTrustFoundation::isExplicitTemporaryGovernanceRepairAuthority(grant.value("instruction").toString())
        || grant.value("recordingAgentId").toString().trimmed().isEmpty()
        || authority.value("provenance").toObject() != QJsonObject{{"actor", "agent"}, {"agentId", grant.value("recordingAgentId")}, {"tool", "WorkerTaskServices"}}
        || authority.value("instruction") != grant.value("instruction") || authority.value("persistentPolicy").toBool(true)
        || semantic(authority.value("affectedFiles")) != semantic(array(sourceFiles))
        || grant.value("projectId") != model.projectId() || grant.value("projectRoot") != root(model) || grant.value("workerIdentity") != worker(model)
        || grant.value("binding") != binding(model) || !issued.isValid() || !expires.isValid() || issued > now || expires <= now
        || issued.msecsTo(expires) <= 0 || issued.msecsTo(expires) > 86400000
        || !repairProfileIds.contains(selectedProfile) || semantic(task.value("files")) != semantic(array(repairSourceFiles(selectedProfile)))
        || task.value("type") != "implementation" || semantic(task.value("scopes")) != semantic(QJsonArray{"source-code", "tests"})
        || !task.value("history").toBool() || task.value("destructive").toBool(true)
        || semantic(task.value("definitionOfDone")) != semantic(WorkerTaskServices::acceptanceProfile(selectedProfile).value("requirements"))
        || sourceFiles != strings(array(repairSourceFiles(selectedProfile))) || !grant.value("canonicalInputs").toArray().isEmpty()
        || validation.value("authorityEventId") != authority.value("eventId") || validation.value("grantFingerprint") != fingerprint(grant)
        || validation.value("provenance").toObject() != grantServiceProvenance()
        || validation.value("postMemoryValidation").toObject().value("status") != "PASS"
        || validation.value("postColdStartValidation").toObject().value("status") != "PASS")
        return deny("Repair authority shape, actual provenance, exact scope, validation or lifetime is invalid");
    const QString completionPath = repairCompletionPath(id);
    const auto completionBytes = rawBytes(pathFor(model, completionPath));
    const auto completion = QJsonDocument::fromJson(completionBytes).object();
    const auto validatedAt = QDateTime::fromString(completion.value("validationTimestamp").toString(), Qt::ISODateWithMs);
    if (completionPath.isEmpty() || grant.value("issuanceCompletionPath") != completionPath
        || !evidencePathAllowed(model, completionPath) || !validatedAt.isValid()
        || validatedAt.toSecsSinceEpoch() < issued.toSecsSinceEpoch() || validatedAt > expires || validatedAt > now
        || completion != repairCompletion(model, grant, authority, validation)
        || completionBytes != QJsonDocument(completion).toJson())
        return deny("Repair issuance has no exact retained final completion receipt");
    const auto hashes = grant.value("approvedPathFingerprints").toObject(), operations = grant.value("operations").toObject();
    if (!exactKeys(hashes, sourceFiles) || !exactKeys(operations, sourceFiles + QStringList{retainedCampaignPath})
        || operations.value(retainedCampaignPath) != "PRESERVE") return deny("Repair observations cannot grant write permission");
    for (const auto& file : sourceFiles) {
        if (operations.value(file) != "MODIFY" || !operationSafePath(model, file)
            || hashes.value(file).toString() != (successor ? successor->value("baseline").toObject().value(file).toString() : fileHash(pathFor(model, file))))
            return deny("Approved repair source or successor baseline differs");
    }
    const auto retained = grant.value("retainedInputs").toObject();
    if (!exactKeys(retained, {retainedCampaignPath})) return deny("Only the fixed failed campaign artifact may be retained");
    const auto campaign = retained.value(retainedCampaignPath).toObject();
    if (!exactKeys(campaign, {"bytesBase64", "fingerprint"}) || !operationSafePath(model, retainedCampaignPath)
        || digest(QByteArray::fromBase64(campaign.value("bytesBase64").toString().toLatin1())) != campaign.value("fingerprint")
        || rawBytes(pathFor(model, retainedCampaignPath)) != QByteArray::fromBase64(campaign.value("bytesBase64").toString().toLatin1()))
        return deny("Retained failed campaign bytes changed");
    const auto history = grant.value("historyPrefixes").toObject();
    const QStringList historicalPaths{workerPath(model, "memory/event-log.jsonl"), workerPath(model, "certification/certificates.jsonl")};
    if (!exactKeys(history, historicalPaths)) return deny("Exact original ledger observations are required");
    for (const auto& path : historicalPaths) {
        const auto original = history.value(path).toObject();
        const QByteArray prefix = QByteArray::fromBase64(original.value("bytesBase64").toString().toLatin1());
        if (!exactKeys(original, {"exists", "bytesBase64", "fingerprint"}) || !operationSafePath(model, path)
            || digest(prefix) != original.value("fingerprint")
            || (!original.value("exists").toBool() && QFileInfo::exists(pathFor(model, path)))
            || (original.value("exists").toBool() && (!QFileInfo::exists(pathFor(model, path)) || !rawBytes(pathFor(model, path)).startsWith(prefix))))
            return deny("Original event or certificate history was replaced");
    }
    QSet<QString> authorizedSources;
    const auto sourceAuthorities = grant.value("sourceAuthorities").toArray();
    if (sourceAuthorities.isEmpty()) return deny("Actual original READY source authorities are required");
    QSet<QString> authorityIds;
    for (const auto& item : sourceAuthorities) {
        const auto value = item.toObject();
        if (!readySourceAuthority(model, value) || authorityIds.contains(value.value("contractId").toString()))
            return deny("Source authority is forged, duplicated or outside current canonical routing");
        authorityIds.insert(value.value("contractId").toString());
        for (const auto& path : strings(value.value("permittedFiles"))) {
            if (!sourceFiles.contains(path)) return deny("Source authority exceeds the fixed minimum repair scope");
            authorizedSources.insert(path);
        }
    }
    if (authorizedSources != QSet<QString>(sourceFiles.cbegin(), sourceFiles.cend())) return deny("Every repair source requires originating READY authority");
    const auto origins = grant.value("origins").toArray();
    QSet<QString> originIds, delegatedPaths;
    if (origins.isEmpty()) return deny("Retained failed origin is absent");
    for (const auto& item : origins) {
        const auto origin = item.toObject(), contract = origin.value("contract").toObject(), report = origin.value("report").toObject();
        const QString originId = contract.value("contractId").toString();
        auto unsignedContract = contract; unsignedContract.remove("contractId");
        if (!exactKeys(origin, {"contract", "report", "evidenceFiles"}) || originIds.contains(originId)
            || originId != fingerprint(unsignedContract) || contract.value("authority") != "DERIVED"
            || !contract.value("preflight").toObject().value("status").toString().startsWith("READY")
            || contract.value("binding").toObject().value("projectId") != model.projectId()
            || contract.value("binding").toObject().value("projectRoot") != root(model)
            || report.value("authority") != "DERIVED" || report.value("contractId") != originId || report.value("status") != "FAIL"
            || origin.value("evidenceFiles") != retainedEvidenceFiles(model, contract, error) || !error->isEmpty())
            return deny("Original contract, FAIL report or immutable native evidence has changed");
        originIds.insert(originId);
        for (const auto& service : report.value("serviceChanges").toArray()) delegatedPaths.insert(service.toObject().value("path").toString());
    }
    const auto first = origins.first().toObject();
    if (first.value("contract").toObject().value("contractId") != grant.value("originContractId")
        || fingerprint(first.value("contract").toObject()) != grant.value("originContractFingerprint")
        || fingerprint(first.value("report").toObject()) != grant.value("originPostflightFingerprint")) return deny("Primary origin linkage differs");
    const auto delegated = grant.value("delegatedOriginalInputs").toObject();
    const auto delegatedKeys = delegated.keys();
    if (QSet<QString>(delegatedKeys.cbegin(), delegatedKeys.cend()) != delegatedPaths) return deny("Delegated observations differ from the reproduced original reports");
    for (auto it = delegated.begin(); it != delegated.end(); ++it) {
        const auto value = it.value().toObject(); const QByteArray original = QByteArray::fromBase64(value.value("bytesBase64").toString().toLatin1());
        const auto owner = fileRole(model, it.key(), WorkerTaskServices::mutationPolicy(model));
        if (!exactKeys(value, {"service", "bytesBase64", "fingerprint"}) || !operationSafePath(model, it.key())
            || owner.value("writer") != value.value("service") || digest(original) != value.value("fingerprint"))
            return deny("Canonical producer observation is invalid");
        if ((it.key().endsWith("/event-log.jsonl") || it.key().endsWith("/certificates.jsonl"))
            && !rawBytes(pathFor(model, it.key())).startsWith(original)) return deny("Original canonical history prefix was rewritten");
    }
    if (successor) {
        const auto link = successor->value("reauthorization").toObject(); auto request = task; request.insert("reauthorizationId", id);
        if (!exactKeys(link, {"authorizationId", "authorityEventId", "originContractId", "originContractFingerprint", "originPostflightFingerprint", "sourceFiles", "canonicalInputs", "operations"})
            || link.value("authorityEventId") != authority.value("eventId") || link.value("authorizationId") != id
            || bound.value("authorityEventId") != authority.value("eventId") || bound.value("contractId") != successor->value("contractId")
            || bound.value("projectId") != model.projectId() || bound.value("provenance").toObject() != grantServiceProvenance()
            || bound.value("originContractId") != grant.value("originContractId")
            || bound.value("originContractFingerprint") != grant.value("originContractFingerprint")
            || bound.value("originPostflightFingerprint") != grant.value("originPostflightFingerprint")
            || successor->value("request") != request || semantic(successor->value("permittedFiles")) != semantic(array(sourceFiles)))
            return deny("Successor request or unique audit binding differs");
        for (const QString& key : {QStringLiteral("originContractId"), QStringLiteral("originContractFingerprint"), QStringLiteral("originPostflightFingerprint"),
                 QStringLiteral("sourceFiles"), QStringLiteral("canonicalInputs"), QStringLiteral("operations")})
            if (link.value(key) != grant.value(key)) return deny("Successor link widened original authority");
    }
    auto result = grant; result.insert("authorityEventId", authority.value("eventId")); result.insert("scope", array(sourceFiles));
    return result;
}

QJsonObject authorizeRepairSuccessor(const ProjectModel& model, const QJsonObject& request)
{
    const auto blocked = [](const QString& message) { return QJsonObject{{"status", "BLOCKED"}, {"error", message}}; };
    if (!exactKeys(request, {"task", "sourceAuthorities", "origins", "instruction", "recordingAgentId", "validityMinutes"})
        || !IdentityTrustFoundation::isExplicitTemporaryGovernanceRepairAuthority(request.value("instruction").toString())
        || request.value("recordingAgentId").toString().trimmed().isEmpty()
        || !request.value("validityMinutes").isDouble() || request.value("validityMinutes").toInt() < 1 || request.value("validityMinutes").toInt() > 1440)
        return blocked("Explicit F2-recognized temporary directive, actual recording agent and bounded exact request are required");
    const auto taskJson = request.value("task").toObject(); const auto task = WorkerTaskRequest::fromJson(taskJson);
    if (semantic(task.toJson()) != semantic(taskJson) || !repairProfileIds.contains(task.acceptanceProfile) || task.destructive || !task.relocations.isEmpty()
        || task.type != "implementation" || semantic(array(task.scopes)) != semantic(QJsonArray{"source-code", "tests"}) || !task.history
        || !task.protectedGrantId.isEmpty() || !task.reauthorizationId.isEmpty()
        || semantic(array(task.files)) != semantic(array(repairSourceFiles(task.acceptanceProfile)))
        || semantic(array(task.definitionOfDone)) != semantic(WorkerTaskServices::acceptanceProfile(task.acceptanceProfile).value("requirements")))
        return blocked("Only the explicit minimum combined repair profile is supported");
    QLockFile lock(QDir::tempPath() + "/aramf-reauthorization-" + digest(root(model).toUtf8()) + ".lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Repair authorization is busy");
    QString error; QJsonObject before;
    if (!protectedMutationReady(model, &before, &error)) return blocked(error);
    const auto events = ProjectMemory().events(root(model), &error);
    if (!error.isEmpty()) return blocked(error);
    QSet<QString> sourceUnion, authorityIds; QJsonArray authorities;
    for (const auto& item : request.value("sourceAuthorities").toArray()) {
        auto authority = item.toObject(); if (authority.contains("contract")) authority = authority.value("contract").toObject();
        if (!readySourceAuthority(model, authority) || authorityIds.contains(authority.value("contractId").toString())) return blocked("Invalid or duplicate READY source authority");
        authorityIds.insert(authority.value("contractId").toString()); authorities.append(authority);
        for (const auto& file : strings(authority.value("permittedFiles"))) sourceUnion.insert(file);
    }
    const auto files = strings(array(repairSourceFiles(task.acceptanceProfile)));
    if (sourceUnion != QSet<QString>(files.cbegin(), files.cend())) return blocked("Original READY authorities must cover exactly the selected fixed repair and verification sources");
    QJsonArray origins; QJsonObject delegated; QSet<QString> originIds;
    for (const auto& item : request.value("origins").toArray()) {
        const auto input = item.toObject(); auto origin = input.value("contract").toObject(); if (origin.contains("contract")) origin = origin.value("contract").toObject();
        const auto report = input.value("report").toObject(); const QString id = origin.value("contractId").toString();
        auto unsignedOrigin = origin; unsignedOrigin.remove("contractId");
        if (!exactKeys(input, {"contract", "report"}) || originIds.contains(id) || id != fingerprint(unsignedOrigin)
            || !origin.value("preflight").toObject().value("status").toString().startsWith("READY")
            || report.value("contractId") != id || report.value("status") != "FAIL"
            || semantic(WorkerTaskServices::postflight(model, origin, report.value("acceptedEvidence").toArray())) != semantic(report))
            return blocked("Every original failed report must reproduce from the real current service");
        for (const auto& event : events) {
            const auto old = event.value("taskReauthorizationGrant").toObject();
            if (old.value("originContractId") == id) return blocked("An origin already has retained authority; replacement is forbidden");
            for (const auto& prior : old.value("origins").toArray())
                if (prior.toObject().value("contract").toObject().value("contractId") == id) return blocked("An origin already has retained authority; replacement is forbidden");
        }
        QSet<QString> servicePaths;
        for (const auto& value : report.value("serviceChanges").toArray()) {
            const auto service = value.toObject(); const QString path = service.value("path").toString();
            const auto owner = fileRole(model, path, WorkerTaskServices::mutationPolicy(model));
            if (servicePaths.contains(path) || service.value("validation") != "canonical-producer-and-boundary"
                || service.value("service") != owner.value("writer") || !operationSafePath(model, path)) return blocked("Invalid canonical observation in origin report");
            servicePaths.insert(path); const QByteArray bytes = rawBytes(pathFor(model, path));
            delegated.insert(path, QJsonObject{{"service", service.value("service")}, {"bytesBase64", QString::fromLatin1(bytes.toBase64())}, {"fingerprint", digest(bytes)}});
        }
        for (const auto& path : strings(report.value("modifiedFiles")))
            if (!files.contains(path) && path != retainedCampaignPath && !servicePaths.contains(path)) return blocked("Uncovered original modification: " + path);
        const auto evidence = retainedEvidenceFiles(model, origin, &error);
        if (!error.isEmpty()) return blocked(error);
        origins.append(QJsonObject{{"contract", origin}, {"report", report}, {"evidenceFiles", evidence}}); originIds.insert(id);
    }
    if (origins.isEmpty() || !operationSafePath(model, retainedCampaignPath) || !QFileInfo(pathFor(model, retainedCampaignPath)).isFile()) return blocked("Actual failed origins and retained campaign bytes are required");
    QJsonObject hashes, operations;
    for (const auto& file : files) {
        const QString hash = fileHash(pathFor(model, file));
        if (!operationSafePath(model, file) || !QRegularExpression("^[a-f0-9]{64}$").match(hash).hasMatch()) return blocked("Repair source is unsafe or unreadable");
        hashes.insert(file, hash); operations.insert(file, "MODIFY");
    }
    operations.insert(retainedCampaignPath, "PRESERVE"); const QByteArray campaign = rawBytes(pathFor(model, retainedCampaignPath));
    const QJsonObject retained{{retainedCampaignPath, QJsonObject{{"bytesBase64", QString::fromLatin1(campaign.toBase64())}, {"fingerprint", digest(campaign)}}}};
    const auto first = origins.first().toObject(); const auto now = QDateTime::currentDateTimeUtc(); const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject history;
    for (const auto& path : QStringList{workerPath(model, "memory/event-log.jsonl"), workerPath(model, "certification/certificates.jsonl")}) {
        if (!operationSafePath(model, path)) return blocked("Historical ledger path is unsafe");
        const QByteArray bytes = rawBytes(pathFor(model, path));
        history.insert(path, QJsonObject{{"exists", QFileInfo::exists(pathFor(model, path))}, {"bytesBase64", QString::fromLatin1(bytes.toBase64())}, {"fingerprint", digest(bytes)}});
    }
    const QJsonObject grant{{"schemaVersion", 2}, {"authorizationId", id}, {"projectId", model.projectId()}, {"projectRoot", root(model)}, {"workerIdentity", worker(model)},
        {"task", task.toJson()}, {"sourceFiles", array(files)}, {"canonicalInputs", QJsonArray{}}, {"operations", operations}, {"approvedPathFingerprints", hashes}, {"binding", binding(model)},
        {"issuedAt", now.toString(Qt::ISODateWithMs)}, {"expiresAt", now.addSecs(request.value("validityMinutes").toInt() * 60).toString(Qt::ISODateWithMs)},
        {"originContractId", first.value("contract").toObject().value("contractId")}, {"originContractFingerprint", fingerprint(first.value("contract").toObject())},
        {"originPostflightFingerprint", fingerprint(first.value("report").toObject())}, {"origins", origins}, {"sourceAuthorities", authorities},
        {"retainedInputs", retained}, {"delegatedOriginalInputs", delegated}, {"historyPrefixes", history}, {"instruction", request.value("instruction")}, {"recordingAgentId", request.value("recordingAgentId")},
        {"issuanceCompletionPath", repairCompletionPath(id)}};
    const QString completionPath = repairCompletionPath(id);
    if (completionPath.isEmpty() || !evidencePathAllowed(model, completionPath) || QFileInfo::exists(pathFor(model, completionPath)))
        return blocked("The exact new retained completion path must be safe ignored untracked and absent before issuance");
    const QJsonObject fields{{"taskReauthorizationGrant", grant}, {"instruction", request.value("instruction")}, {"persistentPolicy", false}, {"affectedFiles", array(files)},
        {"authorityKind", "F2_EXPLICIT_TEMPORARY_REPAIR_DIRECTIVE"}, {"retainedObservationPaths", QJsonArray{retainedCampaignPath}},
        {"provenance", QJsonObject{{"actor", "agent"}, {"agentId", request.value("recordingAgentId")}, {"tool", "WorkerTaskServices"}}}};
    if (!ProjectMemory().appendEvent(root(model), "TASK_REPAIR_SCOPE_AUTHORIZED", "Temporary exact successor authority recorded from the actual user directive; no new human ADMIN_OVERRIDE", fields, &error))
        return blocked("Any partial repair authorization remains retained and inactive: " + error);
    QJsonObject authority; const auto recorded = ProjectMemory().events(root(model), &error);
    for (const auto& event : recorded) if (event.value("taskReauthorizationGrant").toObject().value("authorizationId") == id) authority = event;
    QJsonObject cold, memory;
    if (!error.isEmpty() || authority.isEmpty() || !refreshProtectedMutation(model, &cold, &memory, &error, false)) return blocked("Repair issuance retained without completed validation: " + error);
    const QJsonObject validation{{"authorizationId", id}, {"authorityEventId", authority.value("eventId")}, {"grantFingerprint", fingerprint(grant)},
        {"postColdStartValidation", cold}, {"postMemoryValidation", memory}, {"provenance", grantServiceProvenance()}};
    if (!ProjectMemory().appendEvent(root(model), "TASK_REAUTHORIZATION_AUTHORIZED", "Exact temporary successor authority validation completed", validation, &error)
        || !refreshProtectedMutation(model, &cold, &memory, &error, true) || !protectedMutationReadback(model, before, &error)) return blocked("Repair authority retained but final canonical readback failed: " + error);
    QJsonObject validationEvent;
    for (const auto& event : ProjectMemory().events(root(model), &error))
        if (event.value("eventType") == "TASK_REAUTHORIZATION_AUTHORIZED" && event.value("authorizationId") == id) validationEvent = event;
    if (!error.isEmpty() || validationEvent.isEmpty() || !evidencePathAllowed(model, completionPath))
        return blocked("Repair issuance remains inactive before its final completion receipt: " + error);
    if (QDateTime::currentDateTimeUtc() >= QDateTime::fromString(grant.value("expiresAt").toString(), Qt::ISODateWithMs))
        return blocked("Repair issuance expired before its final completion receipt");
    const auto completion = repairCompletion(model, grant, authority, validationEvent);
    const QString absolute = pathFor(model, completionPath);
    if (!QDir().mkpath(QFileInfo(absolute).absolutePath())) return blocked("Repair issuance remains inactive: final receipt storage is unavailable");
    QTemporaryFile staged(QFileInfo(absolute).absolutePath() + "/.issuance-XXXXXX"); staged.setAutoRemove(false);
    const QByteArray completionBytes = QJsonDocument(completion).toJson();
    if (!staged.open() || staged.write(completionBytes) != completionBytes.size() || !staged.flush())
        return blocked("Repair issuance remains inactive: incomplete receipt staging is retained");
    staged.close();
    if (!staged.rename(absolute)) return blocked("Repair issuance remains inactive: create-only receipt commit failed and staging is retained");
    const auto checked = repairSuccessorGrant(model, id, nullptr, &error);
    if (checked.isEmpty()) return {{"status", "BLOCKED"}, {"authorizationId", id}, {"issuanceCommitted", true},
        {"completionReceipt", completionPath}, {"error", "Final receipt was committed; current authority readback failed: " + error}};
    return {{"status", "PASS"}, {"authorizationId", id}, {"authorityEventId", authority.value("eventId")}, {"expiresAt", grant.value("expiresAt")},
        {"issuanceCommitted", true}, {"completionReceipt", completionPath},
        {"retention", "Retain the final completion receipt with immutable authority history and proof archives; deleting disposable build storage makes authority unusable"},
        {"retainedObservationPaths", QJsonArray{retainedCampaignPath}}, {"canonicalOutputs", array(protectedGrantOutputs() + QStringList{completionPath})}};
}
}

QJsonObject WorkerTaskServices::issueP6BuildGrantFromEvent(const ProjectModel& model, const WorkerTaskRequest& task,
                                                         const QString& authorityEventId, const QString& expiresAt)
{
    const auto blocked = [](const QString& message) { return QJsonObject{{"status", "BLOCKED"}, {"error", message}}; };
    const QStringList exact{"CMakeLists.txt", "src/processes/P6/CodeBank.h", "src/processes/P6/CodeBank.cpp",
        "src/processes/P6/CodeBankStorage.cpp", "src/processes/P6/CodeBankVerification.cpp", "src/processes/P6/README.md", "tests/P6CodeBankTests.cpp"};
    const auto now = QDateTime::currentDateTimeUtc(); const auto expires = QDateTime::fromString(expiresAt, Qt::ISODate);
    if (!model.workerNameSuffix().isEmpty() || AramfPaths::runtimeWorkerDirectoryName() != worker(model)
        || !task.protectedGrantId.isEmpty() || task.destructive || !task.relocations.isEmpty()
        || array(task.files) != array(exact) || !task.goal.contains("P6") || task.definitionOfDone.isEmpty()
        || !expires.isValid() || now.secsTo(expires) <= 0 || now.secsTo(expires) > 86400 || !operationSafePath(model, "CMakeLists.txt"))
        return blocked("An exact seven-file P6 task, safe protected file and fresh lifetime within 24 hours are required");
    QLockFile lock(QDir::tempPath() + "/aramf-protected-grant-" + digest(root(model).toUtf8()) + ".lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Grant authority is busy");
    QString error; const auto events = ProjectMemory().events(root(model), &error); const auto authority = uniqueEvent(events, authorityEventId);
    if (!error.isEmpty() || !scopedP6Authority(authority)) return blocked("A unique original human Admin event containing the complete recognized narrow P6 authorization is required");
    for (const auto& event : events) {
        if ((event.value("eventType") == "PROTECTED_GRANT_ISSUED" && event.value("authorityEventId") == authorityEventId)
            || (event.value("eventType") == "PROTECTED_AUTHORITY_REVOKED" && event.value("authorityEventId") == authorityEventId)
            || event.value("protectedPathGrant").toObject().value("requestFingerprint") == grantRequestFingerprint(task))
            return blocked("Original authority is revoked or already used; retained partial expired and consumed grants are never automatically replaced");
    }
    QJsonObject before; if (!protectedMutationReady(model, &before, &error)) return blocked(error);
    const QByteArray cmake = rawBytes(pathFor(model, "CMakeLists.txt"));
    if (cmake.isEmpty() || cmake.contains("aramf_p6_tests")) return blocked("The exact readable unregistered CMake baseline is required");
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject grant{{"schemaVersion", 2}, {"grantId", id}, {"projectId", model.projectId()}, {"projectRoot", root(model)},
        {"workerIdentity", worker(model)}, {"requestFingerprint", grantRequestFingerprint(task)}, {"path", "CMakeLists.txt"}, {"operation", "MODIFY"},
        {"purpose", "P6_BUILD_TEST_REGISTRATION"}, {"issuedAt", now.toString(Qt::ISODate)}, {"expiresAt", expiresAt},
        {"baseFingerprint", digest(cmake)}, {"resultFingerprint", digest(cmake + p6Registration())}, {"registrationFingerprint", digest(p6Registration())},
        {"authorityEventId", authorityEventId}, {"authorityEventFingerprint", exactEventHash(authority)}};
    const QJsonObject fields{{"protectedPathGrant", grant}, {"authorityEventId", authorityEventId},
        {"authorityEventFingerprint", exactEventHash(authority)}, {"provenance", grantServiceProvenance()}};
    if (!ProjectMemory().appendEvent(root(model), "PROTECTED_GRANT_ISSUED", "First exact P6 grant derived from retained scoped Admin authority", fields, &error))
        return blocked("Grant append failed; any durable partial event remains inactive and must not be reissued: " + error);
    const auto issuedEvents = ProjectMemory().events(root(model), &error); QJsonObject issuance; int matches = 0;
    for (const auto& event : issuedEvents) if (event.value("protectedPathGrant").toObject().value("grantId") == id) { ++matches; issuance = event; }
    QJsonObject cold, memory;
    if (!error.isEmpty() || matches != 1 || issuance.value("protectedPathGrant") != grant
        || !refreshProtectedMutation(model, &cold, &memory, &error, false))
        return blocked("Retained grant issuance has no completed validation and cannot activate: " + error);
    const QJsonObject validation{{"grantId", id}, {"issuedEventId", issuance.value("eventId")}, {"issuedEventFingerprint", exactEventHash(issuance)},
        {"authorityEventId", authorityEventId}, {"authorityEventFingerprint", exactEventHash(authority)},
        {"coldStartValidation", cold}, {"memoryValidation", memory}, {"provenance", grantServiceProvenance()}};
    if (!ProjectMemory().appendEvent(root(model), "PROTECTED_GRANT_VALIDATED", "Exact P6 grant canonical audit validation completed", validation, &error)
        || !refreshProtectedMutation(model, &cold, &memory, &error, true) || !protectedMutationReadback(model, before, &error))
        return blocked("Retained authority mutation did not finish canonical validation: " + error);
    auto grantedTask = task; grantedTask.protectedGrantId = id;
    const auto checked = checkedGrant(model, grantedTask, &error);
    if (checked.isEmpty() || !error.isEmpty()) return blocked("Issued grant failed exact current readback: " + error);
    return {{"status", "PASS"}, {"grant", checked}, {"registration", QString::fromUtf8(p6Registration())},
        {"canonicalOutputs", array(protectedGrantOutputs())}, {"authorityEventId", authorityEventId}};
}

QJsonObject WorkerTaskServices::revokeP6GrantAuthority(const ProjectModel& model, const QString& targetKind,
                                                      const QString& targetId, const QString& authorityEventId)
{
    const auto blocked = [](const QString& message) { return QJsonObject{{"status", "BLOCKED"}, {"error", message}}; };
    if (!model.workerNameSuffix().isEmpty() || AramfPaths::runtimeWorkerDirectoryName() != worker(model))
        return blocked("Protected P6 revocation requires the default project-local authority store");
    if ((targetKind != "grant" && targetKind != "authority") || targetId.isEmpty()) return blocked("An exact protected grant or original authority identity is required");
    QLockFile lock(QDir::tempPath() + "/aramf-protected-grant-" + digest(root(model).toUtf8()) + ".lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Grant authority is busy");
    QString error; const auto events = ProjectMemory().events(root(model), &error); const auto authority = uniqueEvent(events, authorityEventId);
    const QString instruction = "Admin Morgan Lindbom explicitly revokes protected P6 " + targetKind + " " + targetId + " with this administrative override.";
    if (!error.isEmpty() || !humanAuthority(authority) || authority.value("instruction") != instruction
        || authority.value("scope") != "P6-PROTECTED-GRANT-REVOCATION" || authority.value("requestedAction") != "Revoke " + targetKind + " " + targetId
        || !authority.value("affectedFiles").toArray().contains("CMakeLists.txt")) return blocked("Exact retained human revocation authority is required");
    QJsonObject target; int matches = 0;
    for (const auto& event : events) {
        const bool match = targetKind == "grant" ? event.value("protectedPathGrant").toObject().value("grantId") == targetId : event.value("eventId") == targetId;
        if (match) { ++matches; target = event; }
        if ((event.value("eventType") == "PROTECTED_GRANT_REVOKED" && targetKind == "grant" && event.value("grantId") == targetId)
            || (event.value("eventType") == "PROTECTED_AUTHORITY_REVOKED" && targetKind == "authority" && event.value("authorityEventId") == targetId))
            return blocked("The exact protected identity is already revoked; history cannot be replaced");
    }
    if (matches != 1 || (targetKind == "authority" && !scopedP6Authority(target))) return blocked("The exact protected target is absent duplicated or outside the P6 authority form");
    QJsonObject before; if (!protectedMutationReady(model, &before, &error)) return blocked(error);
    QJsonObject fields{{targetKind == "grant" ? "grantId" : "authorityEventId", targetId}, {"targetFingerprint", exactEventHash(target)},
        {"revocationAuthorityEventId", authorityEventId}, {"revocationAuthorityEventFingerprint", exactEventHash(authority)}, {"provenance", grantServiceProvenance()}};
    if (!ProjectMemory().appendEvent(root(model), targetKind == "grant" ? "PROTECTED_GRANT_REVOKED" : "PROTECTED_AUTHORITY_REVOKED",
        "Exact protected P6 authority revoked through retained human instruction", fields, &error))
        return blocked("Revocation append failed; any durable partial event remains retained: " + error);
    QJsonObject cold, memory;
    if (!refreshProtectedMutation(model, &cold, &memory, &error, true) || !protectedMutationReadback(model, before, &error))
        return blocked("Revocation retained but canonical refresh failed: " + error);
    return {{"status", "PASS"}, {"targetKind", targetKind}, {"targetId", targetId}, {"canonicalOutputs", array(protectedGrantOutputs())}};
}

QJsonObject WorkerTaskServices::consumeP6BuildGrant(const ProjectModel& model, const QJsonObject& contract,
                                                  const QJsonArray& evidence)
{
    const auto blocked = [](const QString& error) { return QJsonObject{{"status", "BLOCKED"}, {"error", error}}; };
    QLockFile lock(QDir::tempPath() + "/aramf-protected-grant-" + digest(root(model).toUtf8()) + ".lock"); lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) return blocked("Grant authority is busy");
    const auto request = WorkerTaskRequest::fromJson(contract.value("request").toObject()); QString error;
    const auto grant = checkedGrant(model, request, &error); // Consumption is always unused-only.
    if (grant.isEmpty() || !error.isEmpty()) return blocked(error.isEmpty() ? "No valid unused grant" : error);
    if (fileHash(pathFor(model, "CMakeLists.txt")) != grant.value("resultFingerprint")) return blocked("Exact protected result is not present");
    QJsonObject before;
    if (!protectedMutationReady(model, &before, &error)) return blocked(error);
    const auto validation = postflight(model, contract, evidence);
    if (validation.value("status") != "PASS" || (validation.value("completionState") != "VERIFIED" && validation.value("completionState") != "CERTIFIED"))
        return {{"status", "BLOCKED"}, {"error", "Complete verified postflight required before consumption"}, {"validation", validation}};
    if (grant.value("schemaVersion").toInt() == 2) {
        const auto measured = checkEvidence(model, contract);
        if (measured.value("status") != "PASS" || semantic(measured.value("evidence")) != semantic(validation.value("acceptedEvidence")))
            return blocked("Linked protected grants require complete actual canonical producer observations, not caller PASS envelopes");
    }
    const auto events = ProjectMemory().events(root(model), &error); const auto authority = uniqueEvent(events, grant.value("authorityEventId").toString());
    const QJsonObject fields{{"grantId", request.protectedGrantId}, {"contractId", contract.value("contractId")},
        {"contractFingerprint", exactEventHash(contract)}, {"requestFingerprint", grantRequestFingerprint(request)}, {"grantFingerprint", exactEventHash(grant)},
        {"resultFingerprint", validation.value("resultFingerprint")}, {"authorityEventId", grant.value("authorityEventId")},
        {"authorityEventFingerprint", exactEventHash(authority)}, {"verifiedPostflight", validation}, {"postflightFingerprint", exactEventHash(validation)},
        {"eventPrefixFingerprint", fileHash(pathFor(model, "ARAMF_WORKER/memory/event-log.jsonl"))},
        {"eventPrefixBytes", static_cast<double>(QFileInfo(pathFor(model, "ARAMF_WORKER/memory/event-log.jsonl")).size())},
        {"provenance", grantServiceProvenance()}};
    QJsonArray errors;
    if (!error.isEmpty() || operationSnapshot(model, errors) != before || !state(errors).startsWith("READY"))
        return blocked("Protected source evidence or baseline changed during consumption verification");
    if (!ProjectMemory().appendEvent(root(model), "PROTECTED_GRANT_CONSUMED", "P6 build-registration grant consumed once for its exact verified contract", fields, &error))
        return blocked("Consumption append failed; any durable event remains consumed and cannot be repeated: " + error);
    QJsonObject cold, memory;
    if (!refreshProtectedMutation(model, &cold, &memory, &error, true) || !protectedMutationReadback(model, before, &error))
        return blocked("Consumption retained; canonical refresh did not finish: " + error);
    const auto current = postflight(model, contract, evidence);
    return {{"status", current.value("completionState") == "VERIFIED" || current.value("completionState") == "CERTIFIED" ? "PASS" : "BLOCKED"},
        {"grantId", request.protectedGrantId}, {"validation", current}, {"preConsumptionValidation", validation},
        {"canonicalOutputs", array(protectedGrantOutputs())}};
}

// Dispatch task operations without implicit authority or lifecycle changes.

// Grant issuance and consumption are explicit mutating commands; prepare and
// postflight retain their inspection-only authorization semantics.
int runWorkerTaskCommand(const QStringList& arguments, QTextStream& output, QTextStream& error)
{
    const auto option = [&arguments](const QString& key) { const int index = arguments.indexOf(key); return index >= 0 ? arguments.value(index + 1) : QString{}; };
    const auto containsForbiddenCallerAuthority = [](const QJsonValue& value, const auto& self) -> bool {
        if (value.isObject()) {
            const auto object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it) {
                const QString key = it.key().toCaseFolded();
                if (key.contains("fingerprint") || key.contains("baseline") || key == "contractid"
                    || key == "authorizationid" || key == "grantid" || self(it.value(), self)) return true;
            }
        } else if (value.isArray()) {
            for (const auto& item : value.toArray()) if (self(item, self)) return true;
        }
        return false;
    };
    ProjectModel model;
    QString failure;
    auto inspectionContract = arguments.value(1) == "postflight" ? readObject(option("--contract")) : QJsonObject{};
    if (inspectionContract.contains("contract")) inspectionContract = inspectionContract.value("contract").toObject();
    const bool activationCommand = QStringList{"prepare-activation", "apply-activation", "postflight-activation", "reauthorize-repair"}.contains(arguments.value(1))
        || inspectionContract.value("contractKind") == activationKind;
    const bool scopeRegistrationCommand = QStringList{"prepare-scope-registration", "apply-scope-registration", "postflight-scope-registration"}.contains(arguments.value(1))
        || inspectionContract.value("contractKind") == scopeRegistrationKind;
    const bool loaded = scopeRegistrationCommand ? readScopeRegistrationModel(option("--config"), &model, &failure)
        : activationCommand ? readActivationModel(option("--config"), &model, &failure)
        : ProjectPersistence().load(&model, option("--config"), &failure);
    if (!loaded) { error << failure << '\n'; return 2; }
    QJsonObject result;
    if (arguments.value(1) == "prepare-scope-registration" || arguments.value(1) == "apply-scope-registration"
        || arguments.value(1) == "postflight-scope-registration") {
        const bool preparing = arguments.value(1) == "prepare-scope-registration";
        const QSet<QString> allowed = preparing ? QSet<QString>{"--config", "--request", "--blocked-contract", "--verification-contract"}
            : QSet<QString>{"--config", "--contract"};
        QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete scope-registration option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        if (seen != allowed) { error << "Every exact scope-registration option is required\n"; return 2; }
        result = preparing ? WorkerTaskServices::prepareScopeRegistrationOperation(model, readObject(option("--request")),
            readObject(option("--blocked-contract")), readObject(option("--verification-contract")))
            : arguments.value(1) == "apply-scope-registration" ? WorkerTaskServices::applyScopeRegistrationOperation(model, readObject(option("--contract")))
                : WorkerTaskServices::postflightScopeRegistrationOperation(model, readObject(option("--contract")));
        output << QJsonDocument(preparing ? QJsonObject{{"contract", result}} : result).toJson(QJsonDocument::Indented);
        return preparing ? (result.value("preflight").toObject().value("status").toString().startsWith("READY") ? 0 : 2)
            : (result.value("status") == "PASS" ? 0 : 2);
    }
    if (arguments.value(1) == "reauthorize-repair") {
        const QSet<QString> allowed{"--config", "--request"}; QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported duplicate or incomplete repair authorization option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        if (seen != allowed) { error << "Exact config and declarative repair request are required\n"; return 2; }
        result = authorizeRepairSuccessor(model, readObject(option("--request")));
        output << QJsonDocument(result).toJson(QJsonDocument::Indented);
        return result.value("status") == "PASS" ? 0 : 2;
    }
    if (arguments.value(1) == "p6-requirements-baseline") {
        if (arguments.size() != 4 || arguments.value(2) != "--config") { error << "Only --config is accepted for the fixed baseline preview\n"; return 2; }
        output << QJsonDocument(WorkerTaskServices::p6RequirementsBaseline()).toJson(QJsonDocument::Indented);
        return 0;
    }
    if (arguments.value(1) == "prepare-activation" || arguments.value(1) == "apply-activation" || arguments.value(1) == "postflight-activation") {
        const bool preparing = arguments.value(1) == "prepare-activation";
        const QSet<QString> allowed = preparing ? QSet<QString>{"--config", "--request", "--integration-contract"}
            : QSet<QString>{"--config", "--contract"};
        QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete activation operation option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        if (seen != allowed) { error << "All exact activation operation options are required\n"; return 2; }
        result = preparing ? WorkerTaskServices::prepareActivationOperation(model, readObject(option("--request")), readObject(option("--integration-contract")))
            : arguments.value(1) == "postflight-activation" ? WorkerTaskServices::postflightActivationOperation(model, readObject(option("--contract")))
                : WorkerTaskServices::applyActivationOperation(model, readObject(option("--contract")));
        output << QJsonDocument(preparing ? QJsonObject{{"contract", result}} : result).toJson(QJsonDocument::Indented);
        return preparing ? (result.value("preflight").toObject().value("status").toString().startsWith("READY") ? 0 : 2)
            : (result.value("status") == "PASS" ? 0 : 2);
    }
    if (arguments.value(1) == "prepare-requirements" || arguments.value(1) == "apply-requirements") {
        const bool preparing = arguments.value(1) == "prepare-requirements";
        const QSet<QString> allowed = preparing ? QSet<QString>{"--config", "--request", "--integration-contract"}
            : QSet<QString>{"--config", "--contract"};
        QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete requirements operation option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        if (seen != allowed) { error << "All exact requirements operation options are required\n"; return 2; }
        result = preparing ? WorkerTaskServices::prepareRequirementsOperation(model, readObject(option("--request")), readObject(option("--integration-contract")))
            : WorkerTaskServices::applyRequirementsOperation(model, readObject(option("--contract")));
        output << QJsonDocument(preparing ? QJsonObject{{"contract", result}} : result).toJson(QJsonDocument::Indented);
        return preparing ? (result.value("preflight").toObject().value("status").toString().startsWith("READY") ? 0 : 2)
            : (result.value("status") == "PASS" ? 0 : 2);
    }
    if (arguments.value(1) == "prepare-certification" || arguments.value(1) == "apply-certification" || arguments.value(1) == "recover-certification") {
        const bool preparing = arguments.value(1) == "prepare-certification";
        const QSet<QString> allowed = preparing ? QSet<QString>{"--config", "--request", "--verification-contract"}
            : QSet<QString>{"--config", "--contract"};
        QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete canonical operation option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        result = preparing ? WorkerTaskServices::prepareCertificationOperation(model, readObject(option("--request")),
            readObject(option("--verification-contract"))) : arguments.value(1) == "recover-certification"
                ? WorkerTaskServices::recoverCertificationOperation(model, readObject(option("--contract")))
                : WorkerTaskServices::applyCertificationOperation(model, readObject(option("--contract")));
        output << QJsonDocument(preparing ? QJsonObject{{"contract", result}} : result).toJson(QJsonDocument::Indented);
        return preparing ? (result.value("preflight").toObject().value("status").toString().startsWith("READY") ? 0 : 2)
            : (result.value("status") == "PASS" || result.value("status") == "RECOVERED_FAILURE" ? 0 : 2);
    }
    if (arguments.value(1) == "acceptance-profile") {
        result = WorkerTaskServices::acceptanceProfile(option("--profile"));
        output << QJsonDocument(result).toJson(QJsonDocument::Indented);
        return result.isEmpty() ? 2 : 0;
    }
    if (arguments.value(1) == "run-check" || arguments.value(1) == "evidence-index") {
        const QSet<QString> allowed = arguments.value(1) == "run-check"
            ? QSet<QString>{"--config", "--contract", "--check", "--build-dir", "--timeout-ms"}
            : QSet<QString>{"--config", "--contract"};
        QSet<QString> seen;
        for (int index = 2; index < arguments.size(); index += 2) {
            const auto key = arguments.at(index);
            if (!allowed.contains(key) || seen.contains(key) || index + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete evidence-producer option; caller commands, results and fingerprints are forbidden\n";
                return 2;
            }
            seen.insert(key);
        }
        const auto contract = readObject(option("--contract"));
        result = arguments.value(1) == "run-check"
            ? WorkerTaskServices::runCheck(model, contract, option("--check"), option("--build-dir"),
                option("--timeout-ms").isEmpty() ? 3600000 : option("--timeout-ms").toInt())
            : WorkerTaskServices::checkEvidence(model, contract);
        output << QJsonDocument(result).toJson(QJsonDocument::Indented);
        return result.value("status") == "PASS" ? 0 : 2;
    }
    if (arguments.value(1) == "grant-p6-build") {
        const QSet<QString> allowed{"--config", "--request", "--instruction", "--authority-event", "--expires-at"};
        QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete protected grant option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        if (option("--instruction").isEmpty() == option("--authority-event").isEmpty()) {
            error << "Exactly one current human --instruction or retained --authority-event is required\n"; return 2;
        }
        const auto task = WorkerTaskRequest::fromJson(readObject(option("--request")));
        result = option("--authority-event").isEmpty() ? WorkerTaskServices::issueP6BuildGrant(model, task, option("--instruction"), option("--expires-at"))
            : WorkerTaskServices::issueP6BuildGrantFromEvent(model, task, option("--authority-event"), option("--expires-at"));
        output << QJsonDocument(result).toJson();
        return result.value("status") == "PASS" ? 0 : 2;
    }
    if (arguments.value(1) == "revoke-p6-grant-authority") {
        const QSet<QString> allowed{"--config", "--target-kind", "--target-id", "--authority-event"}; QSet<QString> seen;
        for (int i = 2; i < arguments.size(); i += 2) {
            if (!allowed.contains(arguments.at(i)) || seen.contains(arguments.at(i)) || i + 1 >= arguments.size()) {
                error << "Unsupported, duplicate or incomplete protected revocation option\n"; return 2;
            }
            seen.insert(arguments.at(i));
        }
        if (seen != allowed) { error << "All exact protected revocation options are required\n"; return 2; }
        result = WorkerTaskServices::revokeP6GrantAuthority(model, option("--target-kind"), option("--target-id"), option("--authority-event"));
        output << QJsonDocument(result).toJson(); return result.value("status") == "PASS" ? 0 : 2;
    }
    if (arguments.value(1) == "consume-p6-build-grant") {
        auto contract = readObject(option("--contract"));
        if (contract.contains("contract")) contract = contract.value("contract").toObject();
        result = WorkerTaskServices::consumeP6BuildGrant(model, contract, readObject(option("--evidence")).value("evidence").toArray());
        output << QJsonDocument(result).toJson();
        return result.value("status") == "PASS" ? 0 : 2;
    }
    if (arguments.value(1) == "reauthorize-approve") {
        const auto input = readObject(option("--request"));
        if (containsForbiddenCallerAuthority(input, containsForbiddenCallerAuthority)) {
            error << "Caller-supplied baselines, fingerprints and authority identifiers are forbidden\n";
            return 2;
        }
        result = WorkerTaskServices::authorizeReauthorization(model, readObject(option("--contract")),
            readObject(option("--postflight")), TaskReauthorizationRequest::fromJson(input), option("--instruction"));
        output << QJsonDocument(result).toJson(QJsonDocument::Indented);
        return result.value("status") == "PASS" ? 0 : 2;
    }
    if (arguments.value(1) == "reauthorize-prepare") {
        result = WorkerTaskServices::prepareReauthorized(model, option("--authorization-id"),
            readObject(option("--contract")), readObject(option("--postflight")));
        output << QJsonDocument(QJsonObject{{"contract", result}}).toJson(QJsonDocument::Indented);
        return result.value("preflight").toObject().value("status").toString().startsWith("READY") ? 0 : 2;
    }
    if (arguments.value(1) == "prepare") {
        const auto request = readObject(option("--request"));
        result = WorkerTaskServices::prepare(model, WorkerTaskRequest::fromJson(request));
        output << QJsonDocument(QJsonObject{{"contract", result}}).toJson(QJsonDocument::Indented);
        return result.value("preflight").toObject().value("status").toString().startsWith("READY") ? 0 : 2;
    }
    if (arguments.value(1) == "postflight") {
        auto contract = readObject(option("--contract"));
        if (contract.contains("contract")) contract = contract.value("contract").toObject();
        const auto evidence = readObject(option("--evidence")).value("evidence").toArray();
        result = WorkerTaskServices::postflight(model, contract, evidence);
        output << QJsonDocument(result).toJson(QJsonDocument::Indented);
        return result.value("completionState").toString() == "VERIFIED" || result.value("completionState").toString() == "CERTIFIED" ? 0 : 2;
    }
    error << "Usage: aramf task prepare|postflight|run-check|evidence-index|acceptance-profile|reauthorize-approve|reauthorize-prepare --config <saved-project> ...\n";
    return 2;
}
