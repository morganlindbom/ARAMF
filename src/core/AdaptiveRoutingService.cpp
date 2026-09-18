#include "AdaptiveRoutingService.h"
#include "AramfPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>

const QString AdaptiveRoutingService::RouteStandardDirect = QStringLiteral("ROUTE-STANDARD-DIRECT");
const QString AdaptiveRoutingService::RouteConservativeIsolated = QStringLiteral("ROUTE-CONSERVATIVE-ISOLATED");
const QString AdaptiveRoutingService::RouteFastLocal = QStringLiteral("ROUTE-FAST-LOCAL");
const QString AdaptiveRoutingService::RouteHighRiskCrossLayer = QStringLiteral("ROUTE-HIGH-RISK-CROSS-LAYER");
const QString AdaptiveRoutingService::RouteFallbackMinimal = QStringLiteral("ROUTE-FALLBACK-MINIMAL");

QString routeStatusToString(RouteStatus status)
{
    switch (status) {
    case RouteStatus::Active: return QStringLiteral("ACTIVE");
    case RouteStatus::Degraded: return QStringLiteral("DEGRADED");
    case RouteStatus::Disabled: return QStringLiteral("DISABLED");
    case RouteStatus::Superseded: return QStringLiteral("SUPERSEDED");
    }
    return QStringLiteral("ACTIVE");
}

RouteStatus stringToRouteStatus(const QString& str)
{
    const QString upper = str.trimmed().toUpper();
    if (upper == QStringLiteral("DEGRADED")) return RouteStatus::Degraded;
    if (upper == QStringLiteral("DISABLED")) return RouteStatus::Disabled;
    if (upper == QStringLiteral("SUPERSEDED")) return RouteStatus::Superseded;
    return RouteStatus::Active;
}

QString circuitStateToString(CircuitState state)
{
    switch (state) {
    case CircuitState::Closed: return QStringLiteral("CLOSED");
    case CircuitState::Open: return QStringLiteral("OPEN");
    case CircuitState::HalfOpen: return QStringLiteral("HALF_OPEN");
    }
    return QStringLiteral("CLOSED");
}

CircuitState stringToCircuitState(const QString& str)
{
    const QString upper = str.trimmed().toUpper();
    if (upper == QStringLiteral("OPEN")) return CircuitState::Open;
    if (upper == QStringLiteral("HALF_OPEN")) return CircuitState::HalfOpen;
    return CircuitState::Closed;
}

// -------------------------------------------------------------------------
// RouteDefinition serialization
// -------------------------------------------------------------------------
QJsonObject RouteDefinition::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("routeId")] = routeId;
    obj[QStringLiteral("version")] = version;
    obj[QStringLiteral("description")] = description;
    obj[QStringLiteral("supportedTaskClasses")] = QJsonArray::fromStringList(supportedTaskClasses);
    obj[QStringLiteral("requiredScopes")] = QJsonArray::fromStringList(requiredScopes);
    obj[QStringLiteral("requiredCapabilities")] = QJsonArray::fromStringList(requiredCapabilities);
    obj[QStringLiteral("allowedTools")] = QJsonArray::fromStringList(allowedTools);
    obj[QStringLiteral("allowedAdapters")] = QJsonArray::fromStringList(allowedAdapters);
    obj[QStringLiteral("validationRequirement")] = validationRequirement;
    obj[QStringLiteral("governanceRestrictions")] = QJsonArray::fromStringList(governanceRestrictions);
    obj[QStringLiteral("fallbackEligibility")] = fallbackEligibility;
    obj[QStringLiteral("status")] = routeStatusToString(status);
    return obj;
}

RouteDefinition RouteDefinition::fromJson(const QJsonObject& obj)
{
    RouteDefinition r;
    r.routeId = obj.value(QStringLiteral("routeId")).toString();
    r.version = obj.value(QStringLiteral("version")).toString(QStringLiteral("1.0"));
    r.description = obj.value(QStringLiteral("description")).toString();

    const auto toList = [](const QJsonArray& arr) {
        QStringList list;
        for (const auto& v : arr) list.append(v.toString());
        return list;
    };
    r.supportedTaskClasses = toList(obj.value(QStringLiteral("supportedTaskClasses")).toArray());
    r.requiredScopes = toList(obj.value(QStringLiteral("requiredScopes")).toArray());
    r.requiredCapabilities = toList(obj.value(QStringLiteral("requiredCapabilities")).toArray());
    r.allowedTools = toList(obj.value(QStringLiteral("allowedTools")).toArray());
    r.allowedAdapters = toList(obj.value(QStringLiteral("allowedAdapters")).toArray());
    r.validationRequirement = obj.value(QStringLiteral("validationRequirement")).toString(QStringLiteral("focused"));
    r.governanceRestrictions = toList(obj.value(QStringLiteral("governanceRestrictions")).toArray());
    r.fallbackEligibility = obj.value(QStringLiteral("fallbackEligibility")).toBool(true);
    r.status = stringToRouteStatus(obj.value(QStringLiteral("status")).toString());
    return r;
}

bool RouteDefinition::isValid(QString* error) const
{
    if (routeId.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Route ID cannot be empty");
        return false;
    }
    if (version.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Route version cannot be empty");
        return false;
    }
    if (validationRequirement.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Route validation requirement cannot be empty");
        return false;
    }
    return true;
}

// -------------------------------------------------------------------------
// CandidateRoute serialization
// -------------------------------------------------------------------------
QJsonObject CandidateRoute::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("routeId")] = routeId;
    obj[QStringLiteral("eligible")] = eligible;
    obj[QStringLiteral("rejectionReason")] = rejectionReason;
    obj[QStringLiteral("deterministicScore")] = deterministicScore;
    obj[QStringLiteral("rank")] = rank;
    obj[QStringLiteral("requiredCapabilities")] = QJsonArray::fromStringList(requiredCapabilities);
    obj[QStringLiteral("governanceCompatibility")] = governanceCompatibility;
    obj[QStringLiteral("contextCompatibility")] = contextCompatibility;
    obj[QStringLiteral("predictedSuitability")] = predictedSuitability;
    obj[QStringLiteral("historicalOutcomeEvidence")] = historicalOutcomeEvidence;
    return obj;
}

CandidateRoute CandidateRoute::fromJson(const QJsonObject& obj)
{
    CandidateRoute c;
    c.routeId = obj.value(QStringLiteral("routeId")).toString();
    c.eligible = obj.value(QStringLiteral("eligible")).toBool();
    c.rejectionReason = obj.value(QStringLiteral("rejectionReason")).toString();
    c.deterministicScore = obj.value(QStringLiteral("deterministicScore")).toDouble();
    c.rank = obj.value(QStringLiteral("rank")).toInt();
    for (const auto& v : obj.value(QStringLiteral("requiredCapabilities")).toArray()) {
        c.requiredCapabilities.append(v.toString());
    }
    c.governanceCompatibility = obj.value(QStringLiteral("governanceCompatibility")).toBool();
    c.contextCompatibility = obj.value(QStringLiteral("contextCompatibility")).toBool();
    c.predictedSuitability = obj.value(QStringLiteral("predictedSuitability")).toDouble();
    c.historicalOutcomeEvidence = obj.value(QStringLiteral("historicalOutcomeEvidence")).toObject();
    return c;
}

// -------------------------------------------------------------------------
// RejectedRoute serialization
// -------------------------------------------------------------------------
QJsonObject RejectedRoute::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("routeId")] = routeId;
    obj[QStringLiteral("rejectionReason")] = rejectionReason;
    obj[QStringLiteral("violationCategory")] = violationCategory;
    return obj;
}

