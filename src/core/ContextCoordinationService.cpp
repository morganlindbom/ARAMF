#include "ContextCoordinationService.h"

#include "AramfPaths.h"
#include "WorkerContextResolver.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace {
class WorkerScope final {
public:
    explicit WorkerScope(const QString& suffix)
        : previous_(AramfPaths::detail::workerSuffixOverride())
    { AramfPaths::setRuntimeWorkerNameSuffix(suffix); }
    ~WorkerScope() { AramfPaths::setRuntimeWorkerNameSuffix(previous_); }
private:
    QString previous_;
};

QString root(const ProjectModel& model)
{ return QDir::cleanPath(model.projectPath()); }

QString workerRoot(const ProjectModel& model)
{ return QDir(root(model)).filePath(AramfPaths::runtimeWorkerDirectoryName()); }

QString absolute(const ProjectModel& model, const QString& relative)
{ return QDir(root(model)).filePath(AramfPaths::resolveWorkerRelativePath(relative)); }

QString hashBytes(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QByteArray fileBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QString fileFingerprint(const QString& path)
{
    if (!QFileInfo::exists(path)) return QStringLiteral("MISSING");
    const auto bytes = fileBytes(path);
    if (QFileInfo(path).suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
            return hashBytes(document.toJson(QJsonDocument::Compact));
    }
    return hashBytes(bytes);
}

bool readJson(const QString& path, QJsonObject* object)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    if (object) *object = document.object();
    return true;
}

