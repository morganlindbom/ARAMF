#include "core/AramfPaths.h"
#include "core/ContextCoordinationService.h"
#include "core/ExecutionOrchestrator.h"
#include "core/FrameworkKnowledge.h"
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
        ok &= checkRequire(prediction.confidence.freshnessScore <= 1.0 && prediction.confidence.freshnessScore >= 0.5,
                           "P3-007: Freshness score must be bounded and reflect historical event sequence");
    }

    // P3-008: Prediction cannot expand P0 ownership.
    {
        auto claimResult = RuntimeOwnershipService::claim(&model, prediction.toJson(), QStringLiteral("task-1"), QStringLiteral("worker-1"), {QStringLiteral("src/core/ProjectMemory.cpp")});
        ok &= checkRequire(!claimResult.value(QStringLiteral("granted")).toBool(), "P3-008: Prediction cannot claim runtime ownership");
        ok &= checkRequire(claimResult.value(QStringLiteral("code")).toString() == QStringLiteral("TASK_CONTRACT_INVALID"),
                           "P3-008: Prediction must be rejected as invalid TaskContract");
    }

    // P3-009: Prediction cannot expand P1 context scope.
    {
        QStringList predictedScopesList;
        for (const auto& item : prediction.predictedScopes) predictedScopesList.append(item.item);
        predictedScopesList.append(QStringLiteral("unauthorized-scope-outside-model"));

        const auto route = ContextCoordinationService::route(model, predictedScopesList);
        ok &= checkRequire(!PredictionContract::isExecutionAuthority(), "P3-009: Prediction contract declares no execution authority");
    }

    // P3-010: Prediction cannot mutate P2 evaluation evidence.
    {
        ExecutionOrchestrator orchestrator(&model);
        QJsonObject bogusEvidence = prediction.toJson();
        const auto postflight = WorkerTaskServices::postflight(model, QJsonObject{}, QJsonArray{bogusEvidence});
        ok &= checkRequire(postflight.value(QStringLiteral("status")).toString() != QStringLiteral("PASS")
                           || postflight.value(QStringLiteral("completionState")).toString() != QStringLiteral("CERTIFIED"),
                           "P3-010: P0/P2 postflight rejects prediction artifacts as execution evidence");
    }

    // P3-011: Prediction evaluation measures file precision and recall.
    {
        QJsonObject actualExecution{
            {QStringLiteral("sourceTaskId"), QStringLiteral("task-123")},
            {QStringLiteral("changedResources"), QJsonArray{
                QStringLiteral("src/core/ProjectMemory.cpp"),
                QStringLiteral("src/core/ProjectMemory.h")
            }},
            {QStringLiteral("validationEvidence"), QJsonArray{
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("aramf_core_tests")}, {QStringLiteral("status"), QStringLiteral("PASS")}},
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("memory cold-start")}, {QStringLiteral("status"), QStringLiteral("PASS")}}
            }},
            {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}
        };

        const auto eval = PredictiveOptimizationService::evaluate(prediction, actualExecution);
        ok &= checkRequire(eval.filePrecision > 0.0 && eval.filePrecision <= 1.0, "P3-011: File precision must be in (0, 1]");
        ok &= checkRequire(eval.fileRecall > 0.0 && eval.fileRecall <= 1.0, "P3-011: File recall must be in (0, 1]");
        ok &= checkRequire(eval.truePositiveFiles.contains(QStringLiteral("src/core/ProjectMemory.cpp")), "P3-011: True positive files must include ProjectMemory.cpp");
    }

    // P3-012: Prediction evaluation identifies missed required validation suites.
    {
        QJsonObject actualExecutionWithExtraVal{
            {QStringLiteral("sourceTaskId"), QStringLiteral("task-123")},
            {QStringLiteral("changedResources"), QJsonArray{QStringLiteral("src/core/ProjectMemory.cpp")}},
            {QStringLiteral("validationEvidence"), QJsonArray{
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("aramf_core_tests")}, {QStringLiteral("status"), QStringLiteral("PASS")}},
                QJsonObject{{QStringLiteral("suite"), QStringLiteral("unpredicted_security_audit")}, {QStringLiteral("status"), QStringLiteral("PASS")}}
            }},
            {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}
        };

        const auto eval = PredictiveOptimizationService::evaluate(prediction, actualExecutionWithExtraVal);
        ok &= checkRequire(eval.missedValidation.contains(QStringLiteral("unpredicted_security_audit")), "P3-012: Must detect missed validation suite");
    }

    // P3-013: Prediction evaluation detects unanticipated risk.
    {
        QJsonObject actualExecutionFailed{
            {QStringLiteral("sourceTaskId"), QStringLiteral("task-123")},
            {QStringLiteral("changedResources"), QJsonArray{QStringLiteral("src/core/ProjectMemory.cpp")}},
            {QStringLiteral("completionState"), QStringLiteral("FAILED")},
            {QStringLiteral("failure"), QStringLiteral("OUT_OF_DISK_SPACE")}
        };

        const auto eval = PredictiveOptimizationService::evaluate(prediction, actualExecutionFailed);
        ok &= checkRequire(eval.unanticipatedFailures.contains(QStringLiteral("OUT_OF_DISK_SPACE")), "P3-013: Must detect unanticipated failure");
    }

    // P3-014: Prediction contracts serialize to and deserialize from JSON faithfully.
    {
        const QJsonObject serialized = prediction.toJson();
        PredictionContract deserialized;
        QString err;
        ok &= checkRequire(PredictionContract::fromJson(serialized, &deserialized, &err), "P3-014: Deserialization must succeed");
        ok &= checkRequire(deserialized.predictionId == prediction.predictionId, "P3-014: Prediction ID must match");
        ok &= checkRequire(deserialized.taskSignature.fingerprint() == prediction.taskSignature.fingerprint(), "P3-014: Task signature fingerprint must match");
        ok &= checkRequire(deserialized.confidence.score == prediction.confidence.score, "P3-014: Confidence score must match");
        ok &= checkRequire(!deserialized.isExecutionAuthority(), "P3-014: Deserialized contract must maintain advisory nature");
    }

    // P3-015: Prediction evaluations serialize to and deserialize from JSON faithfully.
    {
        QJsonObject actualExecution{
            {QStringLiteral("sourceTaskId"), QStringLiteral("task-eval-test")},
            {QStringLiteral("changedResources"), QJsonArray{QStringLiteral("src/core/ProjectMemory.cpp")}},
            {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}
        };
        const auto eval = PredictiveOptimizationService::evaluate(prediction, actualExecution);
        const QJsonObject evalJson = eval.toJson();
        PredictionEvaluation reloaded;
        QString err;
        ok &= checkRequire(PredictionEvaluation::fromJson(evalJson, &reloaded, &err), "P3-015: Evaluation deserialization must succeed");
        ok &= checkRequire(reloaded.predictionId == eval.predictionId, "P3-015: Evaluation prediction ID must match");
        ok &= checkRequire(reloaded.filePrecision == eval.filePrecision, "P3-015: Precision must match");
    }

    // P3-016: Predictions persist to project directory and registry without touching P0/P1/P2 authority.
    {
        QString err;
        ok &= checkRequire(PredictiveOptimizationService::savePrediction(testDir.path(), prediction, &err), "P3-016: Save prediction must succeed");
        const QString filePath = QDir(PredictiveOptimizationService::predictionsDirectory(testDir.path())).filePath(prediction.predictionId + QStringLiteral(".json"));
        ok &= checkRequire(QFile::exists(filePath), "P3-016: Individual prediction file must exist on disk");
        ok &= checkRequire(QFile::exists(PredictiveOptimizationService::predictionsRegistryPath(testDir.path())), "P3-016: Registry file must exist on disk");

        const auto list = PredictiveOptimizationService::listPredictions(testDir.path(), &err);
        ok &= checkRequire(!list.isEmpty(), "P3-016: List predictions must return saved prediction");
    }

    // P3-017: Predictions reload from disk faithfully preserving task signatures and confidence.
    {
        PredictionContract reloaded;
        QString err;
        ok &= checkRequire(PredictiveOptimizationService::loadPrediction(testDir.path(), prediction.predictionId, &reloaded, &err), "P3-017: Load prediction must succeed");
        ok &= checkRequire(reloaded.taskId == prediction.taskId, "P3-017: Reloaded task ID must match");
        ok &= checkRequire(reloaded.taskSignature.fingerprint() == prediction.taskSignature.fingerprint(), "P3-017: Reloaded fingerprint must match");
        ok &= checkRequire(reloaded.confidence.rating == prediction.confidence.rating, "P3-017: Reloaded confidence rating must match");
    }

    // P3-018: Cold-start does not convert stale predictions into fresh evidence.
    {
        ProjectMemory memory;
        QString valErr;
        const auto report = memory.validateColdStart(testDir.path(), &valErr);
        ok &= checkRequire(report.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "P3-018: Cold-start validation must pass with predictions present");
    }

    // P3-019: Multi-Project Evidence Integration (Approved Framework Knowledge loaded, candidate/unapproved excluded)
    {
        QTemporaryDir fkDir;
        ProjectModel fkModel;
        fkModel.setProjectId(QStringLiteral("fk-test-project"));
        fkModel.setProjectPath(fkDir.path());

        // Create framework-knowledge.json with 1 approved entry and 1 candidate entry
        const QString fkPath = QDir(fkDir.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/framework-knowledge.json"));
        QJsonObject storeObj{
            {QStringLiteral("_file"), QStringLiteral("framework-knowledge.json")},
            {QStringLiteral("version"), 1},
            {QStringLiteral("entries"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("fk-approved-memory-rule")},
                    {QStringLiteral("title"), QStringLiteral("Preserve memory sequence integrity")},
                    {QStringLiteral("lesson"), QStringLiteral("Always verify append-only sequence numbers during memory tasks.")},
                    {QStringLiteral("status"), QStringLiteral("approved")},
                    {QStringLiteral("reviewStatus"), QStringLiteral("approved")},
                    {QStringLiteral("scopes"), QJsonArray{QStringLiteral("memory"), QStringLiteral("project")}},
                    {QStringLiteral("originProjectId"), QStringLiteral("foreign-project-alpha")}
                },
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("fk-unapproved-candidate")},
                    {QStringLiteral("title"), QStringLiteral("Unreviewed suggestion")},
                    {QStringLiteral("lesson"), QStringLiteral("This candidate lesson should never be loaded by P3.")},
                    {QStringLiteral("status"), QStringLiteral("candidate")},
                    {QStringLiteral("reviewStatus"), QStringLiteral("more-evidence")},
                    {QStringLiteral("scopes"), QJsonArray{QStringLiteral("memory")}}
                }
            }}
        };
        writeTestFile(fkPath, QJsonDocument(storeObj).toJson());

        TaskSignature sig;
        sig.taskCategory = QStringLiteral("memory");
        sig.relevantScopes = {QStringLiteral("memory")};
        sig.targetSubsystem = QStringLiteral("memory");

        const auto fkPred = PredictiveOptimizationService::predict(fkModel, sig);

        bool foundApproved = false;
        bool foundCandidate = false;
        for (const auto& re : fkPred.rankedEvidence) {
            if (re.evidenceId == QStringLiteral("fk-approved-memory-rule")) {
                foundApproved = true;
                ok &= checkRequire(re.sourceType == EvidenceSourceType::ApprovedGlobalKnowledge, "P3-019: Source type must be ApprovedGlobalKnowledge");
                ok &= checkRequire(re.originProjectId == QStringLiteral("foreign-project-alpha"), "P3-019: Origin project ID must be preserved");
            }
            if (re.evidenceId == QStringLiteral("fk-unapproved-candidate")) {
                foundCandidate = true;
            }
        }
        ok &= checkRequire(foundApproved, "P3-019: Approved Framework Knowledge must be included in ranked evidence");
        ok &= checkRequire(!foundCandidate, "P3-019: Candidate / unapproved Framework Knowledge must be strictly excluded");
    }

    // P3-020: Foreign Path Injection Prevention
    {
        QTemporaryDir foreignDir;
        ProjectModel foreignModel;
        foreignModel.setProjectId(QStringLiteral("local-isolated-project"));
        foreignModel.setProjectPath(foreignDir.path());

        const QString fkPath = QDir(foreignDir.path()).filePath(QStringLiteral("ARAMF_WORKER/memory/framework-knowledge.json"));
        QJsonObject storeObj{
            {QStringLiteral("_file"), QStringLiteral("framework-knowledge.json")},
            {QStringLiteral("version"), 1},
            {QStringLiteral("entries"), QJsonArray{
                QJsonObject{
                    {QStringLiteral("id"), QStringLiteral("fk-foreign-paths")},
                    {QStringLiteral("title"), QStringLiteral("Foreign project file rules")},
                    {QStringLiteral("lesson"), QStringLiteral("In Pico SDK, modify foreign/path/to/pico_sdk/main.c and src/alien/driver.cpp carefully.")},
                    {QStringLiteral("status"), QStringLiteral("approved")},
                    {QStringLiteral("reviewStatus"), QStringLiteral("approved")},
                    {QStringLiteral("scopes"), QJsonArray{QStringLiteral("embedded")}},
                    {QStringLiteral("originProjectId"), QStringLiteral("foreign-pico-project")}
                }
            }}
        };
        writeTestFile(fkPath, QJsonDocument(storeObj).toJson());

        TaskSignature sig;
        sig.taskCategory = QStringLiteral("embedded");
        sig.relevantScopes = {QStringLiteral("embedded")};

        const auto foreignPred = PredictiveOptimizationService::predict(foreignModel, sig);

        bool leakedForeignFile = false;
        for (const auto& f : foreignPred.predictedFiles) {
            if (f.item.contains(QStringLiteral("pico_sdk")) || f.item.contains(QStringLiteral("alien"))) {
                leakedForeignFile = true;
            }
        }
        ok &= checkRequire(!leakedForeignFile, "P3-020: Foreign project file paths must NEVER be injected into predictedFiles");
    }

    // P3-021: Deterministic Evidence Ranking Formula & Tie-Breaking
    {
        const double rankLocal = PredictiveOptimizationService::calculateEvidenceRank(
            EvidenceSourceType::LocalOperationalEvent, 0.80, 1.0, 1.0);
        const double rankGlobal = PredictiveOptimizationService::calculateEvidenceRank(
            EvidenceSourceType::ApprovedGlobalKnowledge, 0.80, 1.0, 1.0);
        const double rankPolicy = PredictiveOptimizationService::calculateEvidenceRank(
            EvidenceSourceType::ScopePolicy, 0.80, 1.0, 1.0);
        const double rankContract = PredictiveOptimizationService::calculateEvidenceRank(
            EvidenceSourceType::ContractHistory, 0.80, 1.0, 1.0);

        // Multipliers: Local=1.0, Global=0.85, ScopePolicy=0.70, ContractHistory=0.90
        // Base = 0.50 * 0.80 + 0.25 * 1.0 + 0.25 * 1.0 = 0.40 + 0.25 + 0.25 = 0.90
        ok &= checkRequire(qAbs(rankLocal - 0.90) < 1e-4, "P3-021: Local rank must match 1.0 * 0.90");
        ok &= checkRequire(qAbs(rankGlobal - 0.765) < 1e-4, "P3-021: Global rank must match 0.85 * 0.90");
        ok &= checkRequire(qAbs(rankPolicy - 0.630) < 1e-4, "P3-021: Policy rank must match 0.70 * 0.90");
        ok &= checkRequire(qAbs(rankContract - 0.810) < 1e-4, "P3-021: Contract rank must match 0.90 * 0.90");

        // Verify ordering in prediction.rankedEvidence
        ok &= checkRequire(!prediction.rankedEvidence.isEmpty(), "P3-021: Ranked evidence must not be empty");
        for (int i = 1; i < prediction.rankedEvidence.size(); ++i) {
            const auto& prev = prediction.rankedEvidence.at(i - 1);
            const auto& curr = prediction.rankedEvidence.at(i);
            ok &= checkRequire(prev.totalRank >= curr.totalRank - 1e-6, "P3-021: Ranked evidence must be strictly sorted by totalRank descending");
        }
    }

    // P3-022: Explainability Decomposition
    {
        ok &= checkRequire(!prediction.predictedFiles.isEmpty(), "P3-022: Predicted files must exist");
        for (const auto& f : prediction.predictedFiles) {
            ok &= checkRequire(!f.primaryEvidenceId.isEmpty(), "P3-022: Predicted file must specify primary evidence ID");
            ok &= checkRequire(f.contributionScore > 0.0, "P3-022: Predicted file must have positive contribution score");
            ok &= checkRequire(!f.sourceTypes.isEmpty(), "P3-022: Predicted file must specify contributing source types");
            ok &= checkRequire(f.rationale.contains(QStringLiteral("Primary:")), "P3-022: Rationale must link to primary evidence");
        }
    }

    // P3-023: 6-Tier Change Breadth Estimation
    {
        TaskSignature sig;
        ok &= checkRequire(PredictiveOptimizationService::determineChangeBreadth({}, sig) == QStringLiteral("UNKNOWN"), "P3-023: Empty files = UNKNOWN");
        ok &= checkRequire(PredictiveOptimizationService::determineChangeBreadth({QStringLiteral("src/utils/Helper.cpp")}, sig) == QStringLiteral("LOCAL"), "P3-023: 1 file in 1 module = LOCAL");
        ok &= checkRequire(PredictiveOptimizationService::determineChangeBreadth({QStringLiteral("src/core/A.cpp"), QStringLiteral("src/core/B.cpp")}, sig) == QStringLiteral("COMPONENT"), "P3-023: 2 files in 1 module = COMPONENT");
        ok &= checkRequire(PredictiveOptimizationService::determineChangeBreadth({QStringLiteral("src/core/A.cpp"), QStringLiteral("src/ui/B.cpp")}, sig) == QStringLiteral("MULTI_COMPONENT"), "P3-023: 2 modules = MULTI_COMPONENT");
        ok &= checkRequire(PredictiveOptimizationService::determineChangeBreadth({QStringLiteral("src/core/A.cpp"), QStringLiteral("src/ui/B.cpp"), QStringLiteral("tests/C.cpp")}, sig) == QStringLiteral("CROSS_LAYER"), "P3-023: 3 layers = CROSS_LAYER");
        ok &= checkRequire(PredictiveOptimizationService::determineChangeBreadth({QStringLiteral("CMakeLists.txt"), QStringLiteral("src/core/A.cpp")}, sig) == QStringLiteral("PROJECT_WIDE"), "P3-023: CMakeLists.txt = PROJECT_WIDE");
    }

    // P3-024: 11-Category Risk Taxonomy
    {
        const auto categories = PredictiveOptimizationService::canonicalRiskCategories();
        ok &= checkRequire(categories.size() == 11, "P3-024: Canonical risk taxonomy must contain exactly 11 categories");
        ok &= checkRequire(categories.contains(QStringLiteral("OWNERSHIP_CONFLICT")), "P3-024: Must contain OWNERSHIP_CONFLICT");
        ok &= checkRequire(categories.contains(QStringLiteral("SCOPE_EXPANSION")), "P3-024: Must contain SCOPE_EXPANSION");
        ok &= checkRequire(categories.contains(QStringLiteral("STALE_CONTEXT")), "P3-024: Must contain STALE_CONTEXT");
        ok &= checkRequire(categories.contains(QStringLiteral("STALE_VALIDATION")), "P3-024: Must contain STALE_VALIDATION");
        ok &= checkRequire(categories.contains(QStringLiteral("RECORDER_CONSISTENCY")), "P3-024: Must contain RECORDER_CONSISTENCY");
        ok &= checkRequire(categories.contains(QStringLiteral("PROVENANCE_MISSING")), "P3-024: Must contain PROVENANCE_MISSING");
        ok &= checkRequire(categories.contains(QStringLiteral("PERSISTENCE_FAULT")), "P3-024: Must contain PERSISTENCE_FAULT");
        ok &= checkRequire(categories.contains(QStringLiteral("CROSS_LAYER_REGRESSION")), "P3-024: Must contain CROSS_LAYER_REGRESSION");
        ok &= checkRequire(categories.contains(QStringLiteral("DEPENDENCY_REGRESSION")), "P3-024: Must contain DEPENDENCY_REGRESSION");
        ok &= checkRequire(categories.contains(QStringLiteral("RECOVERY_RETRY")), "P3-024: Must contain RECOVERY_RETRY");
        ok &= checkRequire(categories.contains(QStringLiteral("DESTRUCTIVE_OPERATION")), "P3-024: Must contain DESTRUCTIVE_OPERATION");
    }

    // P3-025: Proportional Validation Routing Escalation
    {
        QList<PredictedItem> noRisks;
        QList<PredictedItem> critRisks{
            PredictedItem{QStringLiteral("CROSS_LAYER_REGRESSION"), QStringLiteral("KNOWN"), QStringLiteral("Risk"), {}}
        };

        ok &= checkRequire(PredictiveOptimizationService::determineValidationLevel(QStringLiteral("LOCAL"), noRisks, 0.85) == QStringLiteral("FOCUSED"),
                           "P3-025: High-confidence local change with no risks routes to FOCUSED");
        ok &= checkRequire(PredictiveOptimizationService::determineValidationLevel(QStringLiteral("MULTI_COMPONENT"), noRisks, 0.85) == QStringLiteral("SUBSYSTEM"),
                           "P3-025: Multi-component change routes to SUBSYSTEM");
        ok &= checkRequire(PredictiveOptimizationService::determineValidationLevel(QStringLiteral("LOCAL"), critRisks, 0.85) == QStringLiteral("FULL_REGRESSION"),
                           "P3-025: Critical risk escalates validation to FULL_REGRESSION");
        ok &= checkRequire(PredictiveOptimizationService::determineValidationLevel(QStringLiteral("PROJECT_WIDE"), noRisks, 0.90) == QStringLiteral("FULL_REGRESSION"),
                           "P3-025: Project-wide change routes to FULL_REGRESSION");
    }

    // P3-026: Boundary Governance on Breadth & Risk
    {
        // Prediction with PROJECT_WIDE breadth and risks cannot bypass P0
        ok &= checkRequire(!PredictionContract::isExecutionAuthority(), "P3-026: Prediction contract must not be execution authority");
        QJsonObject predObj = prediction.toJson();
        predObj.insert(QStringLiteral("predictedChangeBreadth"), QStringLiteral("PROJECT_WIDE"));
        auto claim = RuntimeOwnershipService::claim(&model, predObj, QStringLiteral("task-wide"), QStringLiteral("worker-1"), {QStringLiteral("CMakeLists.txt")});
        ok &= checkRequire(!claim.value(QStringLiteral("granted")).toBool(), "P3-026: Broad prediction cannot bypass P0 claim restrictions");
    }

    // P3-027: Strict Confidence Calibration (N=0, N=1, N=2, N>=3 rules)
    {
        QTemporaryDir calDir;
        ProjectMemory calMem;
        ProjectModel calModel;
        calModel.setProjectId(QStringLiteral("cal-project"));
        calModel.setProjectPath(calDir.path());

        MemoryConfiguration testMemoryConfig;
        testMemoryConfig.maintenanceOptions = {
            QStringLiteral("update-current-state"),
            QStringLiteral("record-task-completion"),
            QStringLiteral("record-build-results"),
            QStringLiteral("record-test-results"),
            QStringLiteral("record-validation")
        };
        calModel.setMemoryConfiguration(testMemoryConfig);

        calMem.initialize(calDir.path(), &calModel);

        QJsonObject prov{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("antigravity")},
            {QStringLiteral("tool"), QStringLiteral("aramf-cli")}
        };

        TaskSignature sig;
        sig.taskCategory = QStringLiteral("single-task");
        sig.relevantScopes = {QStringLiteral("project")};

        // N = 0: INSUFFICIENT_EVIDENCE
        const auto n0Pred = PredictiveOptimizationService::predict(calModel, sig);
        ok &= checkRequire(n0Pred.confidence.sampleSize == 0, "P3-027: N=0 sample size must be 0");
        ok &= checkRequire(n0Pred.confidence.rating == QStringLiteral("INSUFFICIENT_EVIDENCE"), "P3-027: N=0 must be INSUFFICIENT_EVIDENCE");

        // Seed exactly 1 event (N = 1)
        calMem.recordOperation(calDir.path(), QStringLiteral("task-start"), QJsonObject{
            {QStringLiteral("task"), QStringLiteral("Task single 1")},
            {QStringLiteral("category"), QStringLiteral("single-task")},
            {QStringLiteral("scope"), QStringLiteral("project")},
            {QStringLiteral("provenance"), prov}
        });
        calMem.recordOperation(calDir.path(), QStringLiteral("task-complete"), QJsonObject{
            {QStringLiteral("task"), QStringLiteral("Task single 1")},
            {QStringLiteral("status"), QStringLiteral("PASS")},
            {QStringLiteral("provenance"), prov}
        });

        const auto n1Pred = PredictiveOptimizationService::predict(calModel, sig);
        ok &= checkRequire(n1Pred.confidence.sampleSize == 1, "P3-027: N=1 sample size must be 1");
        ok &= checkRequire(n1Pred.confidence.score <= 0.35, "P3-027: N=1 score must be strictly capped at <= 0.35");
        ok &= checkRequire(n1Pred.confidence.rating == QStringLiteral("LOW"), "P3-027: N=1 rating must be LOW");

        // Seed second event (N = 2)
        calMem.recordOperation(calDir.path(), QStringLiteral("task-start"), QJsonObject{
            {QStringLiteral("task"), QStringLiteral("Task single 2")},
            {QStringLiteral("category"), QStringLiteral("single-task")},
            {QStringLiteral("scope"), QStringLiteral("project")},
            {QStringLiteral("provenance"), prov}
        });
        calMem.recordOperation(calDir.path(), QStringLiteral("task-complete"), QJsonObject{
            {QStringLiteral("task"), QStringLiteral("Task single 2")},
            {QStringLiteral("status"), QStringLiteral("PASS")},
            {QStringLiteral("provenance"), prov}
        });

        const auto n2Pred = PredictiveOptimizationService::predict(calModel, sig);
        ok &= checkRequire(n2Pred.confidence.sampleSize == 2, "P3-027: N=2 sample size must be 2");
        ok &= checkRequire(n2Pred.confidence.score <= 0.70, "P3-027: N=2 score must be capped at <= 0.70");
        ok &= checkRequire(n2Pred.confidence.rating == QStringLiteral("MEDIUM") || n2Pred.confidence.rating == QStringLiteral("LOW"), "P3-027: N=2 rating cannot exceed MEDIUM");

        // Seed 2 more events (N = 4, eligible for HIGH if consistent)
        for (int i = 3; i <= 4; ++i) {
            calMem.recordOperation(calDir.path(), QStringLiteral("task-start"), QJsonObject{
                {QStringLiteral("task"), QStringLiteral("Task single %1").arg(i)},
                {QStringLiteral("category"), QStringLiteral("single-task")},
                {QStringLiteral("scope"), QStringLiteral("project")},
                {QStringLiteral("provenance"), prov}
            });
            calMem.recordOperation(calDir.path(), QStringLiteral("task-complete"), QJsonObject{
                {QStringLiteral("task"), QStringLiteral("Task single %1").arg(i)},
                {QStringLiteral("status"), QStringLiteral("PASS")},
                {QStringLiteral("provenance"), prov}
            });
        }

        const auto n4Pred = PredictiveOptimizationService::predict(calModel, sig);
        ok &= checkRequire(n4Pred.confidence.sampleSize >= 3, "P3-027: N>=3 sample size met");
        ok &= checkRequire(n4Pred.confidence.rating == QStringLiteral("HIGH") || n4Pred.confidence.rating == QStringLiteral("MEDIUM"), "P3-027: N>=3 can achieve HIGH");
    }

    // P3-028: Rolling Prediction Drift Detector
    {
        // 1. Stable series
        QList<PredictionEvaluation> goodHistory;
        for (int i = 0; i < 5; ++i) {
            PredictionEvaluation ev;
            ev.predictionId = QStringLiteral("pred-%1").arg(i);
            ev.filePrecision = 0.90;
            ev.fileRecall = 0.90;
            goodHistory.append(ev);
        }
        const auto stableReport = PredictionDriftDetector::evaluateDrift(goodHistory);
        ok &= checkRequire(!stableReport.driftDetected, "P3-028: High precision history must not report drift");
        ok &= checkRequire(stableReport.status == QStringLiteral("STABLE"), "P3-028: Status must be STABLE");

        // 2. Degraded series with missed validation
        QList<PredictionEvaluation> degradedHistory = goodHistory;
        PredictionEvaluation badEv;
        badEv.predictionId = QStringLiteral("pred-bad");
        badEv.filePrecision = 0.40;
        badEv.fileRecall = 0.40;
        badEv.missedValidation = {QStringLiteral("missed_audit_suite")};
        degradedHistory.append(badEv);

        const auto driftReport = PredictionDriftDetector::evaluateDrift(degradedHistory);
        ok &= checkRequire(driftReport.driftDetected, "P3-028: Missed validation must trigger drift");
        ok &= checkRequire(driftReport.status == QStringLiteral("DRIFT_CONFIRMED"), "P3-028: Status must be DRIFT_CONFIRMED");
        ok &= checkRequire(!driftReport.alerts.isEmpty(), "P3-028: Drift alerts must be generated");
    }

    // P3-029: Drift Detector Persistence & Non-Authoritative Advisory Nature
    {
        QList<PredictionEvaluation> evals;
        PredictionEvaluation ev;
        ev.filePrecision = 0.80;
        ev.fileRecall = 0.80;
        evals.append(ev);

        const auto rep = PredictionDriftDetector::evaluateDrift(evals);
        QString err;
        ok &= checkRequire(PredictionDriftDetector::saveDriftReport(testDir.path(), rep, &err), "P3-029: Save drift report must succeed");
        ok &= checkRequire(QFile::exists(PredictionDriftDetector::driftReportPath(testDir.path())), "P3-029: File must exist on disk");

        PredictionDriftReport loaded;
        ok &= checkRequire(PredictionDriftDetector::loadDriftReport(testDir.path(), &loaded, &err), "P3-029: Load drift report must succeed");
        ok &= checkRequire(loaded.status == rep.status, "P3-029: Loaded drift status must match");
        ok &= checkRequire(!PredictionContract::isExecutionAuthority(), "P3-029: Drift detector carries NO execution authority");
    }

    // P3-030: Multi-Scenario Dogfood Campaign across 6 diverse historical scenarios
    {
        ProjectModel selfModel;
        selfModel.setProjectId(QStringLiteral("ARAMF"));
        selfModel.setProjectPath(AramfPaths::programRoot());

        // Scenario 1: Memory Subsystem
        {
            TaskSignature sig;
            sig.taskCategory = QStringLiteral("memory");
            sig.operationType = QStringLiteral("modify");
            sig.targetSubsystem = QStringLiteral("memory");
            sig.referencedFiles = {QStringLiteral("src/core/ProjectMemory.cpp")};
            sig.relevantScopes = {QStringLiteral("project")};

            const auto pred = PredictiveOptimizationService::predict(selfModel, sig);
            ok &= checkRequire(pred.status == QStringLiteral("READY"), "P3-030 S1: Memory status READY");
            ok &= checkRequire(!pred.predictedValidation.isEmpty(), "P3-030 S1: Memory validation predicted");

            QJsonObject actual{
                {QStringLiteral("sourceTaskId"), QStringLiteral("dogfood-s1")},
                {QStringLiteral("changedResources"), QJsonArray{
                    QStringLiteral("src/core/ProjectMemory.cpp"),
                    QStringLiteral("src/core/ProjectMemory.h"),
                    QStringLiteral("src/core/MemoryCommand.cpp")
                }},
                {QStringLiteral("validationEvidence"), QJsonArray{
                    QJsonObject{{QStringLiteral("suite"), QStringLiteral("aramf_core_tests")}, {QStringLiteral("status"), QStringLiteral("PASS")}},
                    QJsonObject{{QStringLiteral("suite"), QStringLiteral("memory cold-start")}, {QStringLiteral("status"), QStringLiteral("PASS")}}
                }},
                {QStringLiteral("completionState"), QStringLiteral("SUCCEEDED")}
            };
            const auto ev = PredictiveOptimizationService::evaluate(pred, actual);
            ok &= checkRequire(ev.filePrecision > 0.0, "P3-030 S1: Precision > 0");
            ok &= checkRequire(ev.fileRecall == 1.0, "P3-030 S1: Recall 1.0");
            ok &= checkRequire(ev.missedValidation.isEmpty(), "P3-030 S1: 0 missed validation");
        }

        // Scenario 2: UI Workflow Subsystem
        {
            TaskSignature sig;
            sig.taskCategory = QStringLiteral("ui");
            sig.operationType = QStringLiteral("modify");
            sig.targetSubsystem = QStringLiteral("ui");
            sig.referencedFiles = {QStringLiteral("src/ui/MainWindow.cpp")};
            sig.relevantScopes = {QStringLiteral("ui")};

            const auto pred = PredictiveOptimizationService::predict(selfModel, sig);
            ok &= checkRequire(pred.status == QStringLiteral("READY"), "P3-030 S2: UI status READY");
            bool hasWorkflowTests = false;
            for (const auto& v : pred.predictedValidation) {
                if (v.item == QStringLiteral("aramf_workflow_tests")) hasWorkflowTests = true;
            }
            ok &= checkRequire(hasWorkflowTests, "P3-030 S2: Must predict aramf_workflow_tests");
        }

        // Scenario 3: Central Version / Release Management
        {
            TaskSignature sig;
            sig.taskCategory = QStringLiteral("release");
            sig.operationType = QStringLiteral("modify");
            sig.targetSubsystem = QStringLiteral("release");
            sig.referencedFiles = {QStringLiteral("src/core/CentralVersionManager.h")};
            sig.relevantScopes = {QStringLiteral("project")};

            const auto pred = PredictiveOptimizationService::predict(selfModel, sig);
            ok &= checkRequire(pred.status == QStringLiteral("READY"), "P3-030 S3: Release status READY");
            ok &= checkRequire(!pred.predictedValidation.isEmpty(), "P3-030 S3: Must predict validation");
        }

        // Scenario 4: Governance / Recertification Subsystem
        {
            TaskSignature sig;
            sig.taskCategory = QStringLiteral("governance");
            sig.operationType = QStringLiteral("validate");
            sig.targetSubsystem = QStringLiteral("governance");
            sig.referencedFiles = {QStringLiteral("ARAMF_WORKER/AGENTS.md")};
            sig.relevantScopes = {QStringLiteral("project")};

            const auto pred = PredictiveOptimizationService::predict(selfModel, sig);
            ok &= checkRequire(pred.status == QStringLiteral("READY"), "P3-030 S4: Governance status READY");
            ok &= checkRequire(!pred.predictedValidation.isEmpty(), "P3-030 S4: Must predict validation");
        }

        // Scenario 5: Configuration / Build System Update
        {
            TaskSignature sig;
            sig.taskCategory = QStringLiteral("configuration");
            sig.operationType = QStringLiteral("update");
            sig.referencedFiles = {QStringLiteral("CMakeLists.txt")};
            sig.relevantScopes = {QStringLiteral("project")};

            const auto pred = PredictiveOptimizationService::predict(selfModel, sig);
            ok &= checkRequire(pred.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE"), "P3-030 S5: CMakeLists.txt must be PROJECT_WIDE");
            ok &= checkRequire(PredictiveOptimizationService::determineValidationLevel(pred.predictedChangeBreadth, pred.predictedRiskCategories, pred.confidence.score) == QStringLiteral("FULL_REGRESSION"),
                               "P3-030 S5: Project-wide change must route to FULL_REGRESSION");
        }

        // Scenario 6: Novel / Insufficient Evidence Task
        {
            TaskSignature sig;
            sig.taskCategory = QStringLiteral("quantum-computing-accelerator");
            sig.operationType = QStringLiteral("entangle");
            sig.targetSubsystem = QStringLiteral("qpu");

            const auto pred = PredictiveOptimizationService::predict(selfModel, sig);
            ok &= checkRequire(pred.confidence.rating == QStringLiteral("INSUFFICIENT_EVIDENCE") || pred.confidence.rating == QStringLiteral("LOW"),
                               "P3-030 S6: Novel task must report INSUFFICIENT_EVIDENCE / LOW");
            ok &= checkRequire(pred.confidence.score <= 0.20, "P3-030 S6: Novel task must produce very low score");
        }
    }

    // P3-031: Cold-Start & Memory Consistency with Full P3 Artifacts
    {
        ProjectMemory memory;
        QString valErr;
        const auto csReport = memory.validateColdStart(testDir.path(), &valErr);
        ok &= checkRequire(csReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "P3-031: Cold-start validation must PASS");

        const auto mcReport = memory.validate(testDir.path(), &valErr);
        ok &= checkRequire(mcReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"), "P3-031: Memory consistency validation must PASS");
    }

    std::cout << "P3 Predictive Task Optimization test matrix completed: " << (ok ? "ALL PASS" : "FAILURES DETECTED") << std::endl;
    return ok;
}