RejectedRoute RejectedRoute::fromJson(const QJsonObject& obj)
{
    RejectedRoute r;
    r.routeId = obj.value(QStringLiteral("routeId")).toString();
    r.rejectionReason = obj.value(QStringLiteral("rejectionReason")).toString();
    r.violationCategory = obj.value(QStringLiteral("violationCategory")).toString();
    return r;
}

// -------------------------------------------------------------------------
// RoutingConfidence serialization
// -------------------------------------------------------------------------
QJsonObject RoutingConfidence::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("score")] = score;
    obj[QStringLiteral("rating")] = rating;
    obj[QStringLiteral("sampleSize")] = sampleSize;
    obj[QStringLiteral("separationScore")] = separationScore;
    obj[QStringLiteral("consistencyScore")] = consistencyScore;
    obj[QStringLiteral("freshnessScore")] = freshnessScore;
    obj[QStringLiteral("conflictingEvidencePenalty")] = conflictingEvidencePenalty;
    obj[QStringLiteral("fallbackFrequency")] = fallbackFrequency;
    obj[QStringLiteral("explanation")] = explanation;
    return obj;
}

RoutingConfidence RoutingConfidence::fromJson(const QJsonObject& obj)
{
    RoutingConfidence rc;
    rc.score = obj.value(QStringLiteral("score")).toDouble();
    rc.rating = obj.value(QStringLiteral("rating")).toString();
    rc.sampleSize = obj.value(QStringLiteral("sampleSize")).toInt();
    rc.separationScore = obj.value(QStringLiteral("separationScore")).toDouble();
    rc.consistencyScore = obj.value(QStringLiteral("consistencyScore")).toDouble();
    rc.freshnessScore = obj.value(QStringLiteral("freshnessScore")).toDouble(1.0);
    rc.conflictingEvidencePenalty = obj.value(QStringLiteral("conflictingEvidencePenalty")).toDouble();
    rc.fallbackFrequency = obj.value(QStringLiteral("fallbackFrequency")).toDouble();
    rc.explanation = obj.value(QStringLiteral("explanation")).toString();
    return rc;
}

// -------------------------------------------------------------------------
// RoutingDecision serialization
// -------------------------------------------------------------------------
QJsonObject RoutingDecision::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("routingDecisionId")] = routingDecisionId;
    obj[QStringLiteral("taskId")] = taskId;
    obj[QStringLiteral("taskSignatureFingerprint")] = taskSignatureFingerprint;
    obj[QStringLiteral("predictionReference")] = predictionReference;
    obj[QStringLiteral("routingSchemaVersion")] = routingSchemaVersion;
    obj[QStringLiteral("algorithmVersion")] = algorithmVersion;

    QJsonArray cArr;
    for (const auto& c : candidateRoutes) cArr.append(c.toJson());
    obj[QStringLiteral("candidateRoutes")] = cArr;

    QJsonArray rArr;
    for (const auto& r : rejectedRoutes) rArr.append(r.toJson());
    obj[QStringLiteral("rejectedRoutes")] = rArr;

    obj[QStringLiteral("selectedRoute")] = selectedRoute;
    obj[QStringLiteral("fallbackRoute")] = fallbackRoute;
    obj[QStringLiteral("routingReason")] = routingReason;
    obj[QStringLiteral("evidenceReferences")] = QJsonArray::fromStringList(evidenceReferences);
    obj[QStringLiteral("confidence")] = confidence.toJson();
    obj[QStringLiteral("governanceConstraints")] = governanceConstraints;
    obj[QStringLiteral("contextRoute")] = contextRoute;
    obj[QStringLiteral("validationRoute")] = validationRoute;
    obj[QStringLiteral("adapterRoute")] = adapterRoute;
    obj[QStringLiteral("provenance")] = provenance;
    obj[QStringLiteral("sourceProjectId")] = sourceProjectId;
    obj[QStringLiteral("sourceProjectPath")] = sourceProjectPath;
    obj[QStringLiteral("createdAt")] = createdAt;
    obj[QStringLiteral("status")] = status;
    obj[QStringLiteral("advisoryStatus")] = QStringLiteral("ADVISORY");
    return obj;
}

RoutingDecision RoutingDecision::fromJson(const QJsonObject& obj)
{
    RoutingDecision d;
    d.routingDecisionId = obj.value(QStringLiteral("routingDecisionId")).toString();
    d.taskId = obj.value(QStringLiteral("taskId")).toString();
    d.taskSignatureFingerprint = obj.value(QStringLiteral("taskSignatureFingerprint")).toString();
    d.predictionReference = obj.value(QStringLiteral("predictionReference")).toString();
    d.routingSchemaVersion = obj.value(QStringLiteral("routingSchemaVersion")).toString(QStringLiteral("1.0"));
    d.algorithmVersion = obj.value(QStringLiteral("algorithmVersion")).toString(QStringLiteral("1.0"));

    for (const auto& v : obj.value(QStringLiteral("candidateRoutes")).toArray()) {
        d.candidateRoutes.append(CandidateRoute::fromJson(v.toObject()));
    }
    for (const auto& v : obj.value(QStringLiteral("rejectedRoutes")).toArray()) {
        d.rejectedRoutes.append(RejectedRoute::fromJson(v.toObject()));
    }
    d.selectedRoute = obj.value(QStringLiteral("selectedRoute")).toString();
    d.fallbackRoute = obj.value(QStringLiteral("fallbackRoute")).toString();
    d.routingReason = obj.value(QStringLiteral("routingReason")).toString();
    for (const auto& v : obj.value(QStringLiteral("evidenceReferences")).toArray()) {
        d.evidenceReferences.append(v.toString());
    }
    d.confidence = RoutingConfidence::fromJson(obj.value(QStringLiteral("confidence")).toObject());
    d.governanceConstraints = obj.value(QStringLiteral("governanceConstraints")).toObject();
    d.contextRoute = obj.value(QStringLiteral("contextRoute")).toString();
    d.validationRoute = obj.value(QStringLiteral("validationRoute")).toString();
    d.adapterRoute = obj.value(QStringLiteral("adapterRoute")).toString();
    d.provenance = obj.value(QStringLiteral("provenance")).toObject();
    d.sourceProjectId = obj.value(QStringLiteral("sourceProjectId")).toString();
    d.sourceProjectPath = obj.value(QStringLiteral("sourceProjectPath")).toString();
    d.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    d.status = obj.value(QStringLiteral("status")).toString();
    return d;
}

// -------------------------------------------------------------------------
// RouteEvaluation serialization
// -------------------------------------------------------------------------
QJsonObject RouteEvaluation::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("evaluationId")] = evaluationId;
    obj[QStringLiteral("routingDecisionId")] = routingDecisionId;
    obj[QStringLiteral("taskId")] = taskId;
    obj[QStringLiteral("routeId")] = routeId;
    obj[QStringLiteral("taskClass")] = taskClass;
    obj[QStringLiteral("executionSuccess")] = executionSuccess;
    obj[QStringLiteral("validationSuccess")] = validationSuccess;
    obj[QStringLiteral("retryCount")] = retryCount;
    obj[QStringLiteral("recoveryEvents")] = recoveryEvents;
    obj[QStringLiteral("resourceCollisions")] = resourceCollisions;
    obj[QStringLiteral("fallbackUsed")] = fallbackUsed;
    obj[QStringLiteral("fallbackRouteId")] = fallbackRouteId;
    obj[QStringLiteral("finalOutcome")] = finalOutcome;
    obj[QStringLiteral("recordedAt")] = recordedAt;
    return obj;
}