QJsonArray strings(QStringList values)
{
    values.removeDuplicates();
    values.sort();
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

QStringList jsonStrings(const QJsonValue& value)
{
    QStringList result;
    for (const auto& item : value.toArray()) if (item.isString()) result.append(item.toString());
    return result;
}

bool writeJson(const QString& path, QJsonObject object, QString* error = nullptr, bool onlyIfMissing = false)
{
    if (!object.contains(QStringLiteral("_file"))) object.insert(QStringLiteral("_file"), QFileInfo(path).fileName());
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (onlyIfMissing && QFileInfo::exists(path)) return true;
    if (fileBytes(path) == bytes) return true;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QString entryId(const QString& projectId, const QString& source,
                const QString& section, const QString& scope)
{ return hashBytes((projectId + QLatin1Char('|') + source + QLatin1Char('|') + section + QLatin1Char('|') + scope).toUtf8()).left(20); }

QJsonObject sourceEntry(const ProjectModel& model, const QString& source,
                        const QString& section, const QString& scope,
                        const QString& provenance = {})
{
    const QString path = source.startsWith(QStringLiteral("external:")) ? provenance : absolute(model, source);
    const QString fingerprint = fileFingerprint(path);
    QJsonObject result{
        {QStringLiteral("id"), entryId(model.projectId(), source, section, scope)},
        {QStringLiteral("projectId"), model.projectId()},
        {QStringLiteral("source"), source},
        {QStringLiteral("path"), path},
        {QStringLiteral("scope"), scope},
        {QStringLiteral("section"), section},
        {QStringLiteral("provenance"), QJsonArray{provenance.isEmpty() ? source : provenance}},
        {QStringLiteral("dependencies"), QJsonArray{source}},
        {QStringLiteral("fingerprint"), fingerprint},
        {QStringLiteral("version"), 1},
        {QStringLiteral("status"), fingerprint == QStringLiteral("MISSING") ? QStringLiteral("INVALID") : QStringLiteral("CURRENT")}
    };
    return result;
}

QJsonObject loadIndex(const ProjectModel& model)
{
    QJsonObject index;
    readJson(absolute(model, QStringLiteral("ARAMF_WORKER/context/context-index.json")), &index);
    return index;
}

QJsonObject freshnessFor(const ProjectModel& model, const QJsonObject& index)
{
    QJsonArray states;
    int stale = 0;
    for (const auto& value : index.value(QStringLiteral("entries")).toArray()) {
        const auto entry = value.toObject();
        const QString current = fileFingerprint(entry.value(QStringLiteral("path")).toString());
        const QString recorded = entry.value(QStringLiteral("fingerprint")).toString();
        const QString status = current == QStringLiteral("MISSING") ? QStringLiteral("INVALID") : current == recorded ? QStringLiteral("CURRENT") : QStringLiteral("STALE");
        if (status != QStringLiteral("CURRENT")) ++stale;
        states.append(QJsonObject{{QStringLiteral("id"), entry.value(QStringLiteral("id"))},
                                  {QStringLiteral("source"), entry.value(QStringLiteral("source"))},
                                  {QStringLiteral("scope"), entry.value(QStringLiteral("scope"))},
                                  {QStringLiteral("section"), entry.value(QStringLiteral("section"))},
                                  {QStringLiteral("recordedFingerprint"), recorded},
                                  {QStringLiteral("currentFingerprint"), current},
                                  {QStringLiteral("status"), status}});
    }
    return QJsonObject{{QStringLiteral("_file"), QStringLiteral("freshness.json")}, {QStringLiteral("schemaVersion"), 1},
                       {QStringLiteral("authority"), QStringLiteral("DERIVED")}, {QStringLiteral("projectId"), model.projectId()},
                       {QStringLiteral("indexFingerprint"), index.value(QStringLiteral("fingerprint"))},
                       {QStringLiteral("staleCount"), stale}, {QStringLiteral("entries"), states}};
}

bool scopeMatches(const QString& entryScope, const QStringList& scopes)
{ return entryScope == QStringLiteral("all") || scopes.contains(entryScope); }

QJsonArray filteredEntries(const QJsonArray& entries, const QStringList& scopes, const QString& section)
{
    QJsonArray result;
    for (const auto& value : entries) {
        const auto entry = value.toObject();
        if (!scopeMatches(entry.value(QStringLiteral("scope")).toString(), scopes)) continue;
        if (!section.isEmpty() && entry.value(QStringLiteral("section")).toString() != section) continue;
        result.append(entry);
    }
    return result;
}

QJsonArray sortedNodes(QJsonArray nodes)
{
    QList<QJsonValue> values;
    for (const auto& value : nodes) values.append(value);
    std::sort(values.begin(), values.end(), [](const QJsonValue& a, const QJsonValue& b) {
        return a.toObject().value(QStringLiteral("id")).toString() < b.toObject().value(QStringLiteral("id")).toString();
    });
    QJsonArray result;
    for (const auto& value : values) result.append(value);
    return result;
}

bool hasCycle(const QHash<QString, QJsonObject>& byId, const QString& id,
              QSet<QString>& active, QSet<QString>& visited)
{
    if (active.contains(id)) return true;
    if (visited.contains(id)) return false;
    active.insert(id);
    for (const auto& dependency : jsonStrings(byId.value(id).value(QStringLiteral("dependencies")))) {
        if (!byId.contains(dependency) || hasCycle(byId, dependency, active, visited)) return true;
    }
    active.remove(id);
    visited.insert(id);
    return false;
}
}

QStringList ContextCoordinationService::derivedPaths()
{
    return {QStringLiteral("ARAMF_WORKER/context/context-index.json"),
            QStringLiteral("ARAMF_WORKER/context/compressed-context.json"),
            QStringLiteral("ARAMF_WORKER/context/freshness.json"),
            QStringLiteral("ARAMF_WORKER/context/agent-adapters.json"),
            QStringLiteral("ARAMF_WORKER/context/task-dag.json")};
}

QJsonObject ContextCoordinationService::buildIndex(const ProjectModel& model)
{
    WorkerScope scope(model.workerNameSuffix());
    QJsonArray entries;
    const auto add = [&](const QString& source, const QString& section, const QString& entryScope, const QString& provenance = QString()) {
        const auto entry = sourceEntry(model, source, section, entryScope, provenance);
        if (entry.value(QStringLiteral("fingerprint")).toString() != QStringLiteral("MISSING")) entries.append(entry);
    };
    add(AramfPaths::AgentInstructions, QStringLiteral("governance"), QStringLiteral("all"));
    add(AramfPaths::ProjectStatus, QStringLiteral("project-state"), QStringLiteral("all"));
    add(AramfPaths::CurrentState, QStringLiteral("project-state"), QStringLiteral("all"));
    add(AramfPaths::Decisions, QStringLiteral("decisions"), QStringLiteral("all"));
    add(AramfPaths::FrameworkKnowledge, QStringLiteral("knowledge"), QStringLiteral("all"));
    add(AramfPaths::TaskRoutes, QStringLiteral("routing"), QStringLiteral("all"));
    add(AramfPaths::ScopeRoutes, QStringLiteral("routing"), QStringLiteral("all"));
    add(AramfPaths::ValidationPolicy, QStringLiteral("validation"), QStringLiteral("all"));
    add(AramfPaths::ResourceManifest, QStringLiteral("resources"), QStringLiteral("all"));
    // latest-validation.json is the derived entry point for current
    // validation, not a context dependency. Indexing it would create a
    // circular dependency because validation also reports context freshness.
    add(AramfPaths::WorkerManifest, QStringLiteral("topology"), QStringLiteral("all"));

    for (const auto& resource : model.resources()) {
        if (!resource.enabled || resource.location.isEmpty()) continue;
        if (resource.scopes.isEmpty()) {
            add(QStringLiteral("external:") + resource.id, QStringLiteral("resources"), QStringLiteral("all"), resource.location);
        } else {
            for (const auto& resourceScope : resource.scopes)
                add(QStringLiteral("external:") + resource.id, QStringLiteral("resources"), resourceScope, resource.location);
        }
    }
    const auto academic = model.academicConfiguration();
    if (academic.thesisDocumentation.enabled)
        add(QStringLiteral("ARAMF_WORKER/documentation/documentation-manifest.json"), QStringLiteral("instructions"), QStringLiteral("thesis"));
    if (academic.reportDocumentation.enabled)
        add(QStringLiteral("ARAMF_WORKER/documentation/documentation-manifest.json"), QStringLiteral("instructions"), QStringLiteral("report"));

    QList<QJsonObject> ordered;
    for (const auto& value : entries) ordered.append(value.toObject());
    std::sort(ordered.begin(), ordered.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value(QStringLiteral("id")).toString() < b.value(QStringLiteral("id")).toString();
    });
    entries = {};
    for (const auto& entry : ordered) entries.append(entry);
    QJsonObject result{{QStringLiteral("_file"), QStringLiteral("context-index.json")},
                       {QStringLiteral("schemaVersion"), 1},
                       {QStringLiteral("authority"), QStringLiteral("DERIVED")},
                       {QStringLiteral("canonicalSourcePolicy"), QStringLiteral("Worker manifest and existing canonical files remain authoritative.")},
                       {QStringLiteral("projectId"), model.projectId()},
                       {QStringLiteral("workerIdentity"), AramfPaths::runtimeWorkerDirectoryName()},
                       {QStringLiteral("entries"), entries}};
    result.insert(QStringLiteral("fingerprint"), hashBytes(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    return result;
}

QJsonObject ContextCoordinationService::route(const ProjectModel& model, QStringList scopes,
                                              const QString& section)
{
    WorkerScope workerScope(model.workerNameSuffix());
    scopes.removeDuplicates();
    scopes.sort();
    const auto resolved = WorkerContextResolver::resolve(workerRoot(model), scopes);
    if (!resolved.value(QStringLiteral("valid")).toBool())
        return QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("valid"), false},
                           {QStringLiteral("errorCode"), QStringLiteral("WORKER_SCOPE_UNRESOLVED")},
                           {QStringLiteral("diagnostics"), resolved.value(QStringLiteral("diagnostics"))}};
    const auto index = buildIndex(model);
    return QJsonObject{{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("valid"), true},
                       {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("scopes"), strings(scopes)},
                       {QStringLiteral("section"), section}, {QStringLiteral("indexFingerprint"), index.value(QStringLiteral("fingerprint"))},
                       {QStringLiteral("entries"), filteredEntries(index.value(QStringLiteral("entries")).toArray(), scopes, section)}};
}

