#include "UpdateService.h"

#include "AramfPaths.h"
#include "FrameworkKnowledge.h"
#include "ProjectMemory.h"
#include "Services.h"
#include "ValidationRouting.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

namespace {
bool readObject(const QString& path, QJsonObject* value, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = parseError.errorString();
        return false;
    }
    *value = document.object();
    return true;
}

bool writeObject(const QString& path, const QJsonObject& value, QString* error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    const auto data = QJsonDocument(value).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QStringList projectScopes(const ProjectModel& model)
{
    QStringList scopes = model.ruleConfiguration().projectScopes;
    for (const auto& resource : model.resources()) {
        for (const auto& scope : resource.scopes) if (!scopes.contains(scope)) scopes.append(scope);
    }
    if (scopes.isEmpty()) scopes << QStringLiteral("entire-project");
    return scopes;
}

QStringList frameworkScopesForProject(const ProjectModel& model)
{
    QStringList scopes = projectScopes(model);
    if (model.templateId() == QStringLiteral("pico-2w-visual-designer")
        && !scopes.contains(QStringLiteral("pico-visual-designer"))) {
        scopes.append(QStringLiteral("pico-visual-designer"));
    }
    const QString root = QDir(model.projectPath()).absolutePath();
    const bool aramfProject = QFileInfo::exists(QDir(root).filePath(QStringLiteral("CMakeLists.txt")))
        && QFileInfo::exists(QDir(root).filePath(QStringLiteral("aramf_setup")))
        && QFileInfo::exists(QDir(root).filePath(QStringLiteral("src")));
    if (aramfProject && !scopes.contains(QStringLiteral("all"))) scopes.append(QStringLiteral("all"));
    const auto options = model.generationOptions();
    if (options.generateAgentRules || options.generateRouting || options.generatePlatforms
        || options.generateResources || options.generateMemory || options.generateProvenance) {
        for (const auto& scope : {QStringLiteral("lifecycle"), QStringLiteral("verification"),
                                  QStringLiteral("finalization"), QStringLiteral("selective-generation"),
                                  QStringLiteral("optional-components")}) {
            if (!scopes.contains(scope)) scopes.append(scope);
        }
    }
    return scopes;
}

QString fingerprintFor(const ProjectModel& model,
                       const QList<FrameworkKnowledgeEntry>& selected,
                       const QString& decisions)
{
    QJsonArray knowledge;
    for (const auto& entry : selected) {
        knowledge.append(QJsonObject{{QStringLiteral("id"), entry.id},
                                     {QStringLiteral("status"), entry.status},
                                     {QStringLiteral("reviewStatus"), entry.reviewStatus},
                                     {QStringLiteral("lesson"), entry.lesson},
                                     {QStringLiteral("scopes"), QJsonArray::fromStringList(entry.scopes)}});
    }
    QJsonArray resources;
    for (const auto& resource : model.resources()) {
        resources.append(QJsonObject{{QStringLiteral("id"), resource.id},
                                     {QStringLiteral("location"), resource.location},
                                     {QStringLiteral("authority"), resource.authorityLevel},
                                     {QStringLiteral("scopes"), QJsonArray::fromStringList(resource.scopes)}});
    }
    const QJsonObject input{{QStringLiteral("projectId"), model.projectId()},
                            {QStringLiteral("projectName"), model.projectName()},
                            {QStringLiteral("scopes"), QJsonArray::fromStringList(frameworkScopesForProject(model))},
                            {QStringLiteral("resources"), resources},
                            {QStringLiteral("knowledge"), knowledge},
                            {QStringLiteral("decisions"), decisions}};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(input).toJson(QJsonDocument::Compact),
                                                        QCryptographicHash::Sha256).toHex());
}

QJsonArray stringArray(const QStringList& values)
{
    return QJsonArray::fromStringList(values);
}

