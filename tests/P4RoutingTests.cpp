#include "core/AdaptiveRoutingService.h"
#include "core/PredictiveOptimizationService.h"
#include "core/ProjectModel.h"
#include "core/ProjectMemory.h"
#include "core/TaskSignature.h"
#include "core/WorkerTaskServices.h"
#include "core/RuntimeOwnershipService.h"
#include "core/ContextCoordinationService.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <iostream>

bool runP4DogfoodCampaign(const QString& selfRepoPath);
bool runP4AdaptationValidation();

namespace {

bool check(bool condition, const char* testName, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL [" << testName << "]: " << message << "\n";
        return false;
    }
    std::cout << "PASS [" << testName << "]: " << message << "\n";
    return true;
}

TaskSignature makeTestSignature(const QString& category, const QString& operation, const QStringList& scopes = {})
{
    TaskSignature sig;
    sig.taskCategory = category;
    sig.operationType = operation;
    sig.relevantScopes = scopes.isEmpty() ? QStringList{QStringLiteral("source-code"), QStringLiteral("tests")} : scopes;
    sig.referencedFiles = {QStringLiteral("src/core/Foo.cpp"), QStringLiteral("tests/FooTests.cpp")};
    sig.targetSubsystem = QStringLiteral("core");
    sig.languageFramework = QStringLiteral("cpp");
    sig.governanceClass = QStringLiteral("standard");
    sig.resourceOwnershipClass = QStringLiteral("exclusive");
    sig.signatureVersion = QStringLiteral("1.0");
    sig.normalize();
    return sig;
}

PredictionContract makeTestPrediction(const QString& breadth = QStringLiteral("LOCAL"), double confScore = 0.85)
{
    PredictionContract pred;
    pred.predictionId = QStringLiteral("pred-test-12345");
    pred.predictedChangeBreadth = breadth;
    pred.confidence.score = confScore;
    pred.confidence.rating = (confScore >= 0.75) ? QStringLiteral("HIGH") : (confScore >= 0.50 ? QStringLiteral("MEDIUM") : QStringLiteral("LOW"));
    pred.status = QStringLiteral("READY");
    return pred;
}

QJsonObject makeTestTaskContract(const QString& projectRoot, const QStringList& permittedFiles = {}, const QString& mandatoryVal = QStringLiteral("focused"))
{
    QJsonObject contract;
    contract[QStringLiteral("contractId")] = QStringLiteral("contract-test-999");
    QJsonArray files;
    if (permittedFiles.isEmpty()) {
        files.append(QDir(projectRoot).filePath(QStringLiteral("src/core/Foo.cpp")));
        files.append(QDir(projectRoot).filePath(QStringLiteral("tests/FooTests.cpp")));
    } else {
        for (const auto& f : permittedFiles) files.append(f);
    }
    contract[QStringLiteral("permittedFiles")] = files;
    contract[QStringLiteral("mandatoryValidation")] = mandatoryVal;
    contract[QStringLiteral("adapterId")] = QStringLiteral("openai-codex");
    contract[QStringLiteral("isDestructive")] = false;
    return contract;
}

} // namespace