QJsonObject ContextCoordinationService::compress(const ProjectModel& model)
{
    const auto index = buildIndex(model);
    QJsonArray compressed;
    for (const auto& value : index.value(QStringLiteral("entries")).toArray()) {
        const auto entry = value.toObject();
        compressed.append(QJsonObject{{QStringLiteral("id"), entry.value(QStringLiteral("id"))},
                                      {QStringLiteral("projectId"), entry.value(QStringLiteral("projectId"))},
                                      {QStringLiteral("scope"), entry.value(QStringLiteral("scope"))},
                                      {QStringLiteral("section"), entry.value(QStringLiteral("section"))},
                                      {QStringLiteral("summary"), QStringLiteral("%1 context from %2").arg(entry.value(QStringLiteral("section")).toString(), entry.value(QStringLiteral("source")).toString())},
                                      {QStringLiteral("source"), entry.value(QStringLiteral("source"))},
                                      {QStringLiteral("sourceFingerprint"), entry.value(QStringLiteral("fingerprint"))},
                                      {QStringLiteral("provenance"), entry.value(QStringLiteral("provenance"))},
                                      {QStringLiteral("status"), entry.value(QStringLiteral("status"))}});
    }
    return QJsonObject{{QStringLiteral("_file"), QStringLiteral("compressed-context.json")},
                       {QStringLiteral("schemaVersion"), 1}, {QStringLiteral("authority"), QStringLiteral("DERIVED")},
                       {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("sourceIndex"), QStringLiteral("context-index.json")},
                       {QStringLiteral("indexFingerprint"), index.value(QStringLiteral("fingerprint"))}, {QStringLiteral("entries"), compressed}};
}

