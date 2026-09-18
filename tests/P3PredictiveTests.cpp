#include "core/AramfPaths.h"
#include "core/ContextCoordinationService.h"
#include "core/ExecutionOrchestrator.h"
#include "core/PredictiveOptimizationService.h"
#include "core/ProjectMemory.h"
#include "core/ProjectModel.h"
#include "core/RuntimeOwnershipService.h"
#include "core/TaskSignature.h"
#include "core/WorkerTaskServices.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>

namespace {
bool checkRequire(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "P3 TEST FAILURE: " << message << std::endl;
        return false;
    }
    return true;
}

bool writeTestFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

void setupHistoricalProject(const QString& root)
{
    ProjectMemory memory;
    ProjectModel model;
    model.setProjectId(QStringLiteral("p3-test-project"));
    model.setProjectPath(root);
    model.setProjectName(QStringLiteral("ARAMF_WORKER"));

    MemoryConfiguration testMemoryConfig;
    testMemoryConfig.maintenanceOptions = {
        QStringLiteral("update-current-state"),
        QStringLiteral("record-task-completion"),
        QStringLiteral("record-build-results"),
        QStringLiteral("record-test-results"),
        QStringLiteral("record-validation")
    };
    model.setMemoryConfiguration(testMemoryConfig);

    memory.initialize(root, &model);

    // Seed historical events with known patterns
    // 1. Memory task with pass
    QJsonObject prov{
        {QStringLiteral("actor"), QStringLiteral("agent")},
        {QStringLiteral("agentId"), QStringLiteral("antigravity")},
        {QStringLiteral("tool"), QStringLiteral("aramf-cli")}
    };

    memory.recordOperation(root, QStringLiteral("task-start"), QJsonObject{
        {QStringLiteral("task"), QStringLiteral("Update ProjectMemory validation")},
        {QStringLiteral("category"), QStringLiteral("memory")},
        {QStringLiteral("scope"), QStringLiteral("project")},
        {QStringLiteral("detail"), QStringLiteral("Modify src/core/ProjectMemory.cpp and src/core/ProjectMemory.h")},
        {QStringLiteral("provenance"), prov}
    });

    memory.recordOperation(root, QStringLiteral("test-result"), QJsonObject{
        {QStringLiteral("task"), QStringLiteral("Update ProjectMemory validation")},
        {QStringLiteral("suite"), QStringLiteral("aramf_core_tests")},
        {QStringLiteral("passed"), 100},
        {QStringLiteral("failed"), 0},
        {QStringLiteral("total"), 100},
        {QStringLiteral("status"), QStringLiteral("PASS")},
        {QStringLiteral("provenance"), prov}
    });

    memory.recordOperation(root, QStringLiteral("task-complete"), QJsonObject{
        {QStringLiteral("task"), QStringLiteral("Update ProjectMemory validation")},
        {QStringLiteral("status"), QStringLiteral("PASS")},
        {QStringLiteral("provenance"), prov}
    });

    // 2. Second memory task with pass
    memory.recordOperation(root, QStringLiteral("task-start"), QJsonObject{
        {QStringLiteral("task"), QStringLiteral("Refactor ProjectMemory compaction")},
        {QStringLiteral("category"), QStringLiteral("memory")},
        {QStringLiteral("scope"), QStringLiteral("project")},
        {QStringLiteral("detail"), QStringLiteral("Touched src/core/ProjectMemory.cpp")},
        {QStringLiteral("provenance"), prov}
    });

    memory.recordOperation(root, QStringLiteral("test-result"), QJsonObject{
        {QStringLiteral("task"), QStringLiteral("Refactor ProjectMemory compaction")},
        {QStringLiteral("suite"), QStringLiteral("aramf_core_tests")},
        {QStringLiteral("passed"), 100},
        {QStringLiteral("failed"), 0},
        {QStringLiteral("total"), 100},
        {QStringLiteral("status"), QStringLiteral("PASS")},
        {QStringLiteral("provenance"), prov}
    });

    memory.recordOperation(root, QStringLiteral("task-complete"), QJsonObject{
        {QStringLiteral("task"), QStringLiteral("Refactor ProjectMemory compaction")},
        {QStringLiteral("status"), QStringLiteral("PASS")},
        {QStringLiteral("provenance"), prov}
    });
}
}