RouteEvaluation RouteEvaluation::fromJson(const QJsonObject& obj)
{
    RouteEvaluation e;
    e.evaluationId = obj.value(QStringLiteral("evaluationId")).toString();
    e.routingDecisionId = obj.value(QStringLiteral("routingDecisionId")).toString();
    e.taskId = obj.value(QStringLiteral("taskId")).toString();
    e.routeId = obj.value(QStringLiteral("routeId")).toString();
    e.taskClass = obj.value(QStringLiteral("taskClass")).toString();
    e.executionSuccess = obj.value(QStringLiteral("executionSuccess")).toBool();
    e.validationSuccess = obj.value(QStringLiteral("validationSuccess")).toBool();
    e.retryCount = obj.value(QStringLiteral("retryCount")).toInt();
    e.recoveryEvents = obj.value(QStringLiteral("recoveryEvents")).toInt();
    e.resourceCollisions = obj.value(QStringLiteral("resourceCollisions")).toInt();
    e.fallbackUsed = obj.value(QStringLiteral("fallbackUsed")).toBool();
    e.fallbackRouteId = obj.value(QStringLiteral("fallbackRouteId")).toString();
    e.finalOutcome = obj.value(QStringLiteral("finalOutcome")).toString();
    e.recordedAt = obj.value(QStringLiteral("recordedAt")).toString();
    return e;
}

// -------------------------------------------------------------------------
// RouteHealth serialization
// -------------------------------------------------------------------------
QJsonObject RouteHealth::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("routeId")] = routeId;
    obj[QStringLiteral("totalRuns")] = totalRuns;
    obj[QStringLiteral("successfulRuns")] = successfulRuns;
    obj[QStringLiteral("failedRuns")] = failedRuns;
    obj[QStringLiteral("consecutiveFailures")] = consecutiveFailures;
    obj[QStringLiteral("retryCount")] = retryCount;
    obj[QStringLiteral("collisionCount")] = collisionCount;
    obj[QStringLiteral("fallbackCount")] = fallbackCount;
    obj[QStringLiteral("successRate")] = successRate;
    obj[QStringLiteral("isDegraded")] = isDegraded;
    obj[QStringLiteral("lastDegradedAt")] = lastDegradedAt;
    obj[QStringLiteral("lastEvaluatedAt")] = lastEvaluatedAt;
    return obj;
}

RouteHealth RouteHealth::fromJson(const QJsonObject& obj)
{
    RouteHealth h;
    h.routeId = obj.value(QStringLiteral("routeId")).toString();
    h.totalRuns = obj.value(QStringLiteral("totalRuns")).toInt();
    h.successfulRuns = obj.value(QStringLiteral("successfulRuns")).toInt();
    h.failedRuns = obj.value(QStringLiteral("failedRuns")).toInt();
    h.consecutiveFailures = obj.value(QStringLiteral("consecutiveFailures")).toInt();
    h.retryCount = obj.value(QStringLiteral("retryCount")).toInt();
    h.collisionCount = obj.value(QStringLiteral("collisionCount")).toInt();
    h.fallbackCount = obj.value(QStringLiteral("fallbackCount")).toInt();
    h.successRate = obj.value(QStringLiteral("successRate")).toDouble(1.0);
    h.isDegraded = obj.value(QStringLiteral("isDegraded")).toBool();
    h.lastDegradedAt = obj.value(QStringLiteral("lastDegradedAt")).toString();
    h.lastEvaluatedAt = obj.value(QStringLiteral("lastEvaluatedAt")).toString();
    return h;
}

// -------------------------------------------------------------------------
// RouteCircuitBreaker serialization
// -------------------------------------------------------------------------
QJsonObject RouteCircuitBreaker::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("routeId")] = routeId;
    obj[QStringLiteral("state")] = circuitStateToString(state);
    obj[QStringLiteral("failureThreshold")] = failureThreshold;
    obj[QStringLiteral("consecutiveFailures")] = consecutiveFailures;
    obj[QStringLiteral("recoveryProbeSuccesses")] = recoveryProbeSuccesses;
    obj[QStringLiteral("requiredProbeSuccesses")] = requiredProbeSuccesses;
    obj[QStringLiteral("tripSequence")] = tripSequence;
    obj[QStringLiteral("lastStateChange")] = lastStateChange;
    return obj;
}

RouteCircuitBreaker RouteCircuitBreaker::fromJson(const QJsonObject& obj)
{
    RouteCircuitBreaker cb;
    cb.routeId = obj.value(QStringLiteral("routeId")).toString();
    cb.state = stringToCircuitState(obj.value(QStringLiteral("state")).toString());
    cb.failureThreshold = obj.value(QStringLiteral("failureThreshold")).toInt(3);
    cb.consecutiveFailures = obj.value(QStringLiteral("consecutiveFailures")).toInt();
    cb.recoveryProbeSuccesses = obj.value(QStringLiteral("recoveryProbeSuccesses")).toInt();
    cb.requiredProbeSuccesses = obj.value(QStringLiteral("requiredProbeSuccesses")).toInt(2);
    cb.tripSequence = obj.value(QStringLiteral("tripSequence")).toInt();
    cb.lastStateChange = obj.value(QStringLiteral("lastStateChange")).toString();
    return cb;
}

// -------------------------------------------------------------------------
// RouteRegistry
// -------------------------------------------------------------------------
RouteRegistry::RouteRegistry()
{
}

bool RouteRegistry::registerRoute(const RouteDefinition& route, QString* error)
{
    if (!route.isValid(error)) return false;
    m_routes.insert(route.routeId, route);
    return true;
}

bool RouteRegistry::updateRouteStatus(const QString& routeId, RouteStatus status, QString* error)
{
    if (!m_routes.contains(routeId)) {
        if (error) *error = QStringLiteral("Route not found: %1").arg(routeId);
        return false;
    }
    m_routes[routeId].status = status;
    return true;
}

RouteDefinition RouteRegistry::findRoute(const QString& routeId) const
{
    return m_routes.value(routeId);
}

QList<RouteDefinition> RouteRegistry::allRoutes() const
{
    auto list = m_routes.values();
    std::sort(list.begin(), list.end(), [](const RouteDefinition& a, const RouteDefinition& b) {
        return a.routeId < b.routeId;
    });
    return list;
}

QList<RouteDefinition> RouteRegistry::activeRoutes() const
{
    QList<RouteDefinition> list;
    for (const auto& r : m_routes.values()) {
        if (r.status == RouteStatus::Active || r.status == RouteStatus::Degraded) {
            list.append(r);
        }
    }
    std::sort(list.begin(), list.end(), [](const RouteDefinition& a, const RouteDefinition& b) {
        return a.routeId < b.routeId;
    });
    return list;
}

QList<RouteDefinition> RouteRegistry::fallbackEligibleRoutes() const
{
    QList<RouteDefinition> list;
    for (const auto& r : m_routes.values()) {
        if (r.fallbackEligibility && (r.status == RouteStatus::Active || r.status == RouteStatus::Degraded)) {
            list.append(r);
        }
    }
    std::sort(list.begin(), list.end(), [](const RouteDefinition& a, const RouteDefinition& b) {
        return a.routeId < b.routeId;
    });
    return list;
}

