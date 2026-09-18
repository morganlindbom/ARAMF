#pragma once

#include "ProjectModel.h"
#include "PredictiveOptimizationService.h"
#include "TaskSignature.h"
#include "WorkerTaskServices.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// Route Lifecycle Status (P4.1.1)
enum class RouteStatus {
    Active,
    Degraded,
    Disabled,
    Superseded
};

QString routeStatusToString(RouteStatus status);
RouteStatus stringToRouteStatus(const QString& str);

// Canonical Route Definition (P4.1.1)
struct RouteDefinition final
{
    QString routeId;
    QString version = QStringLiteral("1.0");
    QString description;
    QStringList supportedTaskClasses;
    QStringList requiredScopes;
    QStringList requiredCapabilities;
    QStringList allowedTools;
    QStringList allowedAdapters;
    QString validationRequirement;       // "focused", "subsystem", "full-regression"
    QStringList governanceRestrictions;
    bool fallbackEligibility = true;
    RouteStatus status = RouteStatus::Active;

    QJsonObject toJson() const;
    static RouteDefinition fromJson(const QJsonObject& obj);
    bool isValid(QString* error = nullptr) const;
};

// Candidate Route evaluated for a task (P4.1.1)
struct CandidateRoute final
{
    QString routeId;
    bool eligible = false;
    QString rejectionReason;
    double deterministicScore = 0.0;
    int rank = 0;
    QStringList requiredCapabilities;
    bool governanceCompatibility = false;
    bool contextCompatibility = false;
    double predictedSuitability = 0.0;
    QJsonObject historicalOutcomeEvidence;

    QJsonObject toJson() const;
    static CandidateRoute fromJson(const QJsonObject& obj);
};

// Explicit Rejected Route record (P4.1.1)
struct RejectedRoute final
{
    QString routeId;
    QString rejectionReason;
    QString violationCategory;           // "GOVERNANCE", "CONTEXT", "CAPABILITY", "STATUS", "TOOL", "ISOLATION"

    QJsonObject toJson() const;
    static RejectedRoute fromJson(const QJsonObject& obj);
};

// Explainable Routing Confidence Model (P4.1.2)
struct RoutingConfidence final
{
    double score = 0.0;                  // [0.0, 1.0]
    QString rating;                      // "HIGH", "MEDIUM", "LOW", "INSUFFICIENT_ROUTING_EVIDENCE"
    int sampleSize = 0;                  // Historical executions
    double separationScore = 0.0;        // Difference between rank 1 and rank 2
    double consistencyScore = 0.0;       // Success rate of selected route
    double freshnessScore = 1.0;
    double conflictingEvidencePenalty = 0.0;
    double fallbackFrequency = 0.0;
    QString explanation;

    QJsonObject toJson() const;
    static RoutingConfidence fromJson(const QJsonObject& obj);
};

// Canonical, versioned RoutingDecision Contract (P4.1.1)
// P4 is NON-AUTHORITATIVE. Routing Decision != Governance Authorization.
struct RoutingDecision final
{
    QString routingDecisionId;
    QString taskId;
    QString taskSignatureFingerprint;
    QString predictionReference;
    QString routingSchemaVersion = QStringLiteral("1.0");
    QString algorithmVersion = QStringLiteral("1.0");
    QList<CandidateRoute> candidateRoutes;
    QList<RejectedRoute> rejectedRoutes;
    QString selectedRoute;
    QString fallbackRoute;
    QString routingReason;
    QStringList evidenceReferences;
    RoutingConfidence confidence;
    QJsonObject governanceConstraints;
    QString contextRoute;               // P1 context strategy: "FOCUSED", "FULL_PROJECT", "DEPENDENCY", "HISTORICAL"
    QString validationRoute;            // "focused", "subsystem", "full-regression"
    QString adapterRoute;               // "internal", "openai-codex", "gemini"
    QJsonObject provenance;
    QString sourceProjectId;
    QString sourceProjectPath;
    QString createdAt;
    QString status = QStringLiteral("ROUTE_SELECTED"); // "ROUTE_SELECTED", "CONSERVATIVE_ROUTE", "ROUTE_SELECTED_LOW_CONFIDENCE", "INSUFFICIENT_ROUTING_EVIDENCE", "NO_ELIGIBLE_ROUTE"

    QJsonObject toJson() const;
    static RoutingDecision fromJson(const QJsonObject& obj);

    // CRITICAL GOVERNANCE BOUNDARY: P4 holds zero execution authority
    bool isExecutionAuthority() const { return false; }
};

// Historical Route Execution Feedback & Evaluation (P4.1.3)
struct RouteEvaluation final
{
    QString evaluationId;
    QString routingDecisionId;
    QString taskId;
    QString routeId;
    QString taskClass;
    bool executionSuccess = false;
    bool validationSuccess = false;
    int retryCount = 0;
    int recoveryEvents = 0;
    int resourceCollisions = 0;
    bool fallbackUsed = false;
    QString fallbackRouteId;
    QString finalOutcome;               // "SUCCESS", "FAILURE", "RETRIED", "RECOVERED", "GOVERNANCE_BLOCKED"
    QString recordedAt;

    QJsonObject toJson() const;
    static RouteEvaluation fromJson(const QJsonObject& obj);
};

// Route Health & Degradation Statistics (P4.1.3, P4.1.4)
struct RouteHealth final
{
    QString routeId;
    int totalRuns = 0;
    int successfulRuns = 0;
    int failedRuns = 0;
    int consecutiveFailures = 0;
    int retryCount = 0;
    int collisionCount = 0;
    int fallbackCount = 0;
    double successRate = 1.0;
    bool isDegraded = false;
    QString lastDegradedAt;
    QString lastEvaluatedAt;