QJsonObject ContextCoordinationService::freshness(const ProjectModel& model)
{
    auto index = loadIndex(model);
    if (index.isEmpty()) index = buildIndex(model);
    return freshnessFor(model, index);
}

QJsonObject ContextCoordinationService::retrieveDecisions(const ProjectModel& model,
                                                           QStringList scopes, bool includeHistory)
{
    const auto routed = route(model, scopes, QStringLiteral("decisions"));
    if (!routed.value(QStringLiteral("valid")).toBool()) return routed;
    auto result = routed;
    result.insert(QStringLiteral("includeHistory"), includeHistory);
    result.insert(QStringLiteral("historyPolicy"), includeHistory ? QStringLiteral("event-log may be loaded explicitly") : QStringLiteral("event-log excluded"));
    return result;
}

QJsonObject ContextCoordinationService::saveTaskDag(const ProjectModel& model,
                                                    QJsonArray nodes,
                                                    const QString& contractId)
{
    WorkerScope scope(model.workerNameSuffix());
    QHash<QString, QJsonObject> byId;
    QJsonArray normalized;
    for (const auto& value : nodes) {
        auto node = value.toObject();
        const QString id = node.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || byId.contains(id)) return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("TASK_DAG_INVALID")}};
        node.insert(QStringLiteral("projectId"), model.projectId());
        node.insert(QStringLiteral("contractId"), contractId.isEmpty() ? node.value(QStringLiteral("contractId")) : contractId);
        if (!node.contains(QStringLiteral("state"))) node.insert(QStringLiteral("state"), QStringLiteral("PENDING"));
        byId.insert(id, node);
        normalized.append(node);
    }
    for (auto it = byId.cbegin(); it != byId.cend(); ++it)
        for (const auto& dependency : jsonStrings(it.value().value(QStringLiteral("dependencies"))))
            if (!byId.contains(dependency)) return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("TASK_DEPENDENCY_MISSING")}};
    QSet<QString> active, visited;
    for (const auto& id : byId.keys()) if (hasCycle(byId, id, active, visited))
        return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("TASK_DAG_CYCLE")}, {QStringLiteral("message"), QStringLiteral("Task dependencies must form a DAG.")}};
    normalized = sortedNodes(normalized);
    QJsonObject result{{QStringLiteral("_file"), QStringLiteral("task-dag.json")}, {QStringLiteral("schemaVersion"), 1},
                       {QStringLiteral("authority"), QStringLiteral("DERIVED")}, {QStringLiteral("projectId"), model.projectId()},
                       {QStringLiteral("contractId"), contractId}, {QStringLiteral("nodes"), normalized}};
    QJsonArray ready, blocked;
    for (const auto& value : normalized) {
        const auto node = value.toObject();
        bool depsComplete = true, depsFailed = false;
        for (const auto& dependency : jsonStrings(node.value(QStringLiteral("dependencies")))) {
            const QString state = byId.value(dependency).value(QStringLiteral("state")).toString();
            depsComplete &= state == QStringLiteral("COMPLETED");
            depsFailed |= state == QStringLiteral("FAILED") || state == QStringLiteral("BLOCKED");
        }
        if (depsFailed || (!jsonStrings(node.value(QStringLiteral("dependencies"))).isEmpty() && !depsComplete)) blocked.append(node.value(QStringLiteral("id")));
        else if (node.value(QStringLiteral("state")).toString() == QStringLiteral("PENDING") || node.value(QStringLiteral("state")).toString() == QStringLiteral("READY")) ready.append(node.value(QStringLiteral("id")));
    }
    result.insert(QStringLiteral("readyNodes"), ready);
    result.insert(QStringLiteral("blockedNodes"), blocked);
    result.insert(QStringLiteral("fingerprint"), hashBytes(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    QString error;
    if (!writeJson(absolute(model, QStringLiteral("ARAMF_WORKER/context/task-dag.json")), result, &error)) {
        result.insert(QStringLiteral("valid"), false); result.insert(QStringLiteral("errorCode"), QStringLiteral("P1_STATE_WRITE_FAILED")); result.insert(QStringLiteral("message"), error); return result;
    }
    result.insert(QStringLiteral("valid"), true);
    return result;
}

QJsonObject ContextCoordinationService::taskDag(const ProjectModel& model)
{
    WorkerScope scope(model.workerNameSuffix());
    QJsonObject result;
    if (!readJson(absolute(model, QStringLiteral("ARAMF_WORKER/context/task-dag.json")), &result))
        return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("TASK_DAG_MISSING")}};
    if (result.value(QStringLiteral("projectId")).toString() != model.projectId())
        return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("PROJECT_ISOLATION_VIOLATION")}};
    result.insert(QStringLiteral("valid"), true);
    return result;
}

