// WorkerContextResolver.cpp
#include "WorkerContextResolver.h"

#include "AramfPaths.h"
#include "ProjectModel.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QDir>
#include <algorithm>
#include <QCryptographicHash>
#include <QDateTime>
#include <filesystem>
#include <functional>

namespace {
thread_local int observationDepth = 0;
thread_local QHash<QByteArray, QJsonObject> observations;
QJsonObject readObject(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Missing or unreadable routing file: %1").arg(path);
        return {};
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("Invalid routing JSON: %1").arg(path);
        return {};
    }
    return document.object();
}

QStringList uniqueSorted(QStringList values)
{
    values.removeDuplicates();
    std::sort(values.begin(), values.end());
    return values;
}

QJsonArray array(const QStringList& values)
{
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}
}

QJsonObject WorkerContextResolver::resolve(const QString& workerRoot, QStringList scopes)
{
    scopes = uniqueSorted(scopes);
    QJsonObject result{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("scopes"), array(scopes)},
                       {QStringLiteral("historyRequired"), false}};
    QString error;
    const QString routePath = QDir(workerRoot).filePath(QStringLiteral("routing/scope-routes.json"));
    const auto routes = readObject(routePath, &error);
    const bool legacyRoutes = routes.value(QStringLiteral("schemaVersion")).isUndefined()
        && routes.value(QStringLiteral("scopes")).isArray()
        && !routes.value(QStringLiteral("scopes")).toArray().isEmpty()
        && routes.value(QStringLiteral("scopes")).toArray().first().isString();
    if (!error.isEmpty() || (routes.value(QStringLiteral("schemaVersion")).toInt(-1) != 1 && !legacyRoutes)) {
        result.insert(QStringLiteral("valid"), false);
        result.insert(QStringLiteral("diagnostics"), QJsonArray{error.isEmpty() ? QStringLiteral("Unsupported scope-route schema.") : error});
        result.insert(QStringLiteral("errors"), QJsonArray{QJsonObject{{"code", error.isEmpty() ? "WORKER_SCHEMA_UNSUPPORTED" : "WORKER_ROUTE_MISSING"},
            {"message", error.isEmpty() ? QStringLiteral("Unsupported scope-route schema.") : error}}});
        return result;
    }

    QHash<QString, QJsonObject> byScope;
    QStringList duplicateScopes;
    for (const auto& value : routes.value(QStringLiteral("scopes")).toArray()) {
        if (value.isString()) {
            const QString id = value.toString();
            byScope.insert(id, QJsonObject{{QStringLiteral("id"), id},
                {QStringLiteral("required"), QJsonArray{AramfPaths::ProjectConfiguration, AramfPaths::CurrentState}},
                {QStringLiteral("optional"), QJsonArray{AramfPaths::ResourceManifest}}});
            continue;
        }
        const auto route = value.toObject();
        const QString id = route.value(QStringLiteral("id")).toString();
        if (byScope.contains(id)) duplicateScopes.append(id);
        if (!id.isEmpty()) byScope.insert(id, route);
    }

    QStringList mandatory{AramfPaths::ProjectConfiguration, AramfPaths::WorkerManifest,
                          AramfPaths::CurrentState, AramfPaths::ColdStartValidation};
    QStringList optional;
    QStringList instructions;
    QStringList diagnostics;
    QJsonArray errors;
    for (const auto& id : duplicateScopes) {
        diagnostics.append(QStringLiteral("Duplicate route owner: %1").arg(id));
        errors.append(QJsonObject{{"code", "CANONICAL_OWNER_DUPLICATE"}, {"message", diagnostics.last()}, {"scope", id}});
    }
    QJsonArray resolvedRoutes;
    bool history = false;
    for (const auto& scope : scopes) {
        // History is an explicit audit context, not a normal project scope.
        // Keeping it out of the generated route table makes ordinary cold
        // starts provably independent from append-only event history.
        if (scope == QStringLiteral("history")) {
            history = true;
            continue;
        }
        if (!byScope.contains(scope)) {
            diagnostics.append(QStringLiteral("No route defined for scope: %1").arg(scope));
            errors.append(QJsonObject{{"code", "WORKER_SCOPE_UNRESOLVED"}, {"message", diagnostics.last()}, {"scope", scope}});
            continue;
        }
        const auto route = byScope.value(scope);
        resolvedRoutes.append(route);
        for (const auto& value : route.value(QStringLiteral("required")).toArray()) mandatory.append(value.toString());
        for (const auto& value : route.value(QStringLiteral("optional")).toArray()) optional.append(value.toString());
        for (const auto& value : route.value(QStringLiteral("instructions")).toArray()) instructions.append(value.toString());
        history = history || route.value(QStringLiteral("historyRequired")).toBool();
    }
    mandatory = uniqueSorted(mandatory);
    optional = uniqueSorted(optional);
    optional.removeAll(QString());
    for (const auto& path : mandatory) optional.removeAll(path);
    instructions = uniqueSorted(instructions);

    QString resourceError;
    const auto resources = readObject(QDir(workerRoot).filePath(QStringLiteral("resources/resources.json")), &resourceError);
    QJsonArray selectedResources;
    if (!resourceError.isEmpty()) {
        diagnostics.append(resourceError);
        errors.append(QJsonObject{{"code", "WORKER_ROUTE_MISSING"}, {"message", resourceError}});
    } else {
        for (const auto& value : resources.value(QStringLiteral("resources")).toArray()) {
            const auto resource = value.toObject();
            if (!resource.value(QStringLiteral("enabled")).toBool()) continue;
            QStringList resourceScopes;
            for (const auto& scope : resource.value(QStringLiteral("scopes")).toArray()) resourceScopes.append(scope.toString());
            bool relevant = resourceScopes.isEmpty() || resourceScopes.contains(QStringLiteral("all"));
            for (const auto& scope : scopes) if (resourceScopes.contains(scope)) relevant = true;
            if (relevant) selectedResources.append(QJsonObject{{QStringLiteral("id"), resource.value(QStringLiteral("id"))},
                {QStringLiteral("role"), resource.value(QStringLiteral("role"))}, {QStringLiteral("authority"), resource.value(QStringLiteral("authority"))},
                {QStringLiteral("type"), resource.value(QStringLiteral("type"))},
                {QStringLiteral("location"), resource.value(QStringLiteral("location"))}, {QStringLiteral("scopes"), resource.value(QStringLiteral("scopes"))}});
        }
    }
    result.insert(QStringLiteral("valid"), diagnostics.isEmpty());
    result.insert(QStringLiteral("legacyRouteFormat"), legacyRoutes);
    result.insert(QStringLiteral("mandatoryFiles"), array(mandatory));
    result.insert(QStringLiteral("optionalFiles"), array(optional));
    result.insert(QStringLiteral("instructions"), array(instructions));
    result.insert(QStringLiteral("resources"), selectedResources);
    result.insert(QStringLiteral("validation"), QJsonArray{AramfPaths::ColdStartValidation, AramfPaths::ValidationPolicy});
    result.insert(QStringLiteral("historyRequired"), history);
    result.insert(QStringLiteral("historyFiles"), history ? QJsonArray{AramfPaths::EventLog} : QJsonArray{});
    result.insert(QStringLiteral("diagnostics"), array(diagnostics));
    result.insert(QStringLiteral("errors"), errors);
    result.insert(QStringLiteral("resolvedRoutes"), resolvedRoutes);
    return result;
}