bool writeApplicationResult(const QString& projectRoot,
                            const QJsonObject& plan,
                            const QString& status,
                            QString* error)
{
    const QJsonObject result{
        {QStringLiteral("_file"), QStringLiteral("update-application-result.json")},
        {QStringLiteral("planId"), plan.value(QStringLiteral("planId"))},
        {QStringLiteral("status"), status},
        {QStringLiteral("projectRoot"), QDir::cleanPath(projectRoot)},
        {QStringLiteral("selectedFrameworkKnowledge"), plan.value(QStringLiteral("selectedFrameworkKnowledge"))},
        {QStringLiteral("adoptedFrameworkKnowledge"), plan.value(QStringLiteral("adoptedFrameworkKnowledge"))},
        {QStringLiteral("controlPlaneChanges"), QJsonArray{
            QStringLiteral("ARAMF_WORKER/update/update-plan.json"),
            QStringLiteral("ARAMF_WORKER/update/update-contract.json")}},
        {QStringLiteral("actualProjectChanges"), QJsonArray{}},
        {QStringLiteral("completionRule"), QStringLiteral("COMPLETED requires actual managed-project changes and validation, or explicit NO_CHANGE_REQUIRED evidence.")},
        {QStringLiteral("handoff"), QStringLiteral("External agent must perform and validate selected implementation changes before completion.")}
    };
    return writeObject(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/update/update-application-result.json")), result, error);
}
}

UpdateService::UpdateService(QObject* parent)
    : QObject(parent)
{
}

QList<FrameworkKnowledgeEntry> UpdateService::applicableApprovedKnowledge(const QString& projectRoot,
                                                                            const ProjectModel& model,
                                                                            QString* error) const
{
    FrameworkKnowledgeService service;
    return service.approvedEntries(projectRoot, frameworkScopesForProject(model), error);
}

QList<FrameworkKnowledgeEntry> UpdateService::approvedKnowledgeForProject(const QString& projectRoot,
                                                                           const ProjectModel& model,
                                                                           QString* error) const
{
    Q_UNUSED(model);
    FrameworkKnowledgeService service;
    QList<FrameworkKnowledgeEntry> result;
    for (const auto& entry : service.effectiveKnowledgeForProject(projectRoot, error)) {
        if (entry.status == QStringLiteral("approved") && entry.reviewStatus == QStringLiteral("approved") && entry.supersededBy.isEmpty()) result.append(entry);
    }
    return result;
}