    QJsonObject toJson() const;
    static RouteHealth fromJson(const QJsonObject& obj);
};

// Deterministic Circuit Breaker State (P4.1.4)
enum class CircuitState {
    Closed,   // Normal operation
    Open,     // Tripped due to consecutive failures, route excluded
    HalfOpen  // Canary state for governed re-evaluation
};

QString circuitStateToString(CircuitState state);
CircuitState stringToCircuitState(const QString& str);

struct RouteCircuitBreaker final
{
    QString routeId;
    CircuitState state = CircuitState::Closed;
    int failureThreshold = 3;           // 3 consecutive failures trips circuit
    int consecutiveFailures = 0;
    int recoveryProbeSuccesses = 0;
    int requiredProbeSuccesses = 2;     // 2 consecutive canary successes to close
    int tripSequence = 0;
    QString lastStateChange;

    QJsonObject toJson() const;
    static RouteCircuitBreaker fromJson(const QJsonObject& obj);
};

// Canonical Route Registry (P4.1.1)
class RouteRegistry final
{
public:
    RouteRegistry();

    bool registerRoute(const RouteDefinition& route, QString* error = nullptr);
    bool updateRouteStatus(const QString& routeId, RouteStatus status, QString* error = nullptr);
    RouteDefinition findRoute(const QString& routeId) const;
    QList<RouteDefinition> allRoutes() const;
    QList<RouteDefinition> activeRoutes() const;
    QList<RouteDefinition> fallbackEligibleRoutes() const;

    bool save(const QString& projectRoot, QString* error = nullptr) const;
    bool load(const QString& projectRoot, QString* error = nullptr);

    static RouteRegistry defaultRegistry();

private:
    QHash<QString, RouteDefinition> m_routes;
};

// P4 Adaptive Routing Service (P4.1.1 - P4.1.4)
class AdaptiveRoutingService final
{
public:
    explicit AdaptiveRoutingService(const QString& projectRoot = {});

    // Canonical Route Registry access
    const RouteRegistry& registry() const { return m_registry; }
    RouteRegistry& registry() { return m_registry; }

    // Core P4 Routing decision pipeline (P4.1.1 - P4.1.4)
    RoutingDecision selectRoute(const TaskSignature& taskSignature,
                                const PredictionContract& prediction,
                                const QJsonObject& taskContract,
                                const ProjectModel& model,
                                const QJsonObject& provenance = {},
                                QString* error = nullptr);

    // Hard Eligibility Filtering (P4.1.1)
    bool evaluateEligibility(const RouteDefinition& route,
                             const TaskSignature& taskSignature,
                             const PredictionContract& prediction,
                             const QJsonObject& taskContract,
                             const ProjectModel& model,
                             QString* rejectionReason = nullptr,
                             QString* violationCategory = nullptr) const;

    // Deterministic Route Ranking (P4.1.2)
    double calculateRouteScore(const RouteDefinition& route,
                               const TaskSignature& taskSignature,
                               const PredictionContract& prediction,
                               const RouteHealth& health,
                               const QList<RouteEvaluation>& historicalEvaluations,
                               const QString& currentPreferredRoute = {}) const;

    // Explainable Routing Confidence (P4.1.2)
    RoutingConfidence evaluateRoutingConfidence(const QList<CandidateRoute>& eligibleRoutes,
                                               const PredictionConfidence& predictionConfidence,
                                               const RouteHealth& selectedHealth,
                                               const QList<RouteEvaluation>& routeEvaluations) const;

    // Governed Route Outcome Feedback (P4.1.3)
    bool recordOutcome(const RouteEvaluation& evaluation, QString* error = nullptr);
    QList<RouteEvaluation> evaluationsForRoute(const QString& routeId) const;
    QList<RouteEvaluation> allEvaluations() const;

    // Route Health & Circuit Breaker (P4.1.4)
    RouteHealth routeHealth(const QString& routeId) const;
    QList<RouteHealth> allRouteHealth() const;
    RouteCircuitBreaker circuitBreaker(const QString& routeId) const;
    void updateCircuitBreaker(const QString& routeId, bool executionSuccess, int currentSequence);

    // Persistence under ARAMF_WORKER/routing/
    bool saveDecision(const RoutingDecision& decision, QString* error = nullptr) const;
    RoutingDecision loadDecision(const QString& decisionId, QString* error = nullptr) const;
    QList<RoutingDecision> allDecisions(QString* error = nullptr) const;

    bool saveHealth(QString* error = nullptr) const;
    bool loadHealth(QString* error = nullptr);

    bool saveCircuits(QString* error = nullptr) const;
    bool loadCircuits(QString* error = nullptr);

    // Verification & Governance Check
    static bool verifyP0GovernanceCompliance(const RoutingDecision& decision,
                                             const QJsonObject& taskContract,
                                             QString* violation = nullptr);

    // Canonical built-in route IDs
    static const QString RouteStandardDirect;
    static const QString RouteConservativeIsolated;
    static const QString RouteFastLocal;
    static const QString RouteHighRiskCrossLayer;
    static const QString RouteFallbackMinimal;

private:
    QString m_projectRoot;
    RouteRegistry m_registry;
    QHash<QString, RouteHealth> m_health;
    QHash<QString, RouteCircuitBreaker> m_circuits;
    QList<RouteEvaluation> m_evaluations;

    void initializeDefaultRoutes();
    void loadAllPersistence();
};