// Capture the complete typed resource without conflating errors with empty data.

// The folder-v1 digest binds identity, explicit policy and sorted relative
// entries. Two complete observations must agree. Exact generated-file markers
// break self-reference only for known canonical validation/context outputs;
// all other children, including ignored and hidden files, remain content-bound.
static QJsonObject inspectResource(const ProjectModel& model, const QString& resourceId)
{
    ProjectResource resource;
    bool found = false;
    for (const auto& candidate : model.resources()) {
        if (candidate.id == resourceId) {
            if (found) return {{"valid", false}, {"error", "Duplicate resource identity"}};
            resource = candidate;
            found = true;
        }
    }
    QJsonObject result{{"schemaVersion", 1}, {"resourceId", resourceId}, {"valid", false}};
    if (!found) { result.insert("error", "Unknown resource identity"); return result; }
    const QString path = QDir::cleanPath(QDir(model.projectPath()).absoluteFilePath(resource.location));
    const QFileInfo rootInfo(path);
    if (resource.location.contains("://") || !rootInfo.exists() || rootInfo.isSymLink()
        || rootInfo.canonicalFilePath().compare(path, Qt::CaseInsensitive) != 0) {
        result.insert("error", "Missing, remote or linked resource root"); return result;
    }
    QJsonObject policy;
    for (const auto& metadata : model.ruleConfiguration().scopeMetadata) {
        const auto policies = metadata.toObject().value("folderResourcePolicies").toObject();
        if (!policies.contains(resourceId)) continue;
        if (!policy.isEmpty()) { result.insert("error", "Ambiguous folder policy owner"); return result; }
        policy = policies.value(resourceId).toObject();
        if (policy.value("schemaVersion").toInt() != 1 || !policy.value("generatedPaths").isArray()) {
            result.insert("error", "Unsupported folder policy schema"); return result;
        }
    }
    QSet<QString> generated;
    const QSet<QString> canonicalGenerated{"context/context-index.json", "context/compressed-context.json",
        "context/freshness.json", "context/agent-adapters.json", "verification/latest-validation.json",
        "verification/verification-result.json", "verification/generation-state.json",
        "memory/cold-start-validation.json", "memory/memory-consistency-validation.json"};
    for (const auto& value : policy.value("generatedPaths").toArray()) {
        const QString relative = value.toString();
        if (resource.type != "folder" || path != QDir(model.projectPath()).filePath(AramfPaths::workerDirectoryName(model.workerNameSuffix()))
            || !canonicalGenerated.contains(relative) || generated.contains(relative)) {
            result.insert("error", "Generated exclusion lacks an exact canonical producer"); return result;
        }
        generated.insert(relative);
    }
    if ((resource.type == "folder") != rootInfo.isDir() || (!rootInfo.isDir() && !rootInfo.isFile())) {
        result.insert("error", "Resource type does not match filesystem entry"); return result;
    }
    QString failure;
    // Hash bytes only after checking the complete read and stable file identity.

    // Read errors, concurrent replacement and partial reads are invalid evidence,
    // not the SHA-256 of an empty byte array.
    const auto hashFile = [&](const QString& filePath) -> QString {
        const QFileInfo before(filePath);
        QFile file(filePath);
        if (!before.isFile() || before.isSymLink() || !file.open(QIODevice::ReadOnly)) {
            failure = "Unreadable or unsafe file: " + filePath; return {};
        }
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 count = 0;
        while (!file.atEnd()) {
            const auto bytes = file.read(1024 * 1024);
            if (bytes.isEmpty() && !file.atEnd()) { failure = "Incomplete read: " + filePath; return {}; }
            count += bytes.size(); hash.addData(bytes);
        }
        const QFileInfo after(filePath);
        if (file.error() != QFileDevice::NoError || count != before.size() || after.size() != before.size()
            || after.lastModified() != before.lastModified() || after.isSymLink()
            || after.canonicalFilePath() != before.canonicalFilePath()) {
            failure = "Resource changed during read: " + filePath; return {};
        }
        return QString::fromLatin1(hash.result().toHex());
    };
    // Collect sorted relative entries and fail on every traversal error.

    // std::filesystem reports directory enumeration errors that an empty Qt
    // entry list would hide. Links, case aliases and normalization ambiguity
    // are rejected; no implicit ignore files or glob exclusions are used.
    const auto observe = [&]() -> QJsonObject {
        QJsonObject entries;
        QSet<QString> folded;
        std::function<bool(const QString&)> visit;
        visit = [&](const QString& relative) {
            const QString absolute = relative.isEmpty() ? path : QDir(path).filePath(relative);
            const QFileInfo info(absolute);
            if (info.isSymLink() || info.canonicalFilePath().compare(absolute, Qt::CaseInsensitive) != 0
                || relative.contains('\\') || relative != relative.normalized(QString::NormalizationForm_C)
                || folded.contains(relative.toCaseFolded()) || entries.size() >= 100000) {
                failure = "Unsafe, ambiguous or oversized folder manifest: " + absolute; return false;
            }
            folded.insert(relative.toCaseFolded());
            if (generated.contains(relative)) {
                if (!info.isFile()) { failure = "Generated entry is not a regular file: " + absolute; return false; }
                entries.insert(relative, QJsonObject{{"type", "generated"}, {"verification", "canonical-producer-required"}});
                return true;
            }
            if (info.isFile()) {
                const auto hash = hashFile(absolute);
                if (hash.isEmpty()) return false;
                entries.insert(relative, QJsonObject{{"type", "file"}, {"sha256", hash}});
                return true;
            }
            if (!info.isDir()) { failure = "Unsupported entry: " + absolute; return false; }
            entries.insert(relative, QJsonObject{{"type", "folder"}});
            std::error_code error;
            const auto native = std::filesystem::u8path(absolute.toUtf8().constData());
            std::filesystem::directory_iterator iterator(native, error), end;
            QStringList children;
            for (; !error && iterator != end; iterator.increment(error)) {
                const auto encoded = iterator->path().filename().u8string();
                children.append(QString::fromUtf8(encoded.data(), static_cast<int>(encoded.size())));
            }
            if (error) { failure = "Incomplete directory enumeration: " + absolute; return false; }
            children.sort(Qt::CaseSensitive);
            for (const auto& child : children) {
                if (child == "." || child == ".." || child.contains('/') || child.contains('\\')
                    || !visit(relative.isEmpty() ? child : relative + '/' + child)) return false;
            }
            return true;
        };
        if (!visit({})) return {};
        return entries;
    };
    result.insert("type", resource.type);
    result.insert("policy", policy);
    if (resource.type == "folder") {
        const auto first = observe();
        const auto second = failure.isEmpty() ? observe() : QJsonObject{};
        if (!failure.isEmpty() || first != second) {
            result.insert("error", failure.isEmpty() ? "Folder changed during inspection" : failure); return result;
        }
        result.insert("entries", first);
        const auto payload = QJsonDocument(result).toJson(QJsonDocument::Compact);
        result.insert("fingerprint", QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex()));
    } else {
        const auto hash = hashFile(path);
        if (hash.isEmpty()) { result.insert("error", failure); return result; }
        result.insert("fingerprint", hash);
    }
    result.insert("valid", true);
    return result;
}