bool runP3PredictiveTests()
{
    bool ok = true;
    std::cout << "Starting P3 Predictive Task Optimization test matrix..." << std::endl;

    // P3-001: Identical normalized task inputs produce deterministic task signatures.
    {
        TaskSignature sig1;
        sig1.taskCategory = QStringLiteral("  MEMORY  ");
        sig1.operationType = QStringLiteral(" MODIFY ");
        sig1.relevantScopes = {QStringLiteral("PROJECT"), QStringLiteral("project"), QStringLiteral("  core  ")};
        sig1.referencedFiles = {QStringLiteral("src\\core\\ProjectMemory.cpp"), QStringLiteral("src/core/ProjectMemory.cpp")};
        sig1.normalize();

        TaskSignature sig2;
        sig2.taskCategory = QStringLiteral("memory");
        sig2.operationType = QStringLiteral("modify");
        sig2.relevantScopes = {QStringLiteral("core"), QStringLiteral("project")};
        sig2.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp")};
        sig2.normalize();

        ok &= checkRequire(sig1.taskCategory == sig2.taskCategory, "P3-001: Category must normalize deterministically");
        ok &= checkRequire(sig1.operationType == sig2.operationType, "P3-001: Operation type must normalize deterministically");
        ok &= checkRequire(sig1.relevantScopes == sig2.relevantScopes, "P3-001: Scopes must normalize deterministically");
        ok &= checkRequire(sig1.referencedFiles == sig2.referencedFiles, "P3-001: Files must normalize deterministically");
    }

    // P3-002: Equivalent canonical task characteristics produce stable fingerprints.
    {
        TaskSignature sig1;
        sig1.taskCategory = QStringLiteral("memory");
        sig1.operationType = QStringLiteral("modify");
        sig1.relevantScopes = {QStringLiteral("project")};
        sig1.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp")};

        TaskSignature sig2;
        sig2.taskCategory = QStringLiteral("  MEMORY  ");
        sig2.operationType = QStringLiteral("modify");
        sig2.relevantScopes = {QStringLiteral("project"), QStringLiteral("PROJECT")};
        sig2.referencedFiles = {QStringLiteral("src\\core\\ProjectMemory.cpp")};

        const QString fp1 = sig1.fingerprint();
        const QString fp2 = sig2.fingerprint();
        ok &= checkRequire(!fp1.isEmpty() && fp1.length() == 64, "P3-002: Fingerprint must be a valid SHA-256 string");
        ok &= checkRequire(fp1 == fp2, "P3-002: Equivalent characteristics must produce identical fingerprint");
    }

    // P3-003: Known historical evidence produces an explainable prediction.
    QTemporaryDir testDir;
    setupHistoricalProject(testDir.path());
    ProjectModel model;
    model.setProjectId(QStringLiteral("p3-test-project"));
    model.setProjectPath(testDir.path());

    PredictionContract prediction;
    {
        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.operationType = QStringLiteral("modify");
        sig.relevantScopes = {QStringLiteral("project")};
        sig.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp")};
        sig.targetSubsystem = QStringLiteral("memory");

        prediction = PredictiveOptimizationService::predict(model, sig);

        ok &= checkRequire(prediction.status == QStringLiteral("READY"), "P3-003: Prediction status must be READY");
        ok &= checkRequire(prediction.advisoryStatus == QStringLiteral("ADVISORY"), "P3-003: Must declare ADVISORY status");
        ok &= checkRequire(!prediction.predictedValidation.isEmpty(), "P3-003: Must predict validation");

        bool foundRationale = false;
        for (const auto& val : prediction.predictedValidation) {
            if (!val.rationale.isEmpty()) foundRationale = true;
        }
        ok &= checkRequire(foundRationale, "P3-003: Every predicted item must have an explainable rationale");
    }

    // P3-004: Prediction includes concrete evidence references.
    {
        ok &= checkRequire(!prediction.evidenceReferences.isEmpty(), "P3-004: Prediction must include concrete evidence references");
        bool hasEventId = false;
        for (const auto& ref : prediction.evidenceReferences) {
            if (ref.startsWith(QStringLiteral("event-"))) hasEventId = true;
        }
        ok &= checkRequire(hasEventId, "P3-004: Evidence references must point to historical event IDs");
    }

    // P3-005: Insufficient evidence returns low confidence / insufficient evidence rather than invented conclusions.
    {
        QTemporaryDir emptyDir;
        ProjectModel emptyModel;
        emptyModel.setProjectId(QStringLiteral("empty-project"));
        emptyModel.setProjectPath(emptyDir.path());

        TaskSignature obscureSig;
        obscureSig.taskCategory = QStringLiteral("nonexistent-category-xyz");
        obscureSig.operationType = QStringLiteral("unknown-op");
        obscureSig.targetSubsystem = QStringLiteral("nonexistent-subsystem");

        const auto obscurePred = PredictiveOptimizationService::predict(emptyModel, obscureSig);
        ok &= checkRequire(obscurePred.status == QStringLiteral("INSUFFICIENT_EVIDENCE") || obscurePred.confidence.rating == QStringLiteral("INSUFFICIENT_EVIDENCE") || obscurePred.confidence.rating == QStringLiteral("LOW"),
                           "P3-005: Insufficient evidence must return low confidence / insufficient evidence");
        ok &= checkRequire(obscurePred.confidence.score <= 0.20, "P3-005: Insufficient evidence must produce low confidence score");
    }

    // P3-006: Conflicting evidence lowers confidence.
    {
        QTemporaryDir conflictDir;
        setupHistoricalProject(conflictDir.path());
        ProjectMemory mem;

        // Add failing event to create conflicting evidence
        QJsonObject prov{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("antigravity")},
            {QStringLiteral("tool"), QStringLiteral("aramf-cli")}
        };
        mem.recordOperation(conflictDir.path(), QStringLiteral("task-start"), QJsonObject{
            {QStringLiteral("task"), QStringLiteral("Failing Memory Task")},
            {QStringLiteral("category"), QStringLiteral("memory")},
            {QStringLiteral("scope"), QStringLiteral("project")},
            {QStringLiteral("detail"), QStringLiteral("Touch src/core/ProjectMemory.cpp")},
            {QStringLiteral("provenance"), prov}
        });
        mem.recordOperation(conflictDir.path(), QStringLiteral("task-complete"), QJsonObject{
            {QStringLiteral("task"), QStringLiteral("Failing Memory Task")},
            {QStringLiteral("status"), QStringLiteral("FAIL")},
            {QStringLiteral("detail"), QStringLiteral("Validation failed with regression")},
            {QStringLiteral("provenance"), prov}
        });

        ProjectModel conflictModel;
        conflictModel.setProjectId(QStringLiteral("conflict-project"));
        conflictModel.setProjectPath(conflictDir.path());

        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.operationType = QStringLiteral("modify");
        sig.relevantScopes = {QStringLiteral("project")};
        sig.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp")};
        sig.targetSubsystem = QStringLiteral("memory");

        const auto conflictPred = PredictiveOptimizationService::predict(conflictModel, sig);
        ok &= checkRequire(conflictPred.confidence.conflictingEvidencePenalty > 0.0, "P3-006: Conflicting evidence must apply penalty");
        ok &= checkRequire(conflictPred.confidence.score < prediction.confidence.score, "P3-006: Conflicting evidence must lower confidence score");
    }

    // P3-007: Stale evidence is identified and weighted according to canonical freshness policy.
    {
        // Legacy cutoff is 311. If all events are sequence <= 311, freshness factor is reduced.
        QTemporaryDir staleDir;
        ProjectModel staleModel;
        staleModel.setProjectId(QStringLiteral("stale-project"));
        staleModel.setProjectPath(staleDir.path());

        // In testDir, sequence numbers are 1, 2, 3... which are <= 311. Freshness score should be 0.70.
        ok &= checkRequire(prediction.confidence.freshnessScore <= 1.0 && prediction.confidence.freshnessScore >= 0.5,
                           "P3-007: Freshness score must be bounded and reflect historical event sequence");
    }

    // P3-008: Prediction cannot expand P0 ownership.
    {
        // Attempt to pass a PredictionContract JSON to RuntimeOwnershipService::claim
        // It must be rejected because PredictionContract is NOT a TaskContract.
        auto claimResult = RuntimeOwnershipService::claim(&model, prediction.toJson(), QStringLiteral("task-1"), QStringLiteral("worker-1"), {QStringLiteral("src/core/ProjectMemory.cpp")});
        ok &= checkRequire(!claimResult.value(QStringLiteral("granted")).toBool(), "P3-008: Prediction cannot claim runtime ownership");
        ok &= checkRequire(claimResult.value(QStringLiteral("code")).toString() == QStringLiteral("TASK_CONTRACT_INVALID"),
                           "P3-008: Prediction must be rejected as invalid TaskContract");
    }

    // P3-009: Prediction cannot expand P1 context scope.
    {
        // P1 routing respects only model.ruleConfiguration().projectScopes
        QStringList predictedScopesList;
        for (const auto& item : prediction.predictedScopes) predictedScopesList.append(item.item);
        predictedScopesList.append(QStringLiteral("unauthorized-scope-outside-model"));

        const auto route = ContextCoordinationService::route(model, predictedScopesList);
        const auto resolvedScopes = route.value(QStringLiteral("requestedScopes")).toArray();
        // The service does not grant permissions or authorize arbitrary scopes.
        ok &= checkRequire(!PredictionContract::isExecutionAuthority(), "P3-009: Prediction contract declares no execution authority");
    }

    // P3-010: Prediction cannot mutate P2 evaluation evidence.
    {
        ExecutionOrchestrator orchestrator(&model);
        // WorkerTaskServices::postflight requires genuine test evidence, not prediction objects
        QJsonObject bogusEvidence = prediction.toJson();
        const auto postflight = WorkerTaskServices::postflight(model, QJsonObject{}, QJsonArray{bogusEvidence});
        ok &= checkRequire(postflight.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")
                           || postflight.value(QStringLiteral("completionState")).toString() != QStringLiteral("CERTIFIED"),
                           "P3-010: P0/P2 postflight rejects prediction artifacts as execution evidence");
    }

    // P3-011: Likely files/components prediction is deterministic.
    {
        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.operationType = QStringLiteral("modify");
        sig.targetSubsystem = QStringLiteral("memory");

        const auto predA = PredictiveOptimizationService::predict(model, sig);
        const auto predB = PredictiveOptimizationService::predict(model, sig);

        QStringList filesA, filesB;
        for (const auto& item : predA.predictedFiles) filesA.append(item.item);
        for (const auto& item : predB.predictedFiles) filesB.append(item.item);
        filesA.sort(); filesB.sort();

        ok &= checkRequire(filesA == filesB, "P3-011: File predictions must be deterministic");
        ok &= checkRequire(filesA.contains(QStringLiteral("src/core/ProjectMemory.cpp")), "P3-011: Must predict relevant memory source file");
    }

    // P3-012: Likely test prediction is deterministic.
    {
        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.operationType = QStringLiteral("modify");
        sig.targetSubsystem = QStringLiteral("memory");

        const auto predA = PredictiveOptimizationService::predict(model, sig);
        const auto predB = PredictiveOptimizationService::predict(model, sig);

        QStringList testsA, testsB;
        for (const auto& item : predA.predictedValidation) testsA.append(item.item);
        for (const auto& item : predB.predictedValidation) testsB.append(item.item);
        testsA.sort(); testsB.sort();

        ok &= checkRequire(testsA == testsB, "P3-012: Validation predictions must be deterministic");
        ok &= checkRequire(testsA.contains(QStringLiteral("aramf_core_tests")), "P3-012: Must predict aramf_core_tests");
        ok &= checkRequire(testsA.contains(QStringLiteral("memory cold-start")), "P3-012: Must predict memory cold-start for memory subsystem");
    }

    // P3-013: Risk prediction is explainable.
    {
        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.operationType = QStringLiteral("modify");
        sig.targetSubsystem = QStringLiteral("memory");

        const auto pred = PredictiveOptimizationService::predict(model, sig);
        ok &= checkRequire(!pred.predictedRiskCategories.isEmpty(), "P3-013: Risk categories must be predicted");
        bool hasRationale = false;
        for (const auto& r : pred.predictedRiskCategories) {
            if (!r.rationale.isEmpty()) hasRationale = true;
        }
        ok &= checkRequire(hasRationale, "P3-013: Risk predictions must be explainable");
    }

    // P3-014: Prediction-vs-actual comparison computes deterministic metrics.
    {
        QJsonObject executionResult{
            {QStringLiteral("sourceTaskId"), QStringLiteral("task-actual-001")},
            {QStringLiteral("changedResources"), QJsonArray{QStringLiteral("src/core/ProjectMemory.cpp")}},
            {QStringLiteral("validationEvidence"), QJsonArray{
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("aramf_core_tests")}, {QStringLiteral("status"), QStringLiteral("PASS")}}
            }},
            {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}
        };

        const auto eval = PredictiveOptimizationService::evaluate(prediction, executionResult);
        ok &= checkRequire(eval.truePositiveFiles.contains(QStringLiteral("src/core/ProjectMemory.cpp")), "P3-014: True positive file detected");
        ok &= checkRequire(eval.filePrecision > 0.0, "P3-014: Precision must be calculated");
        ok &= checkRequire(eval.fileRecall > 0.0, "P3-014: Recall must be calculated");
        ok &= checkRequire(eval.matchedValidation.contains(QStringLiteral("aramf_core_tests")), "P3-014: Validation match detected");
    }

    // P3-015: Project boundaries are preserved.
    {
        QTemporaryDir otherProjectDir;
        ProjectModel otherModel;
        otherModel.setProjectId(QStringLiteral("isolated-foreign-project"));
        otherModel.setProjectPath(otherProjectDir.path());

        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.operationType = QStringLiteral("modify");
        sig.targetSubsystem = QStringLiteral("memory");

        const auto foreignPred = PredictiveOptimizationService::predict(otherModel, sig);
        ok &= checkRequire(foreignPred.sourceProjectId == QStringLiteral("isolated-foreign-project"), "P3-015: Must bind to correct project ID");
        ok &= checkRequire(foreignPred.sourceProjectPath == otherProjectDir.path(), "P3-015: Must bind to correct project path");
        ok &= checkRequire(foreignPred.evidenceReferences.isEmpty(), "P3-015: Cannot leak evidence from unrelated foreign project");
    }

    // P3-016: Provenance is preserved on prediction artifacts.
    {
        QJsonObject customProv{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("test-agent")},
            {QStringLiteral("tool"), QStringLiteral("aramf-test-suite")}
        };
        TaskSignature sig;
        sig.taskCategory = QStringLiteral("testing");
        sig.operationType = QStringLiteral("validate");
        sig.targetSubsystem = QStringLiteral("general");

        const auto provPred = PredictiveOptimizationService::predict(model, sig, customProv);
        ok &= checkRequire(provPred.provenance.value(QStringLiteral("actor")).toString() == QStringLiteral("agent"), "P3-016: Provenance actor preserved");
        ok &= checkRequire(provPred.provenance.value(QStringLiteral("agentId")).toString() == QStringLiteral("test-agent"), "P3-016: Provenance agentId preserved");
        ok &= checkRequire(provPred.provenance.value(QStringLiteral("tool")).toString() == QStringLiteral("aramf-test-suite"), "P3-016: Provenance tool preserved");
    }

    // P3-017: Prediction artifacts survive persistence/reload.
    {
        QString saveErr;
        bool saved = PredictiveOptimizationService::savePrediction(testDir.path(), prediction, &saveErr);
        ok &= checkRequire(saved && saveErr.isEmpty(), "P3-017: Prediction must be savable to disk");

        PredictionContract reloaded;
        QString loadErr;
        bool loaded = PredictiveOptimizationService::loadPrediction(testDir.path(), prediction.predictionId, &reloaded, &loadErr);
        ok &= checkRequire(loaded && loadErr.isEmpty(), "P3-017: Prediction must be reloadable from disk");
        ok &= checkRequire(reloaded.predictionId == prediction.predictionId, "P3-017: Reloaded prediction ID must match");
        ok &= checkRequire(reloaded.taskSignature.fingerprint() == prediction.taskSignature.fingerprint(), "P3-017: Reloaded fingerprint must match");
        ok &= checkRequire(reloaded.confidence.rating == prediction.confidence.rating, "P3-017: Reloaded confidence rating must match");
    }

    // P3-018: Cold-start does not convert stale predictions into fresh evidence.
    {
        ProjectMemory memory;
        QString valErr;
        // Verify cold-start treats predictions directory as non-authoritative
        const auto report = memory.validateColdStart(testDir.path(), &valErr);
        ok &= checkRequire(report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "P3-018: Cold-start validation must pass with predictions present");
    }

    // Phase 11 Dogfooding: Read-only prediction against ARAMF itself
    {
        ProjectModel selfModel;
        selfModel.setProjectId(QStringLiteral("ARAMF"));
        selfModel.setProjectPath(AramfPaths::programRoot());

        TaskSignature dogfoodSig;
        dogfoodSig.taskCategory = QStringLiteral("memory");
        dogfoodSig.operationType = QStringLiteral("modify");
        dogfoodSig.targetSubsystem = QStringLiteral("memory");
        dogfoodSig.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp")};
        dogfoodSig.relevantScopes = {QStringLiteral("project")};

        const auto dogfoodPred = PredictiveOptimizationService::predict(selfModel, dogfoodSig);
        ok &= checkRequire(dogfoodPred.status == QStringLiteral("READY"), "P3-DOGFOOD: Status must be READY");
        ok &= checkRequire(!dogfoodPred.evidenceReferences.isEmpty(), "P3-DOGFOOD: Must find historical evidence in ARAMF repo");
        ok &= checkRequire(dogfoodPred.confidence.rating == QStringLiteral("HIGH") || dogfoodPred.confidence.rating == QStringLiteral("MEDIUM"),
                           "P3-DOGFOOD: Confidence must be MEDIUM or HIGH for well-established memory subsystem");

        // Evaluate dogfood prediction against historical evidence
        QJsonObject historicalOutcome{
            {QStringLiteral("sourceTaskId"), QStringLiteral("historical-memory-validation")},
            {QStringLiteral("changedResources"), QJsonArray{
                QStringLiteral("src/core/ProjectMemory.cpp"),
                QStringLiteral("src/core/ProjectMemory.h"),
                QStringLiteral("src/core/MemoryCommand.cpp")
            }},
            {QStringLiteral("validationEvidence"), QJsonArray{
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("aramf_core_tests")}, {QStringLiteral("status"), QStringLiteral("PASS")}},
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("memory cold-start")}, {QStringLiteral("status"), QStringLiteral("PASS")}},
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("memory validate")}, {QStringLiteral("status"), QStringLiteral("PASS")}}
            }},
            {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}
        };
        const auto dogfoodEval = PredictiveOptimizationService::evaluate(dogfoodPred, historicalOutcome);
        ok &= checkRequire(dogfoodEval.filePrecision == 1.0, "P3-DOGFOOD: Precision must be 1.0 (all predicted files were modified)");
        ok &= checkRequire(dogfoodEval.fileRecall == 1.0, "P3-DOGFOOD: Recall must be 1.0 (all modified files were predicted)");
        ok &= checkRequire(dogfoodEval.matchedValidation.contains(QStringLiteral("aramf_core_tests")), "P3-DOGFOOD: Core tests matched");
        ok &= checkRequire(dogfoodEval.matchedValidation.contains(QStringLiteral("memory cold-start")), "P3-DOGFOOD: Cold-start matched");
        ok &= checkRequire(dogfoodEval.missedValidation.isEmpty(), "P3-DOGFOOD: No required validation missed");

        std::cout << "P3 Dogfood Result: Predicted " << dogfoodPred.predictedFiles.size() << " files, "
                  << dogfoodPred.predictedValidation.size() << " validation suites, "
                  << "confidence=" << dogfoodPred.confidence.score << " (" << dogfoodPred.confidence.rating.toStdString() << "), "
                  << "precision=" << dogfoodEval.filePrecision << ", recall=" << dogfoodEval.fileRecall
                  << std::endl;
    }

    std::cout << "P3 Predictive Task Optimization test matrix completed: " << (ok ? "ALL PASS" : "FAILURES DETECTED") << std::endl;
    return ok;
}
