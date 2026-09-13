#include "WorkerTaskServices.h"
#include "AramfPaths.h"
#include "CertificationService.h"
#include "ContextCoordinationService.h"
#include "DocumentInstruction.h"
#include "ProjectMemory.h"
#include "ProjectPersistence.h"
#include "Services.h"
#include "ValidationRouting.h"
#include "WorkerContextResolver.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcess>
#include <QSet>
#include <algorithm>

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
        if (!resource.location.isEmpty()) result.insert(QStringLiteral("external:") + resource.id, fileHash(pathFor(model, resource.location)));
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
        inputs.insert(path, path.endsWith(".json") && !json.isEmpty() ? fingerprint(json) : fileHash(absolute));
    }
    return {{"projectRoot", root(model)}, {"projectId", model.projectId()}, {"workerIdentity", worker(model)},
        {"modelFingerprint", fingerprint(ProjectPersistence().toJson(model))}, {"inputs", inputs}};
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
void validateCanonical(const ProjectModel& model, QJsonArray& errors)
{
    const auto config = readObject(pathFor(model, workerPath(model, "project.json")));
    const auto manifest = readObject(pathFor(model, workerPath(model, "worker-manifest.json")));
    const QString generationHash = projectConfigurationFingerprint(model, model.generationOptions());
    if (config.isEmpty() || manifest.isEmpty()) issue(errors, "MIGRATION_REQUIRED", "Generate the versioned Worker before preparing a task.");
    if (manifest.value("workerSchemaVersion").toInt() != 1 || config.value("schemaVersion").toInt() != 1)
        issue(errors, "WORKER_SCHEMA_UNSUPPORTED", "Task contracts require Worker schema v1.");
    if (config.value("projectId").toString() != model.projectId() || manifest.value("workerIdentity").toString() != worker(model))
        issue(errors, "WORKER_IDENTITY_MISMATCH", "Worker identity does not match the canonical project.");
    if (config.value("configurationFingerprint").toString() != generationHash || manifest.value("generatedFromFingerprint").toString() != generationHash)
        issue(errors, "STALE_DERIVED_ARTIFACT", "Project/manifest generation fingerprints are stale.");
    const QString contextIndexPath = pathFor(model, workerPath(model, "context/context-index.json"));
    if (QFileInfo::exists(contextIndexPath)) {
        const auto context = ContextCoordinationService::generate(model);
        if (!context.value(QStringLiteral("success")).toBool())
            issue(errors, "STALE_DERIVED_ARTIFACT", "Derived P1 context could not be safely regenerated.", context.value(QStringLiteral("error")).toString());
    }
    const auto expected = GenerationServices::derivedTaskArtifacts(model, model.generationOptions());
    for (auto it = expected.begin(); it != expected.end(); ++it) {
        if (it.key().contains(QStringLiteral("/context/"))) continue;
        auto actual = readObject(pathFor(model, it.key())); actual.remove("_file");
        if (semantic(actual) != semantic(it.value())) {
            issue(errors, "STALE_DERIVED_ARTIFACT", "Derived content disagrees with its canonical producer; regenerate it.", it.key());
        }
    }
    const auto summary = readObject(pathFor(model, workerPath(model, "verification/latest-validation.json")));
    const auto evidence = readObject(pathFor(model, workerPath(model, "verification/verification-result.json")));
    if (summary.value("schemaVersion").toInt() != 1 || summary.value("fingerprint").toString() != generationHash
        || summary.value("overallStatus").toString() != "PASS" || evidence.value("overallStatus").toString() != "PASS"
        || evidence.value("fingerprint").toString() != generationHash)
        issue(errors, "VALIDATION_SUMMARY_CORRUPT", "Current PASS summary and its detailed verification evidence are required.");
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
    if (verification.overallStatus != VerificationStatus::Pass)
        issue(errors, "VALIDATION_FAILED", "Canonical read-only Worker verification is not PASS; stored summary cannot authorize work.");
}
}