QJsonObject ContextCoordinationService::createHandoff(const ProjectModel& model,
                                                       const QJsonObject& contract,
                                                       const QString& sourceAgent,
                                                       const QString& targetAgent,
                                                       QStringList destinationScopes,
                                                       bool persist)
{
    WorkerScope scope(model.workerNameSuffix());
    const auto impact = contract.value(QStringLiteral("impact")).toObject();
    if (freshness(model).value(QStringLiteral("staleCount")).toInt() != 0)
        return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("HANDOFF_CONTEXT_STALE")}, {QStringLiteral("message"), QStringLiteral("Refresh derived context before handoff.")}};
    auto allowed = jsonStrings(impact.value(QStringLiteral("affectedScopes")));
    destinationScopes.removeDuplicates(); destinationScopes.sort();
    for (const auto& requested : destinationScopes) if (!allowed.contains(requested))
        return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("HANDOFF_SCOPE_ESCALATION")}, {QStringLiteral("message"), QStringLiteral("A handoff cannot expand the originating task scope.")}};
    const QString contractId = contract.value(QStringLiteral("contractId")).toString();
    if (contractId.isEmpty() || contract.value(QStringLiteral("binding")).toObject().value(QStringLiteral("projectId")).toString() != model.projectId())
        return {{QStringLiteral("valid"), false}, {QStringLiteral("errorCode"), QStringLiteral("TASK_CONTRACT_INVALID")}};
    const QString id = entryId(model.projectId(), contractId, sourceAgent + targetAgent, destinationScopes.join(","));
    QJsonObject result{{QStringLiteral("_file"), id + QStringLiteral(".json")}, {QStringLiteral("schemaVersion"), 1},
                       {QStringLiteral("authority"), QStringLiteral("DERIVED")}, {QStringLiteral("handoffId"), id},
                       {QStringLiteral("projectId"), model.projectId()}, {QStringLiteral("contractId"), contractId},
                       {QStringLiteral("sourceAgent"), sourceAgent}, {QStringLiteral("targetAgent"), targetAgent},
                       {QStringLiteral("scopes"), strings(destinationScopes)}, {QStringLiteral("permittedFiles"), contract.value(QStringLiteral("permittedFiles"))},
                       {QStringLiteral("requiredEvidence"), contract.value(QStringLiteral("requiredEvidence"))},
                       {QStringLiteral("contextIndex"), QStringLiteral("context-index.json")},
                       {QStringLiteral("provenance"), QJsonArray{QStringLiteral("context/context-index.json"), QStringLiteral("context/freshness.json")}},
                       {QStringLiteral("negativeConstraints"), contract.value(QStringLiteral("negativeConstraints"))}};
    result.insert(QStringLiteral("fingerprint"), hashBytes(QJsonDocument(result).toJson(QJsonDocument::Compact)));
    if (persist) {
        QString error;
        if (!writeJson(QDir(workerRoot(model)).filePath(QStringLiteral("context/handoffs/%1.json").arg(id)), result, &error)) {
            result.insert(QStringLiteral("valid"), false); result.insert(QStringLiteral("errorCode"), QStringLiteral("P1_STATE_WRITE_FAILED")); result.insert(QStringLiteral("message"), error); return result;
        }
    }
    result.insert(QStringLiteral("valid"), true);
    return result;
}

QJsonObject ContextCoordinationService::adapterDescriptors()
{
    return {{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("authority"), QStringLiteral("DERIVED")},
            {QStringLiteral("adapters"), QJsonArray{
                QJsonObject{{QStringLiteral("id"), QStringLiteral("generic")}, {QStringLiteral("presentation"), QStringLiteral("canonical-json")}, {QStringLiteral("governanceOverride"), false}},
                QJsonObject{{QStringLiteral("id"), QStringLiteral("openai-codex")}, {QStringLiteral("presentation"), QStringLiteral("codex-task-contract")}, {QStringLiteral("governanceOverride"), false}},
                QJsonObject{{QStringLiteral("id"), QStringLiteral("gemini")}, {QStringLiteral("presentation"), QStringLiteral("gemini-task-contract")}, {QStringLiteral("governanceOverride"), false}},
                QJsonObject{{QStringLiteral("id"), QStringLiteral("copilot")}, {QStringLiteral("presentation"), QStringLiteral("copilot-task-contract")}, {QStringLiteral("governanceOverride"), false}}
            }}};
}