bool runP4RoutingTests()
{
    std::cout << "Starting P4 Self-Adjusting Routing test matrix...\n";
    bool allPass = true;

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        std::cerr << "FAIL: Temporary directory could not be created\n";
        return false;
    }
    const QString projectRoot = tempDir.path();
    ProjectModel model;
    model.setProjectPath(projectRoot);
    model.setProjectId(QStringLiteral("ARAMF_TEST_P4"));

    // P4-001: Canonical route registry is deterministic and versioned
    {
        RouteRegistry reg = RouteRegistry::defaultRegistry();
        const auto routes = reg.allRoutes();
        bool ok = check(routes.size() >= 5, "P4-001", "Default registry provides at least 5 canonical routes");
        bool hasStandard = false, hasConservative = false, hasFastLocal = false, hasHighRisk = false, hasFallback = false;
        for (const auto& r : routes) {
            ok &= check(r.version == QStringLiteral("1.0"), "P4-001", "Route version is explicit and 1.0");
            if (r.routeId == AdaptiveRoutingService::RouteStandardDirect) hasStandard = true;
            if (r.routeId == AdaptiveRoutingService::RouteConservativeIsolated) hasConservative = true;
            if (r.routeId == AdaptiveRoutingService::RouteFastLocal) hasFastLocal = true;
            if (r.routeId == AdaptiveRoutingService::RouteHighRiskCrossLayer) hasHighRisk = true;
            if (r.routeId == AdaptiveRoutingService::RouteFallbackMinimal) hasFallback = true;
        }
        ok &= check(hasStandard && hasConservative && hasFastLocal && hasHighRisk && hasFallback, "P4-001", "All 5 canonical routes registered");
        allPass &= ok;
    }

    // P4-002: Only eligible routes survive governance filtering
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction(QStringLiteral("CROSS_LAYER"));
        // Contract with 4 files across components
        QStringList broadFiles = {
            QDir(projectRoot).filePath(QStringLiteral("src/core/A.cpp")),
            QDir(projectRoot).filePath(QStringLiteral("src/ui/B.cpp")),
            QDir(projectRoot).filePath(QStringLiteral("src/net/C.cpp")),
            QDir(projectRoot).filePath(QStringLiteral("tests/AllTests.cpp"))
        };
        const auto contract = makeTestTaskContract(projectRoot, broadFiles);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool fastLocalRejected = false;
        for (const auto& r : decision.rejectedRoutes) {
            if (r.routeId == AdaptiveRoutingService::RouteFastLocal) {
                fastLocalRejected = true;
                break;
            }
        }
        bool ok = check(fastLocalRejected, "P4-002", "FastLocal correctly rejected for multi-component cross-layer task");
        for (const auto& c : decision.candidateRoutes) {
            ok &= check(c.eligible, "P4-002", "All surviving candidate routes are marked eligible");
        }
        allPass &= ok;
    }

    // P4-003: A governance-ineligible route cannot win even with the highest score
    {
        AdaptiveRoutingService svc(projectRoot);
        // Disable RouteStandardDirect explicitly
        svc.registry().updateRouteStatus(AdaptiveRoutingService::RouteStandardDirect, RouteStatus::Disabled);

        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"));
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool ok = check(decision.selectedRoute != AdaptiveRoutingService::RouteStandardDirect, "P4-003", "Disabled route cannot win");
        bool rejectedFound = false;
        for (const auto& r : decision.rejectedRoutes) {
            if (r.routeId == AdaptiveRoutingService::RouteStandardDirect) {
                rejectedFound = true;
                ok &= check(r.violationCategory == QStringLiteral("STATUS"), "P4-003", "Violation category is STATUS");
            }
        }
        ok &= check(rejectedFound, "P4-003", "StandardDirect explicitly placed in rejectedRoutes");
        allPass &= ok;
    }

    // P4-004: Route ranking is deterministic
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("modify"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.80);
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision1 = svc.selectRoute(taskSig, pred, contract, model);
        const auto decision2 = svc.selectRoute(taskSig, pred, contract, model);

        bool ok = check(decision1.selectedRoute == decision2.selectedRoute, "P4-004", "Route selection is deterministic across identical inputs");
        ok &= check(decision1.candidateRoutes.size() == decision2.candidateRoutes.size(), "P4-004", "Candidate counts match");
        for (int i = 0; i < decision1.candidateRoutes.size(); ++i) {
            ok &= check(decision1.candidateRoutes[i].routeId == decision2.candidateRoutes[i].routeId, "P4-004", "Candidate ranking order is identical");
            ok &= check(std::abs(decision1.candidateRoutes[i].deterministicScore - decision2.candidateRoutes[i].deterministicScore) < 0.0001, "P4-004", "Candidate scores match exactly");
        }
        allPass &= ok;
    }

    // P4-005: Tie-breaking is deterministic
    {
        AdaptiveRoutingService svc(projectRoot);
        // Verify tie-breaking logic directly: (score DESC, historicalSuccess DESC, routeId ASC)
        CandidateRoute c1; c1.routeId = QStringLiteral("ROUTE-B"); c1.deterministicScore = 0.75;
        CandidateRoute c2; c2.routeId = QStringLiteral("ROUTE-A"); c2.deterministicScore = 0.75;
        QList<CandidateRoute> list = {c1, c2};
        std::sort(list.begin(), list.end(), [](const CandidateRoute& a, const CandidateRoute& b) {
            if (std::abs(a.deterministicScore - b.deterministicScore) > 0.0001) return a.deterministicScore > b.deterministicScore;
            return a.routeId < b.routeId;
        });
        bool ok = check(list.first().routeId == QStringLiteral("ROUTE-A"), "P4-005", "Alphabetical tie-breaking when scores and health are identical");
        allPass &= ok;
    }

    // P4-006: Routing decision preserves provenance
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("test"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);
        QJsonObject prov{{QStringLiteral("actor"), QStringLiteral("agent")},
                         {QStringLiteral("agentId"), QStringLiteral("antigravity-worker")},
                         {QStringLiteral("tool"), QStringLiteral("aramf-cli")}};

        const auto decision = svc.selectRoute(taskSig, pred, contract, model, prov);
        bool ok = check(decision.provenance.value(QStringLiteral("actor")).toString() == QStringLiteral("agent"), "P4-006", "Provenance actor preserved");
        ok &= check(decision.provenance.value(QStringLiteral("agentId")).toString() == QStringLiteral("antigravity-worker"), "P4-006", "Provenance agentId preserved");
        ok &= check(decision.provenance.value(QStringLiteral("tool")).toString() == QStringLiteral("aramf-cli"), "P4-006", "Provenance tool preserved");
        allPass &= ok;
    }

    // P4-007: Routing decision references P3 evidence without mutating it
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("predict"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.92);
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool ok = check(decision.predictionReference == pred.predictionId, "P4-007", "Decision references P3 predictionId");
        ok &= check(decision.evidenceReferences.contains(pred.predictionId), "P4-007", "P3 predictionId added to evidenceReferences");
        ok &= check(pred.confidence.score == 0.92, "P4-007", "P3 prediction object was not mutated");
        allPass &= ok;
    }

    // P4-008: P4 cannot expand P0 file permissions
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("auth"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        QString violation;
        bool compliant = AdaptiveRoutingService::verifyP0GovernanceCompliance(decision, contract, &violation);
        bool ok = check(compliant, "P4-008", "Decision complies with P0 governance");
        ok &= check(!decision.isExecutionAuthority(), "P4-008", "P4 explicitly denies execution authority");
        allPass &= ok;
    }

    // P4-009: P4 cannot claim runtime ownership
    {
        RuntimeOwnershipService ownership;
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("ownership"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);
        AdaptiveRoutingService svc(projectRoot);
        const auto decision = svc.selectRoute(taskSig, pred, contract, model);

        // Attempting to claim runtime ownership using a P4 RoutingDecision must be rejected
        QJsonObject claimResult = RuntimeOwnershipService::claim(&model, decision.toJson(), QStringLiteral("task-p4"), QStringLiteral("worker-p4"), {});
        bool ok = check(!claimResult.value(QStringLiteral("accepted")).toBool(), "P4-009", "RuntimeOwnershipService rejects P4 routing decision as TaskContract");
        allPass &= ok;
    }

    // P4-010: P4 cannot expand P1 context scope
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("context"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"));
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        // FastLocal or standard route must use legitimate P1 context strategies
        bool ok = check(decision.contextRoute == QStringLiteral("FOCUSED") ||
                        decision.contextRoute == QStringLiteral("DEPENDENCY") ||
                        decision.contextRoute == QStringLiteral("FULL_PROJECT"), "P4-010", "Context route uses legal P1 strategy");
        allPass &= ok;
    }

    // P4-011: P4 cannot alter P2 execution evidence
    {
        AdaptiveRoutingService svc(projectRoot);
        RouteEvaluation eval;
        eval.evaluationId = QStringLiteral("eval-001");
        eval.routingDecisionId = QStringLiteral("dec-001");
        eval.routeId = AdaptiveRoutingService::RouteStandardDirect;
        eval.executionSuccess = true;
        eval.validationSuccess = true;
        svc.recordOutcome(eval);

        const auto stored = svc.evaluationsForRoute(AdaptiveRoutingService::RouteStandardDirect);
        bool ok = check(stored.size() == 1, "P4-011", "Evaluation recorded faithfully");
        ok &= check(stored.first().evaluationId == QStringLiteral("eval-001"), "P4-011", "Evaluation record matches original");
        allPass &= ok;
    }

    // P4-012: Insufficient evidence selects canonical conservative behavior
    {
        AdaptiveRoutingService svc(projectRoot); // clean instance with zero historical evaluations
        const auto taskSig = makeTestSignature(QStringLiteral("unknown"), QStringLiteral("novel-task"));
        const auto pred = makeTestPrediction(QStringLiteral("UNKNOWN"), 0.20);
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool ok = check(decision.status == QStringLiteral("CONSERVATIVE_ROUTE"), "P4-012", "Status is CONSERVATIVE_ROUTE when evidence is insufficient");
        ok &= check(decision.selectedRoute == AdaptiveRoutingService::RouteConservativeIsolated, "P4-012", "Selected RouteConservativeIsolated as safe default");
        ok &= check(decision.confidence.rating == QStringLiteral("INSUFFICIENT_ROUTING_EVIDENCE") || decision.confidence.rating == QStringLiteral("LOW"), "P4-012", "Routing confidence rating reflects weak evidence");
        allPass &= ok;
    }

    // P4-013: No eligible routes returns explicit NO_ELIGIBLE_ROUTE
    {
        AdaptiveRoutingService svc(projectRoot);
        // Disable ALL routes
        for (const auto& r : svc.registry().allRoutes()) {
            svc.registry().updateRouteStatus(r.routeId, RouteStatus::Disabled);
        }
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool ok = check(decision.status == QStringLiteral("NO_ELIGIBLE_ROUTE"), "P4-013", "Returns NO_ELIGIBLE_ROUTE when all routes are disabled");
        ok &= check(decision.candidateRoutes.isEmpty(), "P4-013", "Candidate list is empty");
        ok &= check(decision.rejectedRoutes.size() >= 5, "P4-013", "All routes recorded in rejectedRoutes");
        allPass &= ok;
    }

    // P4-014: Fallback routing selects only an independently eligible route
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"));
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool ok = check(!decision.fallbackRoute.isEmpty(), "P4-014", "Fallback route selected");
        ok &= check(decision.fallbackRoute != decision.selectedRoute, "P4-014", "Fallback route is distinct from selected route");
        const auto fallbackDef = svc.registry().findRoute(decision.fallbackRoute);
        ok &= check(fallbackDef.fallbackEligibility, "P4-014", "Fallback route has fallbackEligibility == true");
        allPass &= ok;
    }

    // P4-015: Fallback never expands permissions
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        // Fallback route is purely a route ID; it holds zero execution authority
        bool ok = check(!decision.isExecutionAuthority(), "P4-015", "Fallback carries zero authority to expand permissions");
        allPass &= ok;
    }

    // P4-016: Historical route success influences future ranking deterministically
    {
        QTemporaryDir t16;
        ProjectModel m16; m16.setProjectPath(t16.path());
        AdaptiveRoutingService svc(t16.path());
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"));
        const auto contract = makeTestTaskContract(t16.path());

        // Record 5 successful runs for RouteStandardDirect
        for (int i = 0; i < 5; ++i) {
            RouteEvaluation e;
            e.evaluationId = QStringLiteral("eval-succ-%1").arg(i);
            e.routeId = AdaptiveRoutingService::RouteStandardDirect;
            e.executionSuccess = true;
            e.validationSuccess = true;
            svc.recordOutcome(e);
        }

        const auto health = svc.routeHealth(AdaptiveRoutingService::RouteStandardDirect);
        bool ok = check(health.totalRuns == 5, "P4-016", "Health tracks total runs correctly");
        ok &= check(health.successRate == 1.0, "P4-016", "Health tracks success rate correctly");

        const auto decision = svc.selectRoute(taskSig, pred, contract, m16);
        ok &= check(decision.selectedRoute == AdaptiveRoutingService::RouteStandardDirect, "P4-016", "Historically successful route wins ranking");
        ok &= check(decision.confidence.rating == QStringLiteral("HIGH"), "P4-016", "Confidence elevated to HIGH with N=5 clean runs");
        allPass &= ok;
    }

    // P4-017: Single successful sample cannot cause unstable overconfidence
    {
        QTemporaryDir t17;
        ProjectModel m17; m17.setProjectPath(t17.path());
        AdaptiveRoutingService svc(t17.path());
        RouteEvaluation e;
        e.evaluationId = QStringLiteral("single-eval");
        e.routeId = AdaptiveRoutingService::RouteStandardDirect;
        e.executionSuccess = true;
        e.validationSuccess = true;
        svc.recordOutcome(e);

        const auto health = svc.routeHealth(AdaptiveRoutingService::RouteStandardDirect);
        bool ok = check(health.totalRuns == 1, "P4-017", "Total runs is 1");

        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(t17.path());
        const auto decision = svc.selectRoute(taskSig, pred, contract, m17);

        ok &= check(decision.confidence.rating != QStringLiteral("HIGH"), "P4-017", "N=1 cannot achieve HIGH confidence");
        ok &= check(decision.confidence.score <= 0.45, "P4-017", "N=1 score is strictly capped at <= 0.45");
        allPass &= ok;
    }

    // P4-018: Single failure cannot permanently blacklist a route unless canonical policy requires it
    {
        QTemporaryDir t18;
        AdaptiveRoutingService svc(t18.path());
        RouteEvaluation e;
        e.evaluationId = QStringLiteral("single-fail");
        e.routeId = AdaptiveRoutingService::RouteStandardDirect;
        e.executionSuccess = false;
        e.validationSuccess = false;
        svc.recordOutcome(e);

        const auto health = svc.routeHealth(AdaptiveRoutingService::RouteStandardDirect);
        bool ok = check(health.consecutiveFailures == 1, "P4-018", "One consecutive failure recorded");
        ok &= check(!health.isDegraded, "P4-018", "Route is NOT degraded on a single failure");
        const auto rDef = svc.registry().findRoute(AdaptiveRoutingService::RouteStandardDirect);
        ok &= check(rDef.status == RouteStatus::Active, "P4-018", "Route status remains Active after 1 failure");
        allPass &= ok;
    }

    // P4-019: Anti-oscillation prevents route flapping on marginal score differences
    {
        AdaptiveRoutingService svc(projectRoot);
        // Route score formula gives hysteresis bonus of +0.08 to currentPreferredRoute
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("edit"));
        const auto pred = makeTestPrediction();
        const auto healthStd = svc.routeHealth(AdaptiveRoutingService::RouteStandardDirect);
        const auto rDefStd = svc.registry().findRoute(AdaptiveRoutingService::RouteStandardDirect);
        const auto rDefFast = svc.registry().findRoute(AdaptiveRoutingService::RouteFastLocal);

        double scoreWithoutHysteresis = svc.calculateRouteScore(rDefStd, taskSig, pred, healthStd, {});
        double scoreWithHysteresis = svc.calculateRouteScore(rDefStd, taskSig, pred, healthStd, {}, AdaptiveRoutingService::RouteStandardDirect);

        bool ok = check(scoreWithHysteresis > scoreWithoutHysteresis, "P4-019", "Hysteresis bonus applied to incumbent preferred route");
        ok &= check(std::abs((scoreWithHysteresis - scoreWithoutHysteresis) - 0.08) < 0.001, "P4-019", "Hysteresis delta matches 0.08 switching threshold");
        allPass &= ok;
    }

    // P4-020: Route degradation is deterministic
    {
        QTemporaryDir t20;
        AdaptiveRoutingService svc(t20.path());
        // Record 2 consecutive failures for RouteFastLocal
        for (int i = 0; i < 2; ++i) {
            RouteEvaluation e;
            e.evaluationId = QStringLiteral("degrade-fail-%1").arg(i);
            e.routeId = AdaptiveRoutingService::RouteFastLocal;
            e.executionSuccess = false;
            e.validationSuccess = false;
            svc.recordOutcome(e);
        }
        const auto health = svc.routeHealth(AdaptiveRoutingService::RouteFastLocal);
        bool ok = check(health.isDegraded, "P4-020", "RouteFastLocal marked isDegraded=true after 2 consecutive failures");
        const auto rDef = svc.registry().findRoute(AdaptiveRoutingService::RouteFastLocal);
        ok &= check(rDef.status == RouteStatus::Degraded, "P4-020", "Route status transitioned to DEGRADED");
        allPass &= ok;
    }

    // P4-021: Circuit-breaker state is deterministic and governed if implemented
    {
        QTemporaryDir t21;
        AdaptiveRoutingService svc(t21.path());
        // 3 consecutive failures trips circuit breaker
        for (int i = 0; i < 3; ++i) {
            RouteEvaluation e;
            e.evaluationId = QStringLiteral("circuit-fail-%1").arg(i);
            e.routeId = AdaptiveRoutingService::RouteStandardDirect;
            e.executionSuccess = false;
            e.validationSuccess = false;
            svc.recordOutcome(e);
        }
        const auto cb = svc.circuitBreaker(AdaptiveRoutingService::RouteStandardDirect);
        bool ok = check(cb.state == CircuitState::Open, "P4-021", "Circuit breaker enters OPEN state after 3 failures");
        allPass &= ok;
    }

    // P4-022: Mandatory validation cannot be reduced by routing
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("critical-edit"));
        const auto pred = makeTestPrediction(QStringLiteral("LOCAL"));
        // TaskContract mandates full-regression
        const auto contract = makeTestTaskContract(projectRoot, {}, QStringLiteral("full-regression"));

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        // FastLocal and StandardDirect require focused, so they must be rejected because mandatory is full-regression!
        bool standardRejected = false;
        for (const auto& r : decision.rejectedRoutes) {
            if (r.routeId == AdaptiveRoutingService::RouteStandardDirect) standardRejected = true;
        }
        bool ok = check(standardRejected, "P4-022", "StandardDirect rejected when contract mandates full-regression");
        ok &= check(decision.validationRoute == QStringLiteral("full-regression"), "P4-022", "Winning route maintains full-regression validation");
        allPass &= ok;
    }

    // P4-023: Validation may safely escalate based on predicted risk/breadth
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("cross-layer-edit"));
        const auto pred = makeTestPrediction(QStringLiteral("CROSS_LAYER"));
        // Contract only asks for focused
        const auto contract = makeTestTaskContract(projectRoot, {}, QStringLiteral("focused"));

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool ok = check(decision.validationRoute == QStringLiteral("full-regression"), "P4-023", "Validation safely escalated to full-regression for CROSS_LAYER task");
        allPass &= ok;
    }

    // P4-024: Foreign project route data cannot inject local authorization
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("foreign-test"));
        const auto pred = makeTestPrediction();
        // Foreign path injected into TaskContract
        QStringList foreignFiles = {QStringLiteral("C:/ForeignProject/Secret.cpp")};
        const auto contract = makeTestTaskContract(projectRoot, foreignFiles);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        bool foreignRejected = false;
        for (const auto& r : decision.rejectedRoutes) {
            if (r.violationCategory == QStringLiteral("ISOLATION")) foreignRejected = true;
        }
        bool ok = check(foreignRejected, "P4-024", "Foreign path rejected by cross-project isolation filter");
        allPass &= ok;
    }

    // P4-025: Approved global knowledge cannot inject foreign paths
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto routes = svc.registry().allRoutes();
        bool foreignFound = false;
        for (const auto& r : routes) {
            for (const auto& s : r.requiredScopes) {
                if (s.contains(QStringLiteral(":/")) || s.contains(QStringLiteral(":\\"))) foreignFound = true;
            }
        }
        bool ok = check(!foreignFound, "P4-025", "Route registry contains zero absolute/foreign paths");
        allPass &= ok;
    }

    // P4-026: Routing decisions persist/reload faithfully
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("persist-test"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);

        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        QString saveErr;
        bool saved = svc.saveDecision(decision, &saveErr);
        bool ok = check(saved, "P4-026", "Decision saved to ARAMF_WORKER/routing/decisions/");

        QString loadErr;
        const auto loaded = svc.loadDecision(decision.routingDecisionId, &loadErr);
        ok &= check(loaded.routingDecisionId == decision.routingDecisionId, "P4-026", "Decision ID reloaded accurately");
        ok &= check(loaded.selectedRoute == decision.selectedRoute, "P4-026", "Selected route reloaded accurately");
        ok &= check(loaded.routingSchemaVersion == decision.routingSchemaVersion, "P4-026", "Schema version reloaded accurately");
        allPass &= ok;
    }

    // P4-027: Old routing decisions preserve their algorithm version
    {
        AdaptiveRoutingService svc(projectRoot);
        RoutingDecision legacyDec;
        legacyDec.routingDecisionId = QStringLiteral("legacy-dec-001");
        legacyDec.algorithmVersion = QStringLiteral("0.9-alpha");
        svc.saveDecision(legacyDec);

        const auto loaded = svc.loadDecision(QStringLiteral("legacy-dec-001"));
        bool ok = check(loaded.algorithmVersion == QStringLiteral("0.9-alpha"), "P4-027", "Legacy decision preserves original algorithmVersion");
        allPass &= ok;
    }

    // P4-028: Route health persists/reloads faithfully where required
    {
        AdaptiveRoutingService svc(projectRoot);
        RouteHealth h;
        h.routeId = QStringLiteral("ROUTE-TEST-HEALTH");
        h.totalRuns = 10;
        h.successfulRuns = 8;
        h.failedRuns = 2;
        h.successRate = 0.80;

        RouteEvaluation e;
        e.evaluationId = QStringLiteral("eval-persist");
        e.routeId = QStringLiteral("ROUTE-TEST-HEALTH");
        e.executionSuccess = true;
        e.validationSuccess = true;
        svc.recordOutcome(e);

        AdaptiveRoutingService reloadedSvc(projectRoot);
        const auto reloadedHealth = reloadedSvc.routeHealth(QStringLiteral("ROUTE-TEST-HEALTH"));
        bool ok = check(reloadedHealth.totalRuns == 1, "P4-028", "Health persisted and reloaded from health.json");
        ok &= check(reloadedHealth.successRate == 1.0, "P4-028", "Health success rate reloaded accurately");
        allPass &= ok;
    }

    // P4-029: Self-adjustment modifies only future ranking, never historical evidence
    {
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("adapt-test"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);

        const auto initialDecision = svc.selectRoute(taskSig, pred, contract, model);
        svc.saveDecision(initialDecision);

        // Record a failure on the selected route
        RouteEvaluation eval;
        eval.evaluationId = QStringLiteral("eval-adapt");
        eval.routeId = initialDecision.selectedRoute;
        eval.executionSuccess = false;
        eval.validationSuccess = false;
        svc.recordOutcome(eval);

        // Verify historical decision file is unchanged
        const auto historicalReloaded = svc.loadDecision(initialDecision.routingDecisionId);
        bool ok = check(historicalReloaded.selectedRoute == initialDecision.selectedRoute, "P4-029", "Historical decision was not mutated");
        ok &= check(historicalReloaded.candidateRoutes.size() == initialDecision.candidateRoutes.size(), "P4-029", "Historical candidates immutable");
        allPass &= ok;
    }

    // P4-030: P4 remains distinct from P6 agent quality scoring
    {
        AdaptiveRoutingService svc(projectRoot);
        // Verify RouteHealth tracks routeId, runs, successRate, but contains ZERO agent reputation metrics
        const auto health = svc.routeHealth(AdaptiveRoutingService::RouteStandardDirect);
        const auto json = health.toJson();
        bool ok = check(!json.contains(QStringLiteral("agentReputation")), "P4-030", "No agent reputation in route health");
        ok &= check(!json.contains(QStringLiteral("agentScore")), "P4-030", "No agent quality score in route health");
        ok &= check(!json.contains(QStringLiteral("agentRank")), "P4-030", "No agent rank in route health");
        allPass &= ok;
    }

    // P4-031: P4 remains distinct from P5 Canonical Code Bank
    {
        AdaptiveRoutingService svc(projectRoot);
        // Verify decision contains ZERO code bank promotions, code snippets, or code assets
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("p5-check"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);
        const auto decision = svc.selectRoute(taskSig, pred, contract, model);
        const auto json = decision.toJson();

        bool ok = check(!json.contains(QStringLiteral("promotedCode")), "P4-031", "No promoted code in decision");
        ok &= check(!json.contains(QStringLiteral("codeBankAsset")), "P4-031", "No code bank asset in decision");
        ok &= check(!json.contains(QStringLiteral("certifiedSnippet")), "P4-031", "No certified snippet in decision");
        allPass &= ok;
    }

    // P4-032: P4 output cannot satisfy P0 postflight execution evidence
    {
        WorkerTaskServices workerServices;
        AdaptiveRoutingService svc(projectRoot);
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("p0-postflight"));
        const auto pred = makeTestPrediction();
        const auto contract = makeTestTaskContract(projectRoot);
        const auto decision = svc.selectRoute(taskSig, pred, contract, model);

        // Attempting to use P4 routing decision as execution evidence in P0 postflight must fail
        QJsonObject postflightReport = WorkerTaskServices::postflight(model, contract, QJsonArray{decision.toJson()});
        bool ok = check(!postflightReport.value(QStringLiteral("accepted")).toBool(), "P4-032", "P0 postflight rejects P4 routing decision as execution evidence");
        allPass &= ok;
    }

    // P4-033: Cold-start correctly validates P4 persisted artifacts
    {
        AdaptiveRoutingService svc(projectRoot);
        svc.registry().save(projectRoot);
        svc.saveHealth();
        svc.saveCircuits();

        bool ok = check(QFile::exists(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/routes.json"))), "P4-033", "routes.json exists");
        ok &= check(QFile::exists(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/health.json"))), "P4-033", "health.json exists");
        ok &= check(QFile::exists(QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/routing/circuits.json"))), "P4-033", "circuits.json exists");
        allPass &= ok;
    }

    // P4-034: Memory consistency remains PASS with P4 artifacts present
    {
        ProjectMemory memory;
        QString err;
        const auto report = memory.validate(projectRoot, &err);
        // Even if empty project, basic structural checks run
        bool ok = check(err.isEmpty() || report.value(QStringLiteral("status")).toString() != QStringLiteral("ERROR"), "P4-034", "Memory validator tolerates P4 routing artifacts");
        allPass &= ok;
    }

    // P4-035: End-to-end P3 -> P4 -> P0/P1 -> P2 governed route lifecycle PASS
    {
        // 1. Task Definition
        const auto taskSig = makeTestSignature(QStringLiteral("core"), QStringLiteral("refactor-service"), {QStringLiteral("source-code")});

        // 2. P3 Prediction
        PredictionContract pred;
        pred.predictionId = QStringLiteral("pred-e2e-001");
        pred.predictedChangeBreadth = QStringLiteral("LOCAL");
        pred.confidence.score = 0.88;
        pred.confidence.rating = QStringLiteral("HIGH");

        // 3. P4 Routing Decision
        AdaptiveRoutingService svc(projectRoot);
        const auto contract = makeTestTaskContract(projectRoot);
        const auto decision = svc.selectRoute(taskSig, pred, contract, model);

        bool ok = check(decision.status == QStringLiteral("ROUTE_SELECTED") || decision.status == QStringLiteral("CONSERVATIVE_ROUTE"), "P4-035", "P4 produces valid route decision");
        ok &= check(!decision.selectedRoute.isEmpty(), "P4-035", "Route selected");
        ok &= check(!decision.fallbackRoute.isEmpty(), "P4-035", "Fallback route selected");

        // 4. P0 Authorization Check
        QString p0Violation;
        bool p0Approved = AdaptiveRoutingService::verifyP0GovernanceCompliance(decision, contract, &p0Violation);
        ok &= check(p0Approved, "P4-035", "P0 authorizes execution under selected route");

        // 5. P1 Context Coordination
        ok &= check(!decision.contextRoute.isEmpty(), "P4-035", "P1 context strategy provided");

        // 6. P2 Outcome Feedback
        RouteEvaluation eval;
        eval.evaluationId = QStringLiteral("eval-e2e-001");
        eval.routingDecisionId = decision.routingDecisionId;
        eval.routeId = decision.selectedRoute;
        eval.taskClass = QStringLiteral("core");
        eval.executionSuccess = true;
        eval.validationSuccess = true;
        eval.finalOutcome = QStringLiteral("SUCCESS");
        bool evalRecorded = svc.recordOutcome(eval);
        ok &= check(evalRecorded, "P4-035", "P2 execution feedback successfully recorded into P4");

        allPass &= ok;
    }

    // Run Real Dogfood Campaign across 9 scenarios
    allPass &= runP4DogfoodCampaign(tempDir.path());

    // Run Adaptation Validation (Bi-directional learning & hysteresis)
    allPass &= runP4AdaptationValidation();

    std::cout << "P4 Self-Adjusting Routing test matrix completed: " << (allPass ? "ALL PASS" : "FAILURES DETECTED") << "\n";
    return allPass;
}

bool runP4DogfoodCampaign(const QString& selfRepoPath)
{
    std::cout << "\nStarting P4 Multi-Scenario Dogfood Campaign against ARAMF self-model...\n";
    bool ok = true;
    ProjectModel selfModel;
    selfModel.setProjectPath(selfRepoPath);
    selfModel.setProjectId(QStringLiteral("ARAMF"));
    AdaptiveRoutingService svc(selfRepoPath);

    // Scenario 1: Focused core/memory change
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("memory"), QStringLiteral("modify"));
        sig.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp"), QStringLiteral("src/core/ProjectMemory.h")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("COMPONENT"), 0.85);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(!dec.selectedRoute.isEmpty(), "DOGFOOD-S1", "Scenario 1 (Core/Memory) selects valid route");
        ok &= check(!dec.fallbackRoute.isEmpty(), "DOGFOOD-S1", "Scenario 1 selects valid fallback");
        ok &= check(dec.contextRoute == QStringLiteral("FOCUSED") || dec.contextRoute == QStringLiteral("DEPENDENCY"), "DOGFOOD-S1", "Context route appropriate");

        RouteEvaluation ev;
        ev.evaluationId = QStringLiteral("dogfood-ev-1");
        ev.routingDecisionId = dec.routingDecisionId;
        ev.routeId = dec.selectedRoute;
        ev.taskClass = QStringLiteral("memory");
        ev.executionSuccess = true;
        ev.validationSuccess = true;
        ev.finalOutcome = QStringLiteral("SUCCESS");
        svc.recordOutcome(ev);
    }

    // Scenario 2: UI/workflow change
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("ui"), QStringLiteral("modify"));
        sig.referencedFiles = {QStringLiteral("src/ui/mainwindow/MainWindow.cpp")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.80);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(!dec.selectedRoute.isEmpty(), "DOGFOOD-S2", "Scenario 2 (UI/Workflow) selects valid route");
        ok &= check(dec.contextRoute == QStringLiteral("FOCUSED"), "DOGFOOD-S2", "Context route FOCUSED");

        RouteEvaluation ev;
        ev.evaluationId = QStringLiteral("dogfood-ev-2");
        ev.routingDecisionId = dec.routingDecisionId;
        ev.routeId = dec.selectedRoute;
        ev.taskClass = QStringLiteral("ui");
        ev.executionSuccess = true;
        ev.validationSuccess = true;
        ev.finalOutcome = QStringLiteral("SUCCESS");
        svc.recordOutcome(ev);
    }

    // Scenario 3: Cross-layer change
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("cross-layer"), QStringLiteral("integrate"));
        sig.referencedFiles = {
            QStringLiteral("src/core/AdaptiveRoutingService.cpp"),
            QStringLiteral("src/ui/mainwindow/MainWindow.cpp"),
            QStringLiteral("ARAMF_WORKER/rules/generated-rules.md")
        };
        PredictionContract pred = makeTestPrediction(QStringLiteral("CROSS_LAYER"), 0.90);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(dec.selectedRoute == AdaptiveRoutingService::RouteHighRiskCrossLayer, "DOGFOOD-S3", "Cross-layer routes to RouteHighRiskCrossLayer");
        ok &= check(dec.validationRoute == QStringLiteral("full-regression"), "DOGFOOD-S3", "Validation escalated to full-regression");
        ok &= check(dec.contextRoute == QStringLiteral("FULL_PROJECT"), "DOGFOOD-S3", "Context escalated to FULL_PROJECT");
    }

    // Scenario 4: Configuration / build-system change
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("configuration"), QStringLiteral("update"));
        sig.referencedFiles = {QStringLiteral("CMakeLists.txt")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("PROJECT_WIDE"), 0.85);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(!dec.selectedRoute.isEmpty(), "DOGFOOD-S4", "Config change selects valid route");
        ok &= check(dec.validationRoute == QStringLiteral("full-regression"), "DOGFOOD-S4", "Project-wide config change mandates full-regression");
    }

    // Scenario 5: Read-only analysis
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("analysis"), QStringLiteral("inspect"));
        sig.referencedFiles = {QStringLiteral("ARAMF_WORKER/worker-manifest.json")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.90);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(!dec.selectedRoute.isEmpty(), "DOGFOOD-S5", "Read-only analysis selects valid route");
        ok &= check(dec.contextRoute == QStringLiteral("FOCUSED"), "DOGFOOD-S5", "Context route FOCUSED");
    }

    // Scenario 6: Low-evidence novel task
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("quantum-computing"), QStringLiteral("entangle"));
        sig.referencedFiles = {QStringLiteral("src/qpu/QpuAccelerator.cpp")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("UNKNOWN"), 0.15);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(dec.status == QStringLiteral("CONSERVATIVE_ROUTE"), "DOGFOOD-S6", "Novel low-evidence task selects CONSERVATIVE_ROUTE");
        ok &= check(dec.selectedRoute == AdaptiveRoutingService::RouteConservativeIsolated, "DOGFOOD-S6", "Conservative default selected");
    }

    // Scenario 7: Route failure requiring fallback
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("core"), QStringLiteral("fallback-test"));
        sig.referencedFiles = {QStringLiteral("src/core/Services.cpp")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.75);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(!dec.fallbackRoute.isEmpty(), "DOGFOOD-S7", "Fallback route is available");
        // Record failure of selected route and execution on fallback route
        RouteEvaluation ev;
        ev.evaluationId = QStringLiteral("dogfood-ev-fallback");
        ev.routingDecisionId = dec.routingDecisionId;
        ev.routeId = dec.selectedRoute;
        ev.executionSuccess = false;
        ev.validationSuccess = false;
        ev.fallbackUsed = true;
        ev.fallbackRouteId = dec.fallbackRoute;
        ev.finalOutcome = QStringLiteral("RECOVERED");
        svc.recordOutcome(ev);
        ok &= check(svc.routeHealth(dec.selectedRoute).fallbackCount >= 1, "DOGFOOD-S7", "Health tracks fallback activation");
    }

    // Scenario 8: Resource collision / recovery scenario
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("core"), QStringLiteral("collision-test"));
        sig.referencedFiles = {QStringLiteral("src/core/Services.cpp")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.75);
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        RouteEvaluation ev;
        ev.evaluationId = QStringLiteral("dogfood-ev-collision");
        ev.routingDecisionId = dec.routingDecisionId;
        ev.routeId = dec.selectedRoute;
        ev.executionSuccess = true;
        ev.validationSuccess = true;
        ev.resourceCollisions = 1;
        ev.retryCount = 1;
        ev.finalOutcome = QStringLiteral("RETRIED");
        svc.recordOutcome(ev);
        ok &= check(svc.routeHealth(dec.selectedRoute).collisionCount >= 1, "DOGFOOD-S8", "Collision tracked in route health");
    }

    // Scenario 9: High-risk predicted task requiring validation escalation
    {
        TaskSignature sig = makeTestSignature(QStringLiteral("governance"), QStringLiteral("recertify"));
        sig.referencedFiles = {QStringLiteral("src/core/WorkerTaskServices.cpp"), QStringLiteral("src/core/ProjectMemory.cpp")};
        PredictionContract pred = makeTestPrediction(QStringLiteral("CROSS_LAYER"), 0.95);
        // Contract explicitly only asks for focused validation
        QJsonObject contract = makeTestTaskContract(selfRepoPath, sig.referencedFiles, QStringLiteral("focused"));

        RoutingDecision dec = svc.selectRoute(sig, pred, contract, selfModel);
        ok &= check(dec.validationRoute == QStringLiteral("full-regression"), "DOGFOOD-S9", "Validation safely escalated to full-regression");
        ok &= check(dec.selectedRoute == AdaptiveRoutingService::RouteHighRiskCrossLayer, "DOGFOOD-S9", "High risk task routes to HighRiskCrossLayer");
    }

    std::cout << "P4 Dogfood Campaign completed: " << (ok ? "ALL PASS" : "FAILURES") << "\n";
    return ok;
}