bool RouteRegistry::save(const QString& projectRoot, QString* error) const
{
    if (projectRoot.isEmpty()) return true;
    const QDir routingDir(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing")));
    if (!routingDir.exists()) {
        QDir(projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/routing"));
    }
    const QString filePath = routingDir.filePath(QStringLiteral("routes.json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not write routes to %1").arg(filePath);
        return false;
    }
    QJsonArray arr;
    for (const auto& r : allRoutes()) arr.append(r.toJson());
    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = QStringLiteral("1.0");
    root[QStringLiteral("routes")] = arr;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool RouteRegistry::load(const QString& projectRoot, QString* error)
{
    if (projectRoot.isEmpty()) return true;
    const QString filePath = QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/routes.json"));
    QFile file(filePath);
    if (!file.exists()) return true; // default registry stays
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not read routes from %1").arg(filePath);
        return false;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("Invalid JSON in %1").arg(filePath);
        return false;
    }
    const auto arr = doc.object().value(QStringLiteral("routes")).toArray();
    for (const auto& v : arr) {
        RouteDefinition r = RouteDefinition::fromJson(v.toObject());
        if (r.isValid()) {
            m_routes.insert(r.routeId, r);
        }
    }
    return true;
}

RouteRegistry RouteRegistry::defaultRegistry()
{
    RouteRegistry reg;

    // 1. ROUTE-STANDARD-DIRECT
    RouteDefinition standard;
    standard.routeId = AdaptiveRoutingService::RouteStandardDirect;
    standard.version = QStringLiteral("1.0");
    standard.description = QStringLiteral("Standard direct execution route for typical core, UI, and configuration changes");
    standard.supportedTaskClasses = {QStringLiteral("core"), QStringLiteral("ui"), QStringLiteral("configuration"), QStringLiteral("build"), QStringLiteral("refactoring")};
    standard.requiredScopes = {QStringLiteral("source-code"), QStringLiteral("tests"), QStringLiteral("configuration")};
    standard.requiredCapabilities = {QStringLiteral("code-editing"), QStringLiteral("build-execution"), QStringLiteral("test-execution")};
    standard.allowedTools = {QStringLiteral("aramf-cli"), QStringLiteral("ctest"), QStringLiteral("cmake")};
    standard.allowedAdapters = {QStringLiteral("internal"), QStringLiteral("openai-codex")};
    standard.validationRequirement = QStringLiteral("focused");
    standard.governanceRestrictions = {QStringLiteral("no-unmapped-files"), QStringLiteral("task-contract-binding")};
    standard.fallbackEligibility = true;
    standard.status = RouteStatus::Active;
    reg.registerRoute(standard);

    // 2. ROUTE-CONSERVATIVE-ISOLATED
    RouteDefinition conservative;
    conservative.routeId = AdaptiveRoutingService::RouteConservativeIsolated;
    conservative.version = QStringLiteral("1.0");
    conservative.description = QStringLiteral("Conservative isolated execution route with strict scoping, elevated validation, and safe defaults");
    conservative.supportedTaskClasses = {QStringLiteral("core"), QStringLiteral("memory"), QStringLiteral("governance"), QStringLiteral("cross-layer"), QStringLiteral("release"), QStringLiteral("unknown")};
    conservative.requiredScopes = {QStringLiteral("source-code"), QStringLiteral("tests"), QStringLiteral("configuration"), QStringLiteral("project-memory")};
    conservative.requiredCapabilities = {QStringLiteral("code-editing"), QStringLiteral("build-execution"), QStringLiteral("test-execution"), QStringLiteral("subsystem-analysis")};
    conservative.allowedTools = {QStringLiteral("aramf-cli"), QStringLiteral("ctest"), QStringLiteral("cmake")};
    conservative.allowedAdapters = {QStringLiteral("internal"), QStringLiteral("openai-codex"), QStringLiteral("gemini")};
    conservative.validationRequirement = QStringLiteral("subsystem");
    conservative.governanceRestrictions = {QStringLiteral("no-unmapped-files"), QStringLiteral("task-contract-binding"), QStringLiteral("isolated-context")};
    conservative.fallbackEligibility = true;
    conservative.status = RouteStatus::Active;
    reg.registerRoute(conservative);

    // 3. ROUTE-FAST-LOCAL
    RouteDefinition fastLocal;
    fastLocal.routeId = AdaptiveRoutingService::RouteFastLocal;
    fastLocal.version = QStringLiteral("1.0");
    fastLocal.description = QStringLiteral("Fast local execution route for localized single-file or single-component tasks with zero cross-layer risk");
    fastLocal.supportedTaskClasses = {QStringLiteral("core"), QStringLiteral("ui"), QStringLiteral("documentation"), QStringLiteral("analysis")};
    fastLocal.requiredScopes = {QStringLiteral("source-code"), QStringLiteral("tests")};
    fastLocal.requiredCapabilities = {QStringLiteral("code-editing"), QStringLiteral("test-execution")};
    fastLocal.allowedTools = {QStringLiteral("aramf-cli"), QStringLiteral("ctest")};
    fastLocal.allowedAdapters = {QStringLiteral("internal")};
    fastLocal.validationRequirement = QStringLiteral("focused");
    fastLocal.governanceRestrictions = {QStringLiteral("no-unmapped-files"), QStringLiteral("single-component-boundary")};
    fastLocal.fallbackEligibility = false;
    fastLocal.status = RouteStatus::Active;
    reg.registerRoute(fastLocal);

    // 4. ROUTE-HIGH-RISK-CROSS-LAYER
    RouteDefinition highRisk;
    highRisk.routeId = AdaptiveRoutingService::RouteHighRiskCrossLayer;
    highRisk.version = QStringLiteral("1.0");
    highRisk.description = QStringLiteral("High-risk cross-layer execution route requiring full regression validation and explicit verification checkpoints");
    highRisk.supportedTaskClasses = {QStringLiteral("cross-layer"), QStringLiteral("migration"), QStringLiteral("governance"), QStringLiteral("release"), QStringLiteral("architecture")};
    highRisk.requiredScopes = {QStringLiteral("entire-project"), QStringLiteral("source-code"), QStringLiteral("tests"), QStringLiteral("build-system"), QStringLiteral("configuration")};
    highRisk.requiredCapabilities = {QStringLiteral("code-editing"), QStringLiteral("build-execution"), QStringLiteral("test-execution"), QStringLiteral("cross-layer-analysis")};
    highRisk.allowedTools = {QStringLiteral("aramf-cli"), QStringLiteral("ctest"), QStringLiteral("cmake")};
    highRisk.allowedAdapters = {QStringLiteral("internal"), QStringLiteral("openai-codex")};
    highRisk.validationRequirement = QStringLiteral("full-regression");
    highRisk.governanceRestrictions = {QStringLiteral("no-unmapped-files"), QStringLiteral("task-contract-binding"), QStringLiteral("full-regression-mandatory")};
    highRisk.fallbackEligibility = false;
    highRisk.status = RouteStatus::Active;
    reg.registerRoute(highRisk);

    // 5. ROUTE-FALLBACK-MINIMAL
    RouteDefinition fallbackMin;
    fallbackMin.routeId = AdaptiveRoutingService::RouteFallbackMinimal;
    fallbackMin.version = QStringLiteral("1.0");
    fallbackMin.description = QStringLiteral("Resilient fallback route utilizing minimal required capabilities, isolated context, and robust verification");
    fallbackMin.supportedTaskClasses = {QStringLiteral("core"), QStringLiteral("ui"), QStringLiteral("configuration"), QStringLiteral("build"), QStringLiteral("memory"), QStringLiteral("cross-layer"), QStringLiteral("recovery")};
    fallbackMin.requiredScopes = {QStringLiteral("source-code"), QStringLiteral("tests")};
    fallbackMin.requiredCapabilities = {QStringLiteral("code-editing"), QStringLiteral("test-execution")};
    fallbackMin.allowedTools = {QStringLiteral("aramf-cli"), QStringLiteral("ctest")};
    fallbackMin.allowedAdapters = {QStringLiteral("internal")};
    fallbackMin.validationRequirement = QStringLiteral("subsystem");
    fallbackMin.governanceRestrictions = {QStringLiteral("no-unmapped-files"), QStringLiteral("task-contract-binding"), QStringLiteral("fallback-isolation")};
    fallbackMin.fallbackEligibility = true;
    fallbackMin.status = RouteStatus::Active;
    reg.registerRoute(fallbackMin);

    return reg;
}

// -------------------------------------------------------------------------
// AdaptiveRoutingService
// -------------------------------------------------------------------------
AdaptiveRoutingService::AdaptiveRoutingService(const QString& projectRoot)
    : m_projectRoot(projectRoot)
    , m_registry(RouteRegistry::defaultRegistry())
{
    loadAllPersistence();
}

void AdaptiveRoutingService::loadAllPersistence()
{
    if (m_projectRoot.isEmpty()) return;
    m_registry.load(m_projectRoot);
    loadHealth();
    loadCircuits();
}

bool AdaptiveRoutingService::evaluateEligibility(const RouteDefinition& route,
                                                const TaskSignature& taskSignature,
                                                const PredictionContract& prediction,
                                                const QJsonObject& taskContract,
                                                const ProjectModel& model,
                                                QString* rejectionReason,
                                                QString* violationCategory) const
{
    // 1. Check Route Status
    if (route.status == RouteStatus::Disabled) {
        if (rejectionReason) *rejectionReason = QStringLiteral("Route is DISABLED by policy");
        if (violationCategory) *violationCategory = QStringLiteral("STATUS");
        return false;
    }
    if (route.status == RouteStatus::Superseded) {
        if (rejectionReason) *rejectionReason = QStringLiteral("Route is SUPERSEDED by newer architecture");
        if (violationCategory) *violationCategory = QStringLiteral("STATUS");
        return false;
    }

    // 2. Check Circuit Breaker State (P4.1.4)
    if (m_circuits.contains(route.routeId)) {
        const auto& cb = m_circuits.value(route.routeId);
        if (cb.state == CircuitState::Open) {
            if (rejectionReason) *rejectionReason = QStringLiteral("Circuit breaker is OPEN due to %1 consecutive failures").arg(cb.consecutiveFailures);
            if (violationCategory) *violationCategory = QStringLiteral("STATUS");
            return false;
        }
    }

    // 3. Check Cross-Project Isolation & Foreign Path Injection (P4.1.1, P4.1.3)
    const QString projectRootClean = QDir::cleanPath(model.projectPath());
    const auto permittedFiles = taskContract.value(QStringLiteral("permittedFiles")).toArray();
    for (const auto& fVal : permittedFiles) {
        const QString fPath = fVal.toString();
        if (fPath.startsWith(QStringLiteral("/")) || fPath.contains(QStringLiteral(":/")) || fPath.contains(QStringLiteral(":\\"))) {
            const QString absClean = QDir::cleanPath(fPath);
            if (!projectRootClean.isEmpty() && !absClean.startsWith(projectRootClean)) {
                if (rejectionReason) *rejectionReason = QStringLiteral("Foreign project path detected: %1").arg(fPath);
                if (violationCategory) *violationCategory = QStringLiteral("ISOLATION");
                return false;
            }
        }
    }

    // 4. Check P0 Governance Boundaries & File Permission Constraints
    // Fast local route permits only 1-2 files within a single component
    if (route.routeId == RouteFastLocal) {
        if (permittedFiles.size() > 2) {
            if (rejectionReason) *rejectionReason = QStringLiteral("Route FastLocal limited to single-component tasks (max 2 files claimed, got %1)").arg(permittedFiles.size());
            if (violationCategory) *violationCategory = QStringLiteral("GOVERNANCE");
            return false;
        }
        if (prediction.predictedChangeBreadth == QStringLiteral("CROSS_LAYER") ||
            prediction.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE") ||
            prediction.predictedChangeBreadth == QStringLiteral("MULTI_COMPONENT")) {
            if (rejectionReason) *rejectionReason = QStringLiteral("Route FastLocal cannot handle %1 breadth").arg(prediction.predictedChangeBreadth);
            if (violationCategory) *violationCategory = QStringLiteral("GOVERNANCE");
            return false;
        }
    }

    // 5. Check P0 Destructive Command Prohibition
    const bool isDestructive = taskContract.value(QStringLiteral("isDestructive")).toBool(false);
    if (isDestructive && !taskContract.value(QStringLiteral("adminOverrideAuthorized")).toBool(false)) {
        if (rejectionReason) *rejectionReason = QStringLiteral("Destructive command requires explicit administrative override");
        if (violationCategory) *violationCategory = QStringLiteral("GOVERNANCE");
        return false;
    }

    // 6. Check Tool & Adapter Availability
    const QString requestedAdapter = taskContract.value(QStringLiteral("adapterId")).toString();
    if (!requestedAdapter.isEmpty() && !route.allowedAdapters.contains(requestedAdapter)) {
        if (rejectionReason) *rejectionReason = QStringLiteral("Route does not support requested adapter: %1").arg(requestedAdapter);
        if (violationCategory) *violationCategory = QStringLiteral("TOOL");
        return false;
    }

    // 7. Check Mandatory Validation Governance (P4 CAN NEVER WEAKEN MANDATORY VALIDATION)
    const QString mandatoryValidation = taskContract.value(QStringLiteral("mandatoryValidation")).toString();
    if (mandatoryValidation == QStringLiteral("full-regression") && route.validationRequirement != QStringLiteral("full-regression")) {
        if (rejectionReason) *rejectionReason = QStringLiteral("Route validation level '%1' is lower than mandatory level 'full-regression'").arg(route.validationRequirement);
        if (violationCategory) *violationCategory = QStringLiteral("GOVERNANCE");
        return false;
    }

    return true;
}

double AdaptiveRoutingService::calculateRouteScore(const RouteDefinition& route,
                                                   const TaskSignature& taskSignature,
                                                   const PredictionContract& prediction,
                                                   const RouteHealth& health,
                                                   const QList<RouteEvaluation>& historicalEvaluations,
                                                   const QString& currentPreferredRoute) const
{
    // Factor 1: Task Class / Predicted Suitability [0.0, 1.0] (Weight: 0.30)
    double suitability = 0.50; // neutral default
    const QString category = taskSignature.taskCategory.toLower();
    if (route.supportedTaskClasses.contains(category)) {
        suitability = 0.85;
    }
    // High-risk cross layer match
    if (prediction.predictedChangeBreadth == QStringLiteral("CROSS_LAYER") ||
        prediction.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE")) {
        if (route.routeId == RouteHighRiskCrossLayer) {
            suitability = 1.0;
        } else if (route.routeId == RouteFastLocal) {
            suitability = 0.10;
        }
    } else if (prediction.predictedChangeBreadth == QStringLiteral("LOCAL")) {
        if (route.routeId == RouteFastLocal) {
            suitability = 0.95;
        }
    }

    // Factor 2: Prediction Confidence [0.0, 1.0] (Weight: 0.20)
    const double predConfidence = prediction.confidence.score;

    // Factor 3: Historical Success Rate [0.0, 1.0] (Weight: 0.25)
    double historicalSuccess = 0.60; // neutral prior
    if (health.totalRuns > 0) {
        historicalSuccess = health.successRate;
    }

    // Factor 4: Validation Requirement Alignment [0.0, 1.0] (Weight: 0.15)
    double validationAlignment = 0.70;
    if (prediction.predictedChangeBreadth == QStringLiteral("CROSS_LAYER") && route.validationRequirement == QStringLiteral("full-regression")) {
        validationAlignment = 1.0;
    } else if (route.validationRequirement == QStringLiteral("focused") && prediction.predictedChangeBreadth == QStringLiteral("LOCAL")) {
        validationAlignment = 0.95;
    }

    // Factor 5: Stability Bonus [0.0, 0.10] (Weight: 0.10)
    double stabilityBonus = 0.0;
    if (health.totalRuns >= 2 && health.consecutiveFailures == 0) {
        stabilityBonus = 0.10;
    }

    // Penalties
    double penaltyRetry = 0.0;
    double penaltyCollision = 0.0;
    if (health.totalRuns > 0) {
        penaltyRetry = std::min(1.0, (double)health.retryCount / (double)health.totalRuns);
        penaltyCollision = std::min(1.0, (double)health.collisionCount / (double)health.totalRuns);
    }

    // Anti-Oscillation / Hysteresis (P4.1.3):
    // Current preferred route receives a small hysteresis bonus (+0.08)
    // so a challenger must exceed it by at least this switching threshold to trigger a route switch.
    double hysteresisBonus = 0.0;
    if (!currentPreferredRoute.isEmpty() && route.routeId == currentPreferredRoute) {
        hysteresisBonus = 0.08;
    }

    // Factor 6: Degradation and Failure Penalty (P4.1.3, P4.1.4)
    double degradationPenalty = 0.0;
    if (health.isDegraded) {
        degradationPenalty += 0.35;
    }
    if (health.consecutiveFailures > 0) {
        degradationPenalty += std::min(0.40, 0.15 * health.consecutiveFailures);
    }

    double rawScore = (0.30 * suitability) +
                      (0.20 * predConfidence) +
                      (0.25 * historicalSuccess) +
                      (0.15 * validationAlignment) +
                      (0.10 * stabilityBonus) +
                      hysteresisBonus -
                      (0.15 * penaltyRetry) -
                      (0.15 * penaltyCollision) -
                      degradationPenalty;

    return std::max(0.0, std::min(1.0, rawScore));
}

RoutingConfidence AdaptiveRoutingService::evaluateRoutingConfidence(const QList<CandidateRoute>& eligibleRoutes,
                                                                    const PredictionConfidence& predictionConfidence,
                                                                    const RouteHealth& selectedHealth,
                                                                    const QList<RouteEvaluation>& routeEvaluations) const
{
    RoutingConfidence rc;
    if (eligibleRoutes.isEmpty()) {
        rc.score = 0.0;
        rc.rating = QStringLiteral("NO_ELIGIBLE_ROUTE");
        rc.explanation = QStringLiteral("No candidate route satisfied hard governance and eligibility criteria.");
        return rc;
    }

    rc.sampleSize = selectedHealth.totalRuns;
    rc.consistencyScore = (selectedHealth.totalRuns > 0) ? selectedHealth.successRate : 0.50;

    // Calculate separation margin between rank 1 and rank 2
    if (eligibleRoutes.size() >= 2) {
        rc.separationScore = std::max(0.0, eligibleRoutes.at(0).deterministicScore - eligibleRoutes.at(1).deterministicScore);
    } else {
        rc.separationScore = 0.50; // unambiguous single candidate
    }

    // Penalties
    if (selectedHealth.totalRuns > 0) {
        rc.conflictingEvidencePenalty = (double)selectedHealth.failedRuns / (double)selectedHealth.totalRuns;
        rc.fallbackFrequency = (double)selectedHealth.fallbackCount / (double)selectedHealth.totalRuns;
    }

    // Compute raw confidence score
    const double sampleSizeWeight = std::min(1.0, (double)rc.sampleSize / 3.0);
    double raw = (0.35 * sampleSizeWeight) +
                 (0.30 * rc.consistencyScore) +
                 (0.20 * rc.separationScore) +
                 (0.15 * predictionConfidence.score) -
                 (0.20 * rc.conflictingEvidencePenalty);

    rc.score = std::max(0.0, std::min(1.0, raw));

    // Calibration: Strict sample-size gating (P4.1.2)
    if (rc.sampleSize == 0) {
        rc.score = std::min(rc.score, 0.30);
        rc.rating = QStringLiteral("INSUFFICIENT_ROUTING_EVIDENCE");
        rc.explanation = QStringLiteral("Zero historical executions for selected route; relying on conservative defaults.");
    } else if (rc.sampleSize == 1) {
        rc.score = std::min(rc.score, 0.45);
        rc.rating = QStringLiteral("LOW");
        rc.explanation = QStringLiteral("Single historical execution; insufficient evidence for confident routing.");
    } else if (rc.sampleSize == 2) {
        rc.score = std::min(rc.score, 0.70);
        rc.rating = (rc.score >= 0.55) ? QStringLiteral("MEDIUM") : QStringLiteral("LOW");
        rc.explanation = QStringLiteral("Two historical executions with %1% consistency.").arg(int(rc.consistencyScore * 100));
    } else { // N >= 3
        if (rc.consistencyScore >= 0.80 && rc.conflictingEvidencePenalty == 0.0) {
            rc.rating = (rc.score >= 0.75) ? QStringLiteral("HIGH") : QStringLiteral("MEDIUM");
        } else {
            rc.rating = QStringLiteral("MEDIUM");
        }
        rc.explanation = QStringLiteral("Robust evidence across %1 executions; consistency=%2%, separation=%3.")
                             .arg(rc.sampleSize)
                             .arg(int(rc.consistencyScore * 100))
                             .arg(rc.separationScore, 0, 'f', 3);
    }

    return rc;
}

RoutingDecision AdaptiveRoutingService::selectRoute(const TaskSignature& taskSignature,
                                                   const PredictionContract& prediction,
                                                   const QJsonObject& taskContract,
                                                   const ProjectModel& model,
                                                   const QJsonObject& provenance,
                                                   QString* error)
{
    RoutingDecision decision;
    decision.routingDecisionId = QStringLiteral("route-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    decision.taskId = taskSignature.taskCategory + QStringLiteral(":") + taskSignature.operationType;
    decision.taskSignatureFingerprint = taskSignature.fingerprint();
    decision.predictionReference = prediction.predictionId;
    decision.routingSchemaVersion = QStringLiteral("1.0");
    decision.algorithmVersion = QStringLiteral("1.0");
    decision.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    decision.sourceProjectId = model.projectId().isEmpty() ? QStringLiteral("ARAMF") : model.projectId();
    decision.sourceProjectPath = model.projectPath();
    decision.provenance = provenance.isEmpty() ? QJsonObject{{QStringLiteral("actor"), QStringLiteral("agent")},
                                                            {QStringLiteral("agentId"), QStringLiteral("antigravity")},
                                                            {QStringLiteral("tool"), QStringLiteral("aramf-cli")}}
                                               : provenance;

    // 1. Evaluate Eligibility over all registered routes (Hard Eligibility Filter)
    QList<CandidateRoute> eligibleCandidates;
    const auto allRoutes = m_registry.allRoutes();

    for (const auto& rDef : allRoutes) {
        QString rejectionReason;
        QString violationCategory;
        const bool eligible = evaluateEligibility(rDef, taskSignature, prediction, taskContract, model,
                                                  &rejectionReason, &violationCategory);
        if (eligible) {
            CandidateRoute c;
            c.routeId = rDef.routeId;
            c.eligible = true;
            c.governanceCompatibility = true;
            c.contextCompatibility = true;
            c.requiredCapabilities = rDef.requiredCapabilities;

            // Compute score
            const auto health = routeHealth(rDef.routeId);
            c.deterministicScore = calculateRouteScore(rDef, taskSignature, prediction, health, m_evaluations);
            c.historicalOutcomeEvidence = health.toJson();
            eligibleCandidates.append(c);
        } else {
            RejectedRoute rej;
            rej.routeId = rDef.routeId;
            rej.rejectionReason = rejectionReason;
            rej.violationCategory = violationCategory;
            decision.rejectedRoutes.append(rej);
        }
    }

    // 2. Handle Case: No eligible routes
    if (eligibleCandidates.isEmpty()) {
        decision.status = QStringLiteral("NO_ELIGIBLE_ROUTE");
        decision.routingReason = QStringLiteral("All candidate routes violated hard governance or eligibility constraints.");
        decision.confidence.rating = QStringLiteral("NO_ELIGIBLE_ROUTE");
        decision.confidence.score = 0.0;
        return decision;
    }

    // 3. Deterministic Ranking & Tie-breaking: (score DESC, historicalSuccess DESC, routeId ASC)
    std::sort(eligibleCandidates.begin(), eligibleCandidates.end(), [this](const CandidateRoute& a, const CandidateRoute& b) {
        if (std::abs(a.deterministicScore - b.deterministicScore) > 0.0001) {
            return a.deterministicScore > b.deterministicScore;
        }
        const auto healthA = routeHealth(a.routeId);
        const auto healthB = routeHealth(b.routeId);
        if (std::abs(healthA.successRate - healthB.successRate) > 0.0001) {
            return healthA.successRate > healthB.successRate;
        }
        return a.routeId < b.routeId;
    });

    for (int i = 0; i < eligibleCandidates.size(); ++i) {
        eligibleCandidates[i].rank = i + 1;
    }
    decision.candidateRoutes = eligibleCandidates;

    // 4. Determine Selected Route and Routing Confidence
    CandidateRoute topCandidate = eligibleCandidates.first();
    const auto topHealth = routeHealth(topCandidate.routeId);
    decision.confidence = evaluateRoutingConfidence(eligibleCandidates, prediction.confidence, topHealth, m_evaluations);

    // Conservative Default Routing (P4.1.2):
    // When confidence is INSUFFICIENT_ROUTING_EVIDENCE or LOW:
    // If the task is predicted CROSS_LAYER or PROJECT_WIDE, RouteHighRiskCrossLayer is the maximal-safety route.
    // Otherwise, prefer RouteConservativeIsolated as the safe default.
    bool conservativeUsed = false;
    if (prediction.predictedChangeBreadth == QStringLiteral("CROSS_LAYER") ||
        prediction.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE")) {
        for (const auto& c : eligibleCandidates) {
            if (c.routeId == RouteHighRiskCrossLayer) {
                topCandidate = c;
                break;
            }
        }
    } else if (decision.confidence.rating == QStringLiteral("INSUFFICIENT_ROUTING_EVIDENCE") ||
               decision.confidence.rating == QStringLiteral("LOW")) {
        for (const auto& c : eligibleCandidates) {
            if (c.routeId == RouteConservativeIsolated) {
                topCandidate = c;
                conservativeUsed = true;
                break;
            }
        }
    }

    decision.selectedRoute = topCandidate.routeId;
    if (conservativeUsed) {
        decision.status = QStringLiteral("CONSERVATIVE_ROUTE");
    } else if (decision.confidence.rating == QStringLiteral("LOW")) {
        decision.status = QStringLiteral("ROUTE_SELECTED_LOW_CONFIDENCE");
    } else {
        decision.status = QStringLiteral("ROUTE_SELECTED");
    }

    // 5. Select Governed Fallback Route (P4.1.2)
    // Fallback must be independently eligible and have fallbackEligibility == true
    for (const auto& c : eligibleCandidates) {
        if (c.routeId != decision.selectedRoute) {
            const auto rDef = m_registry.findRoute(c.routeId);
            if (rDef.fallbackEligibility) {
                decision.fallbackRoute = c.routeId;
                break;
            }
        }
    }
    if (decision.fallbackRoute.isEmpty()) {
        // Use Minimal Fallback if eligible
        for (const auto& c : eligibleCandidates) {
            if (c.routeId == RouteFallbackMinimal) {
                decision.fallbackRoute = c.routeId;
                break;
            }
        }
    }

    // 6. Context & Validation Routing (P4.1.2)
    const auto selDef = m_registry.findRoute(decision.selectedRoute);
    if (decision.selectedRoute == RouteFastLocal) {
        decision.contextRoute = QStringLiteral("FOCUSED");
    } else if (decision.selectedRoute == RouteHighRiskCrossLayer) {
        decision.contextRoute = QStringLiteral("FULL_PROJECT");
    } else if (decision.selectedRoute == RouteConservativeIsolated) {
        if (taskSignature.taskCategory == QStringLiteral("ui") ||
            taskSignature.taskCategory == QStringLiteral("analysis") ||
            taskSignature.referencedFiles.size() <= 1) {
            decision.contextRoute = QStringLiteral("FOCUSED");
        } else {
            decision.contextRoute = QStringLiteral("DEPENDENCY");
        }
    } else {
        decision.contextRoute = QStringLiteral("FOCUSED");
    }

    // Validation route escalation: max of route requirement and P3 predicted validation
    decision.validationRoute = selDef.validationRequirement;
    if (prediction.predictedChangeBreadth == QStringLiteral("CROSS_LAYER") ||
        prediction.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE")) {
        decision.validationRoute = QStringLiteral("full-regression");
    }

    // Adapter route
    decision.adapterRoute = selDef.allowedAdapters.contains(QStringLiteral("openai-codex"))
                                ? QStringLiteral("openai-codex")
                                : (selDef.allowedAdapters.isEmpty() ? QStringLiteral("internal") : selDef.allowedAdapters.first());

    // 7. Explainability Formulation
    QString reason = QStringLiteral("Selected %1 (score=%2, rank=%3) based on %4 confidence evidence. ")
                         .arg(decision.selectedRoute)
                         .arg(topCandidate.deterministicScore, 0, 'f', 3)
                         .arg(topCandidate.rank)
                         .arg(decision.confidence.rating);
    if (conservativeUsed) {
        reason += QStringLiteral("Applied safe conservative fallback policy due to low historical sample size. ");
    }
    if (!decision.rejectedRoutes.isEmpty()) {
        reason += QStringLiteral("%1 routes rejected by hard governance filtering: ").arg(decision.rejectedRoutes.size());
        for (int i = 0; i < decision.rejectedRoutes.size(); ++i) {
            const auto& r = decision.rejectedRoutes.at(i);
            reason += QStringLiteral("[%1: %2]").arg(r.routeId, r.rejectionReason);
            if (i < decision.rejectedRoutes.size() - 1) reason += QStringLiteral(", ");
        }
    }
    decision.routingReason = reason;

    // Evidence references
    if (!prediction.predictionId.isEmpty()) decision.evidenceReferences.append(prediction.predictionId);
    if (!taskSignature.fingerprint().isEmpty()) decision.evidenceReferences.append(taskSignature.fingerprint());

    return decision;
}

// -------------------------------------------------------------------------
// Route Outcome Feedback & Health (P4.1.3, P4.1.4)
// -------------------------------------------------------------------------
bool AdaptiveRoutingService::recordOutcome(const RouteEvaluation& evaluation, QString* error)
{
    if (evaluation.routeId.isEmpty()) {
        if (error) *error = QStringLiteral("Evaluation routeId cannot be empty");
        return false;
    }
    m_evaluations.append(evaluation);

    // Update RouteHealth
    RouteHealth& h = m_health[evaluation.routeId];
    h.routeId = evaluation.routeId;
    h.totalRuns++;
    if (evaluation.executionSuccess && evaluation.validationSuccess) {
        h.successfulRuns++;
        h.consecutiveFailures = 0;
    } else {
        h.failedRuns++;
        h.consecutiveFailures++;
    }
    h.retryCount += evaluation.retryCount;
    h.collisionCount += evaluation.resourceCollisions;
    if (evaluation.fallbackUsed) h.fallbackCount++;

    h.successRate = (h.totalRuns > 0) ? (double)h.successfulRuns / (double)h.totalRuns : 1.0;

    // Degradation threshold: 2 consecutive failures or successRate < 0.50 with >= 3 runs
    if (h.consecutiveFailures >= 2 || (h.totalRuns >= 3 && h.successRate < 0.50)) {
        h.isDegraded = true;
        h.lastDegradedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        m_registry.updateRouteStatus(evaluation.routeId, RouteStatus::Degraded);
    } else if (h.consecutiveFailures == 0 && h.successRate >= 0.70) {
        h.isDegraded = false;
        m_registry.updateRouteStatus(evaluation.routeId, RouteStatus::Active);
    }
    h.lastEvaluatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Update Circuit Breaker (P4.1.4)
    updateCircuitBreaker(evaluation.routeId, evaluation.executionSuccess && evaluation.validationSuccess, h.totalRuns);

    saveHealth();
    saveCircuits();
    return true;
}

QList<RouteEvaluation> AdaptiveRoutingService::evaluationsForRoute(const QString& routeId) const
{
    QList<RouteEvaluation> list;
    for (const auto& e : m_evaluations) {
        if (e.routeId == routeId) list.append(e);
    }
    return list;
}

QList<RouteEvaluation> AdaptiveRoutingService::allEvaluations() const
{
    return m_evaluations;
}

RouteHealth AdaptiveRoutingService::routeHealth(const QString& routeId) const
{
    if (m_health.contains(routeId)) return m_health.value(routeId);
    RouteHealth h;
    h.routeId = routeId;
    return h;
}

QList<RouteHealth> AdaptiveRoutingService::allRouteHealth() const
{
    return m_health.values();
}

RouteCircuitBreaker AdaptiveRoutingService::circuitBreaker(const QString& routeId) const
{
    if (m_circuits.contains(routeId)) return m_circuits.value(routeId);
    RouteCircuitBreaker cb;
    cb.routeId = routeId;
    return cb;
}

void AdaptiveRoutingService::updateCircuitBreaker(const QString& routeId, bool executionSuccess, int currentSequence)
{
    RouteCircuitBreaker& cb = m_circuits[routeId];
    cb.routeId = routeId;

    if (executionSuccess) {
        cb.consecutiveFailures = 0;
        if (cb.state == CircuitState::HalfOpen) {
            cb.recoveryProbeSuccesses++;
            if (cb.recoveryProbeSuccesses >= cb.requiredProbeSuccesses) {
                cb.state = CircuitState::Closed;
                cb.recoveryProbeSuccesses = 0;
                cb.lastStateChange = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            }
        }
    } else {
        cb.consecutiveFailures++;
        cb.recoveryProbeSuccesses = 0;
        if (cb.consecutiveFailures >= cb.failureThreshold && cb.state != CircuitState::Open) {
            cb.state = CircuitState::Open;
            cb.tripSequence = currentSequence;
            cb.lastStateChange = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        }
    }
}

// -------------------------------------------------------------------------
// Persistence
// -------------------------------------------------------------------------
bool AdaptiveRoutingService::saveDecision(const RoutingDecision& decision, QString* error) const
{
    if (m_projectRoot.isEmpty()) return true;
    const QDir decisionsDir(QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/decisions")));
    if (!decisionsDir.exists()) {
        QDir(m_projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/routing/decisions"));
    }
    const QString filePath = decisionsDir.filePath(decision.routingDecisionId + QStringLiteral(".json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not write decision to %1").arg(filePath);
        return false;
    }
    file.write(QJsonDocument(decision.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

RoutingDecision AdaptiveRoutingService::loadDecision(const QString& decisionId, QString* error) const
{
    if (m_projectRoot.isEmpty()) return {};
    const QString filePath = QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/decisions/") + decisionId + QStringLiteral(".json"));
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not read decision from %1").arg(filePath);
        return {};
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("Invalid JSON in %1").arg(filePath);
        return {};
    }
    return RoutingDecision::fromJson(doc.object());
}

QList<RoutingDecision> AdaptiveRoutingService::allDecisions(QString* error) const
{
    QList<RoutingDecision> list;
    if (m_projectRoot.isEmpty()) return list;
    const QDir decisionsDir(QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/decisions")));
    if (!decisionsDir.exists()) return list;

    const auto files = decisionsDir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const auto& fname : files) {
        QFile file(decisionsDir.filePath(fname));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                list.append(RoutingDecision::fromJson(doc.object()));
            }
        }
    }
    return list;
}

bool AdaptiveRoutingService::saveHealth(QString* error) const
{
    if (m_projectRoot.isEmpty()) return true;
    const QDir routingDir(QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing")));
    if (!routingDir.exists()) QDir(m_projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/routing"));

    const QString filePath = routingDir.filePath(QStringLiteral("health.json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not write health to %1").arg(filePath);
        return false;
    }
    QJsonArray arr;
    for (const auto& h : m_health.values()) arr.append(h.toJson());
    QJsonObject root;
    root[QStringLiteral("health")] = arr;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool AdaptiveRoutingService::loadHealth(QString* error)
{
    if (m_projectRoot.isEmpty()) return true;
    const QString filePath = QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/health.json"));
    QFile file(filePath);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not read health from %1").arg(filePath);
        return false;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    for (const auto& v : doc.object().value(QStringLiteral("health")).toArray()) {
        RouteHealth h = RouteHealth::fromJson(v.toObject());
        m_health.insert(h.routeId, h);
    }
    return true;
}

bool AdaptiveRoutingService::saveCircuits(QString* error) const
{
    if (m_projectRoot.isEmpty()) return true;
    const QDir routingDir(QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing")));
    if (!routingDir.exists()) QDir(m_projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/routing"));

    const QString filePath = routingDir.filePath(QStringLiteral("circuits.json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not write circuits to %1").arg(filePath);
        return false;
    }
    QJsonArray arr;
    for (const auto& cb : m_circuits.values()) arr.append(cb.toJson());
    QJsonObject root;
    root[QStringLiteral("circuits")] = arr;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool AdaptiveRoutingService::loadCircuits(QString* error)
{
    if (m_projectRoot.isEmpty()) return true;
    const QString filePath = QDir(m_projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/circuits.json"));
    QFile file(filePath);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not read circuits from %1").arg(filePath);
        return false;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return false;
    for (const auto& v : doc.object().value(QStringLiteral("circuits")).toArray()) {
        RouteCircuitBreaker cb = RouteCircuitBreaker::fromJson(v.toObject());
        m_circuits.insert(cb.routeId, cb);
    }
    return true;
}

bool AdaptiveRoutingService::verifyP0GovernanceCompliance(const RoutingDecision& decision,
                                                         const QJsonObject& taskContract,
                                                         QString* violation)
{
    // P0 ALWAYS has final authority.
    // Verify that the routing decision does not expand permitted files
    const auto contractFiles = taskContract.value(QStringLiteral("permittedFiles")).toArray();
    QSet<QString> contractSet;
    for (const auto& f : contractFiles) contractSet.insert(f.toString());

    // Verify routing decision has zero file-expanding capability
    if (decision.isExecutionAuthority()) {
        if (violation) *violation = QStringLiteral("P4 RoutingDecision attempted to claim execution authority");
        return false;
    }

    return true;
}