UpdateAnalysisResult UpdateService::analyze(const QString& projectRoot,
                                            const ProjectModel& model,
                                            const QStringList& selectedKnowledgeIds) const
{
    UpdateAnalysisResult result;
    result.planPath = QDir(projectRoot).filePath(AramfPaths::UpdatePlan);
    result.contractPath = QDir(projectRoot).filePath(AramfPaths::UpdateContract);
    if (selectedKnowledgeIds.isEmpty()) {
        result.error = QStringLiteral("Select at least one approved Framework Knowledge entry.");
        return result;
    }

    QString knowledgeError;
    const auto approved = approvedKnowledgeForProject(projectRoot, model, &knowledgeError);
    if (!knowledgeError.isEmpty()) {
        result.error = knowledgeError;
        return result;
    }
    QList<FrameworkKnowledgeEntry> selected;
    for (const auto& id : selectedKnowledgeIds) {
        auto it = std::find_if(approved.cbegin(), approved.cend(), [&id](const auto& entry) { return entry.id == id; });
        if (it == approved.cend()) {
            result.error = QStringLiteral("Knowledge is not approved or active: %1").arg(id);
            return result;
        }
        selected.append(*it);
    }

    QString decisions;
    QFile decisionFile(QDir(projectRoot).filePath(AramfPaths::Decisions));
    if (decisionFile.open(QIODevice::ReadOnly | QIODevice::Text)) decisions = QString::fromUtf8(decisionFile.readAll());
    QStringList conflicts;
    for (const auto& entry : selected) {
        if (decisions.contains(QStringLiteral("BLOCKS %1").arg(entry.id), Qt::CaseInsensitive)
            || (decisions.contains(QStringLiteral("requires"), Qt::CaseInsensitive)
                && entry.lesson.contains(QStringLiteral("recommends"), Qt::CaseInsensitive))) {
            conflicts << QStringLiteral("%1 is blocked by a higher-authority durable decision.").arg(entry.id);
        }
    }
    if (!conflicts.isEmpty()) {
        result.blockedByAuthority = true;
        result.status = QStringLiteral("conflict");
    }

    QStringList scopes;
    QStringList systems;
    QStringList expectedAreas;
    for (const auto& entry : selected) {
        for (const auto& scope : entry.scopes) {
            if (!scopes.contains(scope)) scopes.append(scope);
            const QString area = scope == QStringLiteral("lifecycle") || scope == QStringLiteral("verification")
                ? QStringLiteral("validation and lifecycle")
                : scope == QStringLiteral("selective-generation") || scope == QStringLiteral("generation")
                ? QStringLiteral("generation and routing")
                : scope == QStringLiteral("implementation") || scope == QStringLiteral("regression")
                ? QStringLiteral("implementation and tests")
                : scope;
            if (!expectedAreas.contains(area)) expectedAreas.append(area);
        }
        systems << QStringLiteral("project-wide analysis for %1").arg(entry.id);
    }
    const auto validation = ValidationRouting::route({QStringLiteral("src/core/UpdateService.cpp"), QStringLiteral("update-plan")},
                                                      QStringLiteral("update workflow"));
    QJsonArray selectedKnowledge;
    const auto requestedFrameworkScopes = frameworkScopesForProject(model);
    const bool hasImplementationBaseline = QFileInfo::exists(QDir(model.projectPath()).filePath(QStringLiteral("src")));
    bool requiresImplementation = false;
    for (const auto& entry : selected) {
        const bool relevant = requestedFrameworkScopes.contains(QStringLiteral("all"))
            || std::any_of(entry.scopes.cbegin(), entry.scopes.cend(), [&requestedFrameworkScopes](const auto& scope) { return requestedFrameworkScopes.contains(scope); });
        const QString classification = !relevant ? QStringLiteral("NOT_APPLICABLE")
            : hasImplementationBaseline ? QStringLiteral("ALREADY_SATISFIED") : QStringLiteral("APPLICABLE_CHANGE_REQUIRED");
        requiresImplementation |= classification == QStringLiteral("APPLICABLE_CHANGE_REQUIRED");
        selectedKnowledge.append(QJsonObject{{QStringLiteral("id"), entry.id},
                                             {QStringLiteral("title"), entry.title},
                                             {QStringLiteral("lesson"), entry.lesson},
                                             {QStringLiteral("scopes"), stringArray(entry.scopes)},
                                             {QStringLiteral("evidence"), stringArray(entry.evidence)},
                                             {QStringLiteral("status"), entry.status},
                                             {QStringLiteral("reviewStatus"), entry.reviewStatus},
                                             {QStringLiteral("origin"), entry.origin},
                                             {QStringLiteral("originProjectId"), entry.originProjectId},
                                             {QStringLiteral("originalKnowledgeId"), entry.originalKnowledgeId},
                                             {QStringLiteral("portable"), entry.portable},
                                             {QStringLiteral("approvedAt"), entry.approvedAt},
                                             {QStringLiteral("approvalSource"), entry.approvalSource},
                                             {QStringLiteral("supersededBy"), entry.supersededBy},
                                             {QStringLiteral("classification"), classification},
                                             {QStringLiteral("adoption"), relevant ? QStringLiteral("will-adopt") : QStringLiteral("not-adopted")},
                                             {QStringLiteral("implementationRequired"), classification == QStringLiteral("APPLICABLE_CHANGE_REQUIRED")}});
    }
    const QString fingerprint = fingerprintFor(model, selected, decisions);
    const QString planId = QStringLiteral("update-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QJsonObject plan{
        {QStringLiteral("_file"), QStringLiteral("update-plan.json")},
        {QStringLiteral("planId"), planId},
        {QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("selectedFrameworkKnowledge"), selectedKnowledge},
        {QStringLiteral("affectedScopes"), stringArray(scopes)},
        {QStringLiteral("affectedSystems"), stringArray(systems)},
        {QStringLiteral("expectedAreas"), stringArray(expectedAreas)},
        {QStringLiteral("authorityConstraints"), QJsonArray{QStringLiteral("explicit current user instruction"), QStringLiteral("current Source of Truth"), QStringLiteral("current durable project decisions")}},
        {QStringLiteral("conflicts"), stringArray(conflicts)},
        {QStringLiteral("recommendedValidationLevel"), ValidationRouting::levelName(validation.level)},
        {QStringLiteral("inputFingerprint"), fingerprint},
        {QStringLiteral("planFingerprint"), fingerprint},
        {QStringLiteral("projectRoot"), QDir::cleanPath(projectRoot)},
        {QStringLiteral("requiresImplementation"), requiresImplementation},
        {QStringLiteral("status"), result.blockedByAuthority ? QStringLiteral("conflict") : QStringLiteral("prepared")}
    };
    result.plan = plan;
    result.success = writeObject(result.planPath, plan, &result.error);
    if (!result.success) return result;
    result.status = plan.value(QStringLiteral("status")).toString();
    return result;
}

QJsonObject UpdateService::currentPlan(const QString& projectRoot, QString* error) const
{
    QJsonObject plan;
    readObject(QDir(projectRoot).filePath(AramfPaths::UpdatePlan), &plan, error);
    return plan;
}