// Start a fresh observation group at the outer service boundary.

// Nested calls reuse verified observations only during this synchronous group.
WorkerResourceObservation::WorkerResourceObservation()
{
    if (observationDepth++ == 0) observations.clear();
}

// Discard observations when the outer service call finishes.

// Subsequent preparation, verification and postflight must inspect disk again.
WorkerResourceObservation::~WorkerResourceObservation()
{
    if (--observationDepth == 0) observations.clear();
}

// Inspect each resource once within an explicitly bounded operation.

// The cache key binds project, worker, resource declarations and policy. Direct
// callers without a scope always inspect disk; invalid observations stay invalid.
QJsonObject WorkerContextResolver::resourceSnapshot(const ProjectModel& model, const QString& resourceId)
{
    QJsonArray resources;
    for (const auto& resource : model.resources())
        resources.append(QJsonObject{{"id", resource.id}, {"type", resource.type}, {"location", resource.location}});
    const auto key = QJsonDocument(QJsonObject{{"root", model.projectPath()}, {"projectId", model.projectId()},
        {"worker", model.workerNameSuffix()}, {"resourceId", resourceId}, {"resources", resources},
        {"policies", model.ruleConfiguration().scopeMetadata}}).toJson(QJsonDocument::Compact);
    if (observationDepth > 0 && observations.contains(key)) return observations.value(key);
    const auto result = inspectResource(model, resourceId);
    if (observationDepth > 0) observations.insert(key, result);
    return result;
}