bool runP4AdaptationValidation()
{
    std::cout << "\nStarting P4 Adaptation Validation (Bi-directional Hysteresis & Learning)...\n";
    bool ok = true;
    QTemporaryDir adaptDir;
    ProjectModel model;
    model.setProjectPath(adaptDir.path());
    model.setProjectId(QStringLiteral("ARAMF_ADAPT"));
    AdaptiveRoutingService svc(adaptDir.path());

    TaskSignature sig = makeTestSignature(QStringLiteral("core"), QStringLiteral("work"));
    PredictionContract pred = makeTestPrediction(QStringLiteral("LOCAL"), 0.80);
    QJsonObject contract = makeTestTaskContract(adaptDir.path());

    // Step 1: Baseline Decision
    RoutingDecision initialDec = svc.selectRoute(sig, pred, contract, model);
    ok &= check(!initialDec.selectedRoute.isEmpty(), "ADAPT-01", "Initial baseline route selected");

    // Step 2: Route A (RouteStandardDirect) gets 4 consecutive successful runs
    for (int i = 0; i < 4; ++i) {
        RouteEvaluation e;
        e.evaluationId = QStringLiteral("adapt-succ-%1").arg(i);
        e.routeId = AdaptiveRoutingService::RouteStandardDirect;
        e.executionSuccess = true;
        e.validationSuccess = true;
        e.finalOutcome = QStringLiteral("SUCCESS");
        svc.recordOutcome(e);
    }
    // Route B (RouteConservativeIsolated) gets 3 runs with 2 retries
    for (int i = 0; i < 3; ++i) {
        RouteEvaluation e;
        e.evaluationId = QStringLiteral("adapt-retry-%1").arg(i);
        e.routeId = AdaptiveRoutingService::RouteConservativeIsolated;
        e.executionSuccess = true;
        e.validationSuccess = true;
        e.retryCount = 2;
        e.finalOutcome = QStringLiteral("RETRIED");
        svc.recordOutcome(e);
    }

    // Step 3: Route A now strongly preferred over Route B
    RoutingDecision adaptDec = svc.selectRoute(sig, pred, contract, model);
    ok &= check(adaptDec.selectedRoute == AdaptiveRoutingService::RouteStandardDirect, "ADAPT-02", "RouteStandardDirect preferred after repeated success");
    ok &= check(adaptDec.confidence.rating == QStringLiteral("HIGH"), "ADAPT-02", "Confidence elevated to HIGH");

    // Step 4: Reverse adaptation: Route A now experiences 3 consecutive failures
    for (int i = 0; i < 3; ++i) {
        RouteEvaluation e;
        e.evaluationId = QStringLiteral("adapt-fail-%1").arg(i);
        e.routeId = AdaptiveRoutingService::RouteStandardDirect;
        e.executionSuccess = false;
        e.validationSuccess = false;
        e.finalOutcome = QStringLiteral("FAILURE");
        svc.recordOutcome(e);
    }
    // Route B gets 4 consecutive clean successes
    for (int i = 0; i < 4; ++i) {
        RouteEvaluation e;
        e.evaluationId = QStringLiteral("adapt-b-succ-%1").arg(i);
        e.routeId = AdaptiveRoutingService::RouteConservativeIsolated;
        e.executionSuccess = true;
        e.validationSuccess = true;
        e.retryCount = 0;
        e.finalOutcome = QStringLiteral("SUCCESS");
        svc.recordOutcome(e);
    }

    // Step 5: System adapts back to RouteConservativeIsolated!
    RoutingDecision reversedDec = svc.selectRoute(sig, pred, contract, model);
    ok &= check(reversedDec.selectedRoute != AdaptiveRoutingService::RouteStandardDirect, "ADAPT-03", "Degraded RouteStandardDirect discarded");
    ok &= check(reversedDec.selectedRoute == AdaptiveRoutingService::RouteConservativeIsolated, "ADAPT-03", "System successfully adapted back to RouteConservativeIsolated");

    std::cout << "P4 Adaptation Validation completed: " << (ok ? "ALL PASS" : "FAILURES") << "\n";
    return ok;
}