QJsonObject WorkerTaskRequest::toJson() const
{
    return {{"goal", goal}, {"type", type}, {"scopes", array(scopes)}, {"files", array(files)},
        {"definitionOfDone", array(definitionOfDone)}, {"history", history}, {"destructive", destructive}};
}
WorkerTaskRequest WorkerTaskRequest::fromJson(const QJsonObject& value)
{
    return {value.value("goal").toString(), value.value("type").toString(), strings(value.value("scopes")),
        strings(value.value("files")), strings(value.value("definitionOfDone")), value.value("history").toBool(), value.value("destructive").toBool()};
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

QJsonObject WorkerTaskServices::prepare(const ProjectModel& model, const WorkerTaskRequest& task)
{
    QJsonArray errors;
    if (!QDir(model.projectPath()).exists() || model.projectPath().isEmpty()) {
        issue(errors, "REPOSITORY_MISSING", "Select an existing project root.", {}, "FATAL");
        return {{"schemaVersion", 1}, {"preflight", QJsonObject{{"status", "FATAL"}, {"errors", errors}}}};
    }
    if (task.goal.trimmed().isEmpty() || task.scopes.isEmpty() || task.files.isEmpty() || task.definitionOfDone.isEmpty())
        issue(errors, "TASK_CONTRACT_INVALID", "Goal, scopes, exact proposed files and definition of done are required.");
    validateCanonical(model, errors);
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
    const auto policy = mutationPolicy(model);
    const auto impact = ChangeImpactResolver::resolve(model, task);
    const auto context = impact.value("context").toObject();
    if (!context.value("valid").toBool()) {
        for (const auto& message : context.value("diagnostics").toArray()) issue(errors, "WORKER_SCOPE_UNRESOLVED", message.toString());
    }
    QJsonArray permissions;
    QStringList permitted, protectedFiles;
    const auto potential = strings(impact.value("potentialFiles"));
    const auto permissionsConfigured = model.aiConfiguration().permissions;
    if (!permissionsConfigured.contains("read-project-files") || !permissionsConfigured.contains("modify-files"))
        issue(errors, "AGENT_PERMISSION_MISSING", "Canonical AI permissions must allow read-project-files and modify-files.");
    for (const auto& file : strings(array(task.files))) {
        auto ownership = fileRole(model, file, policy);
        const bool source = !file.startsWith(worker(model) + '/') && file != "AGENTS.md"
            && ownership.value("owner").toString() == "user" && !strings(ownership.value("actions")).contains("NEVER_TOUCH");
        if (!safePath(root(model), file)) issue(errors, "UNSAFE_PATH", "Proposed path is unsafe or escapes the project.", file, "FATAL");
        else if (!potential.contains(file)) { issue(errors, "FILE_OWNERSHIP_UNKNOWN", "Proposed file has no ownership mapping in the active task scopes.", file); issue(errors, "TASK_SCOPE_VIOLATION", "Proposed file is not mapped to an affected canonical scope.", file); }
        else if (!source) issue(errors, "FORBIDDEN_FILE_MODIFICATION", "File must be changed by its canonical owner/service, not the agent.", file);
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
    const auto tests = ValidationRouting::taskPlan(task.files, task.type, impact, risk);
    QJsonObject evidenceDependencies;
    QStringList taskDependencies = potential + task.files;
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
        const QString hash = remote ? QStringLiteral("UNVERIFIED_REMOTE") : fileHash(pathFor(model, location));
        if (remote || hash == "MISSING" || hash == "UNREADABLE") issue(errors, resource.value("role").toString() == "source-of-truth" ? "SOURCE_OF_TRUTH_MISSING" : "USER_SOURCE_MISSING", "Relevant source must be locally available and readable; remote sources require explicit verified local evidence.", location);
        sourceDependencies.append(QJsonObject{{"id", resourceId}, {"source", location}, {"fingerprint", hash}, {"authority", resource.value("authority")}});
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
    QJsonObject historyLengths;
    for (const QString& relative : {QStringLiteral("memory/event-log.jsonl"), QStringLiteral("certification/certificates.jsonl")})
        historyLengths.insert(workerPath(model, relative), static_cast<double>(QFileInfo(pathFor(model, workerPath(model, relative))).size()));
    parallelState(model, baseline, errors);
    QJsonObject contract{{"schemaVersion", 1}, {"authority", "DERIVED"}, {"request", task.toJson()}, {"binding", binding(model)},
        {"impact", impact}, {"permittedFiles", array(permitted)}, {"protectedFiles", array(protectedFiles)}, {"mutationPolicy", policy},
        {"filePermissions", permissions}, {"negativeConstraints", array(constraints)}, {"instructions", instructions},
        {"resources", sourceDependencies}, {"decisions", decisions}, {"decisionSource", workerPath(model, "memory/decisions.md")},
        {"risk", QJsonObject{{"level", risk}, {"reasons", array(reasons)}, {"reviewDepth", risk == "LOW" ? "focused" : "cross-component"}}},
        {"historyFiles", task.history ? QJsonArray{workerPath(model, "memory/event-log.jsonl")} : QJsonArray{}},
        {"validation", tests}, {"requiredEvidence", tests.value("completionChecks")}, {"evidenceDependencies", evidenceDependencies}, {"taskDependencies", array(taskDependencies)},
        {"baseline", baseline}, {"historyLengths", historyLengths},
        {"preflight", QJsonObject{{"status", state(errors)}, {"errors", errors}}}, {"completionState", "NOT_STARTED"}};
    contract.insert("contractId", fingerprint(contract));
    return contract;
}

QJsonObject WorkerTaskServices::postflight(const ProjectModel& model, const QJsonObject& contract, const QJsonArray& evidence)
{
    QJsonArray errors;
    auto unsignedContract = contract;
    unsignedContract.remove("contractId");
    if (contract.value("schemaVersion").toInt() != 1)
        issue(errors, "TASK_CONTRACT_INVALID", "Unsupported or missing TaskContract schema.");
    else if (fingerprint(unsignedContract) != contract.value("contractId").toString())
        issue(errors, "TASK_CONTRACT_TAMPERED", "Contract content/hash changed after preflight.");
    const QString preflight = contract.value("preflight").toObject().value("status").toString();
    if (preflight != "READY" && preflight != "READY_WITH_WARNINGS") issue(errors, "TASK_PREFLIGHT_BLOCKED", "Invalid preflight cannot authorize work.");
    if (contract.value("binding").toObject() != binding(model)) issue(errors, "STALE_TASK_CONTRACT", "Canonical inputs or project identity changed; derive and review a new contract.");
    // A recomputed checksum is not authority. Re-derive the authorization fields
    // from canonical inputs so an agent cannot widen a serialized contract.
    const auto authoritative = prepare(model, WorkerTaskRequest::fromJson(contract.value("request").toObject()));
    for (const QString& field : {QStringLiteral("permittedFiles"), QStringLiteral("requiredEvidence"), QStringLiteral("risk"), QStringLiteral("impact"), QStringLiteral("evidenceDependencies"), QStringLiteral("taskDependencies")})
        if (semantic(contract.value(field)) != semantic(authoritative.value(field))) issue(errors, "TASK_CONTRACT_INVALID", "Contract authorization differs from canonical derivation.", field);
    validateCanonical(model, errors);
    const auto current = snapshot(model, errors);
    const auto before = contract.value("baseline").toObject();
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
    bool verificationChecked = false;
    VerificationResult currentVerification;
    QString certificationError;
    const auto expectedCertification = CertificationService().derivedCurrentState(root(model), &certificationError);
    auto actualCertification = CertificationService().currentState(root(model)); actualCertification.remove("_file");
    const bool certificationStateValid = certificationError.isEmpty() && !expectedCertification.isEmpty() && actualCertification == expectedCertification;
    QJsonArray serviceChanges;
    for (const auto& path : paths) {
        if (before.value(path) == current.value(path)) continue;
        modified.append(path);
        const auto ownership = fileRole(model, path, mutationPolicy(model));
        bool delegated = false;
        if (ownership.value("writer").toString() == "ProjectMemory recorder") {
            delegated = checkMemory() && appendIntact(workerPath(model, "memory/event-log.jsonl"));
            if (path == workerPath(model, "memory/event-log.jsonl")) delegated = delegated && appendIntact(path);
        }
        const bool declaredGeneration = strings(contract.value("impact").toObject().value("generatedArtifacts")).contains(path);
        const bool p1Generation = expectedDerived.contains(path) && ownership.value("writer").toString() == "ContextCoordinationService";
        const bool requestedGeneration = declaredGeneration || p1Generation;
        if (requestedGeneration && expectedDerived.contains(path) && (ownership.value("writer").toString() == "GenerationServices" || p1Generation)
            && strings(ownership.value("actions")).contains("REGENERATE")) {
            auto actual = readObject(pathFor(model, path)); actual.remove("_file");
            delegated = !actual.isEmpty() && semantic(actual) == semantic(expectedDerived.value(path));
        }
        if (requestedGeneration && ownership.value("writer").toString() == "VerificationServices") {
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
            issue(errors, requestedGeneration ? "GENERATED_CONTENT_MISMATCH" : "UNEXPECTED_SERVICE_REGENERATION", "Derived change is undeclared or does not match its canonical producer.", path);
        if (!strings(contract.value("permittedFiles")).contains(path)) issue(errors, "TASK_SCOPE_VIOLATION", "Observed modification outside the exact permitted file set.", path);
        if (!current.contains(path) || current.value(path).toString() == "MISSING") issue(errors, "FORBIDDEN_FILE_MODIFICATION", "Deletion was not authorized by this task.", path);
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
    return {{"schemaVersion", 1}, {"authority", "DERIVED"}, {"contractId", contract.value("contractId")},
        {"resultFingerprint", resultFingerprint}, {"evidenceFingerprints", evidenceFingerprints}, {"modifiedFiles", array(modified)}, {"serviceChanges", serviceChanges}, {"status", safe && !evidenceFailure ? "PASS" : "FAIL"},
        {"completionState", completion}, {"missingEvidence", array(missing)}, {"acceptedEvidence", accepted}, {"errors", errors}, {"certificateId", certified ? certificate.value("certificateId") : QJsonValue{}},
        {"certification", "CERTIFIED is issued only by the canonical CertificationService; physical evidence is never inferred."}};
}

int runWorkerTaskCommand(const QStringList& arguments, QTextStream& output, QTextStream& error)
{
    const auto option = [&arguments](const QString& key) { const int index = arguments.indexOf(key); return index >= 0 ? arguments.value(index + 1) : QString{}; };
    ProjectModel model;
    QString failure;
    if (!ProjectPersistence().load(&model, option("--config"), &failure)) { error << failure << '\n'; return 2; }
    QJsonObject result;
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
    error << "Usage: aramf task prepare|postflight --config <saved-project> --request|--contract <json> [--evidence <json>]\n";
    return 2;
}