QJsonObject WorkerContextResolver::scopeRoutes(const RuleConfiguration& rules, const QString& fingerprint)
{
    QJsonArray routes;
    for (const auto& scope : uniqueSorted(rules.projectScopes)) {
        QJsonArray instructions;
        if (scope == QStringLiteral("thesis")) instructions.append(QStringLiteral("aramf-thesis-instruction"));
        if (scope == QStringLiteral("report")) instructions.append(QStringLiteral("aramf-report-instruction"));
        routes.append(QJsonObject{{"id", scope}, {"required", QJsonArray{AramfPaths::ProjectConfiguration, AramfPaths::CurrentState}},
            {"optional", QJsonArray{AramfPaths::GeneratedRules, AramfPaths::ResourceManifest}}, {"instructions", instructions},
            {"historyRequired", false}, {"taskMetadata", rules.scopeMetadata.value(scope).toObject()}});
    }
    return {{"schemaVersion", 1}, {"inputFingerprint", fingerprint}, {"scopes", routes}};
}

QJsonObject WorkerContextResolver::taskRoutes(const RuleConfiguration& rules, const QString& fingerprint)
{
    return {{"schemaVersion", 1}, {"strategy", rules.loadingStrategy}, {"workTypes", array(rules.workScopes)},
        {"contextPolicies", array(rules.contextPolicies)}, {"conflictPolicy", rules.conflictPolicy}, {"inputFingerprint", fingerprint},
        {"readSet", QJsonObject{{"mandatory", QJsonArray{AramfPaths::ProjectConfiguration, AramfPaths::WorkerManifest, AramfPaths::CurrentState, AramfPaths::ColdStartValidation}},
            {"historyRequired", false}, {"scopeOrder", array(uniqueSorted(rules.projectScopes))}}}};
}

QJsonObject WorkerContextResolver::resolveImpact(const QString& workerRoot, QStringList scopes)
{
    QStringList visited;
    QStringList pending = uniqueSorted(scopes);
    while (!pending.isEmpty()) {
        const QString scope = pending.takeFirst();
        if (visited.contains(scope)) continue;
        visited.append(scope);
        const auto context = resolve(workerRoot, {scope});
        for (const auto& value : context.value("resolvedRoutes").toArray()) {
            for (const auto& affected : value.toObject().value("taskMetadata").toObject().value("affects").toArray())
                if (!visited.contains(affected.toString())) pending.append(affected.toString());
        }
        pending = uniqueSorted(pending);
    }
    return resolve(workerRoot, visited);
}