QJsonObject ContextCoordinationService::adaptContract(const QJsonObject& contract, const QString& adapterId)
{
    QString presentation = QStringLiteral("canonical-json");
    for (const auto& value : adapterDescriptors().value(QStringLiteral("adapters")).toArray())
        if (value.toObject().value(QStringLiteral("id")).toString() == adapterId) presentation = value.toObject().value(QStringLiteral("presentation")).toString();
    return {{QStringLiteral("schemaVersion"), 1}, {QStringLiteral("adapterId"), adapterId}, {QStringLiteral("presentation"), presentation},
            {QStringLiteral("governance"), contract}, {QStringLiteral("governanceOverride"), false},
            {QStringLiteral("fingerprint"), hashBytes(QJsonDocument(contract).toJson(QJsonDocument::Compact))}};
}

QJsonObject ContextCoordinationService::generate(const ProjectModel& model)
{
    WorkerScope scope(model.workerNameSuffix());
    QJsonObject result{{QStringLiteral("success"), false}, {QStringLiteral("generatedFiles"), QJsonArray{}}};
    if (!QDir(workerRoot(model)).exists()) { result.insert(QStringLiteral("error"), QStringLiteral("ARAMF_WORKER does not exist.")); return result; }
    QString error;
    const auto index = buildIndex(model);
    const auto compressed = compress(model);
    const auto fresh = freshnessFor(model, index.isEmpty() ? buildIndex(model) : index);
    const auto adapters = adapterDescriptors();
    const QList<QPair<QString, QJsonObject>> outputs{
        {QStringLiteral("context/context-index.json"), index},
        {QStringLiteral("context/compressed-context.json"), compressed},
        {QStringLiteral("context/freshness.json"), fresh},
        {QStringLiteral("context/agent-adapters.json"), adapters}};
    for (const auto& output : outputs) {
        if (!writeJson(absolute(model, QStringLiteral("ARAMF_WORKER/") + output.first), output.second, &error)) {
            result.insert(QStringLiteral("error"), error); return result;
        }
        auto files = result.value(QStringLiteral("generatedFiles")).toArray(); files.append(AramfPaths::resolveWorkerRelativePath(QStringLiteral("ARAMF_WORKER/") + output.first)); result.insert(QStringLiteral("generatedFiles"), files);
    }
    const QString dagPath = absolute(model, QStringLiteral("ARAMF_WORKER/context/task-dag.json"));
    if (!QFileInfo::exists(dagPath)) {
        auto dag = saveTaskDag(model, {}, {});
        if (!dag.value(QStringLiteral("valid")).toBool()) { result.insert(QStringLiteral("error"), dag.value(QStringLiteral("message"))); return result; }
    }
    result.insert(QStringLiteral("generatedFiles"), result.value(QStringLiteral("generatedFiles")).toArray());
    result.insert(QStringLiteral("success"), true);
    result.insert(QStringLiteral("fingerprint"), index.value(QStringLiteral("fingerprint")));
    return result;
}

QJsonObject ContextCoordinationService::validate(const ProjectModel& model)
{
    WorkerScope scope(model.workerNameSuffix());
    const auto index = loadIndex(model);
    const auto fresh = freshness(model);
    const bool valid = !index.isEmpty() && index.value(QStringLiteral("schemaVersion")).toInt() == 1
        && index.value(QStringLiteral("authority")).toString() == QStringLiteral("DERIVED")
        && index.value(QStringLiteral("projectId")).toString() == model.projectId()
        && fresh.value(QStringLiteral("staleCount")).toInt() == 0;
    return {{QStringLiteral("valid"), valid}, {QStringLiteral("projectId"), model.projectId()},
            {QStringLiteral("indexValid"), !index.isEmpty()}, {QStringLiteral("freshness"), fresh},
            {QStringLiteral("errorCode"), valid ? QJsonValue{} : QJsonValue(QStringLiteral("P1_CONTEXT_STALE_OR_INVALID"))}};
}