bool UpdateService::isPlanCurrent(const QString& projectRoot,
                                  const ProjectModel& model,
                                  QString* error) const
{
    const auto plan = currentPlan(projectRoot, error);
    if (plan.isEmpty()) return false;
    if (plan.value(QStringLiteral("status")).toString() == QStringLiteral("stale")
        || plan.value(QStringLiteral("status")).toString() == QStringLiteral("conflict")) return false;
    QList<FrameworkKnowledgeEntry> selected;
    const auto applicable = approvedKnowledgeForProject(projectRoot, model, error);
    if (error && !error->isEmpty()) return false;
    for (const auto& value : plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray()) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString();
        const auto it = std::find_if(applicable.cbegin(), applicable.cend(), [&id](const auto& entry) { return entry.id == id; });
        if (it == applicable.cend()) return false;
        selected.append(*it);
    }
    QFile decisionFile(QDir(projectRoot).filePath(AramfPaths::Decisions));
    QString decisions;
    if (decisionFile.open(QIODevice::ReadOnly | QIODevice::Text)) decisions = QString::fromUtf8(decisionFile.readAll());
    return !selected.isEmpty()
        && selected.size() == plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size()
        && fingerprintFor(model, selected, decisions) == plan.value(QStringLiteral("inputFingerprint")).toString();
}

bool UpdateService::apply(const QString& projectRoot,
                          const ProjectModel& model,
                          QString* error) const
{
    QJsonObject plan = currentPlan(projectRoot, error);
    if (plan.isEmpty()) return false;
    if (plan.value(QStringLiteral("status")).toString() == QStringLiteral("conflict")) {
        if (error) *error = QStringLiteral("Update is blocked by higher authority.");
        return false;
    }
    if (!isPlanCurrent(projectRoot, model, error)) {
        if (error) *error = QStringLiteral("Update plan is stale; analyze again.");
        return false;
    }
    plan.insert(QStringLiteral("plannedProjectAdoption"), plan.value(QStringLiteral("selectedFrameworkKnowledge")));
    plan.insert(QStringLiteral("adoptedFrameworkKnowledge"), QJsonArray{});
    plan.insert(QStringLiteral("status"), QStringLiteral("READY"));
    plan.insert(QStringLiteral("executionState"), QStringLiteral("READY"));
    plan.insert(QStringLiteral("appliedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!writeObject(QDir(projectRoot).filePath(AramfPaths::UpdatePlan), plan, error)) return false;
    const QJsonObject contract{
        {QStringLiteral("_file"), QStringLiteral("update-contract.json")},
        {QStringLiteral("planId"), plan.value(QStringLiteral("planId"))},
        {QStringLiteral("selectedFrameworkKnowledge"), plan.value(QStringLiteral("selectedFrameworkKnowledge"))},
        {QStringLiteral("authorityHierarchy"), QJsonArray{QStringLiteral("explicit current user instruction"), QStringLiteral("current Source of Truth"), QStringLiteral("current durable project decisions"), QStringLiteral("approved applicable Framework Knowledge"), QStringLiteral("templates/defaults"), QStringLiteral("AI inference")}},
        {QStringLiteral("affectedScopes"), plan.value(QStringLiteral("affectedScopes"))},
        {QStringLiteral("expectedAreas"), plan.value(QStringLiteral("expectedAreas"))},
        {QStringLiteral("prohibitedOverrides"), QJsonArray{QStringLiteral("higher-authority project information"), QStringLiteral("unselected Framework Knowledge"), QStringLiteral("candidate or superseded knowledge")}},
        {QStringLiteral("validationLevel"), plan.value(QStringLiteral("recommendedValidationLevel"))},
        {QStringLiteral("memory"), QStringLiteral("Record actual work through ProjectMemory; do not edit managed memory files directly.")},
        {QStringLiteral("executionState"), QStringLiteral("READY")},
        {QStringLiteral("completionStates"), QJsonArray{QStringLiteral("COMPLETED"), QStringLiteral("NO_CHANGE_REQUIRED"), QStringLiteral("FAILED"), QStringLiteral("STALE")}},
        {QStringLiteral("completionRequirement"), QStringLiteral("READY is prepared planning only. Execute performs planned adoption and implementation; validation is required before completion.")}
    };
    if (!writeObject(QDir(projectRoot).filePath(AramfPaths::UpdateContract), contract, error)) return false;
    return writeApplicationResult(projectRoot, plan, QStringLiteral("READY"), error);
}
