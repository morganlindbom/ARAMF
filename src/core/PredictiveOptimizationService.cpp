#include "PredictiveOptimizationService.h"
#include "AramfPaths.h"
#include "ProjectMemory.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QUuid>

namespace {
QJsonArray stringListToArray(const QStringList& list)
{
    return QJsonArray::fromStringList(list);
}

QStringList arrayToStringList(const QJsonArray& arr)
{
    QStringList result;
    for (const auto& item : arr) {
        const QString s = item.toString().trimmed();
        if (!s.isEmpty()) result.append(s);
    }
    return result;
}

QJsonArray predictedItemsToArray(const QList<PredictedItem>& items)
{
    QJsonArray arr;
    for (const auto& item : items) {
        arr.append(item.toJson());
    }
    return arr;
}

QList<PredictedItem> arrayToPredictedItems(const QJsonArray& arr)
{
    QList<PredictedItem> items;
    for (const auto& val : arr) {
        items.append(PredictedItem::fromJson(val.toObject()));
    }
    return items;
}

struct HistoricalTaskCluster {
    QString taskName;
    QString category;
    QString scope;
    QStringList eventIds;
    QList<int> sequenceNumbers;
    bool completed = false;
    bool success = true;
    QStringList testSuites;
    QStringList mentionedFiles;
    QStringList failures;
    double similarity = 0.0;
    double freshness = 1.0;
};
}

QJsonObject PredictedItem::toJson() const
{
    return QJsonObject{
        {QStringLiteral("item"), item},
        {QStringLiteral("status"), status},
        {QStringLiteral("rationale"), rationale},
        {QStringLiteral("evidenceReferences"), stringListToArray(evidenceReferences)}
    };
}

PredictedItem PredictedItem::fromJson(const QJsonObject& obj)
{
    PredictedItem result;
    result.item = obj.value(QStringLiteral("item")).toString();
    result.status = obj.value(QStringLiteral("status")).toString();
    result.rationale = obj.value(QStringLiteral("rationale")).toString();
    result.evidenceReferences = arrayToStringList(obj.value(QStringLiteral("evidenceReferences")).toArray());
    return result;
}

QJsonObject PredictionConfidence::toJson() const
{
    return QJsonObject{
        {QStringLiteral("score"), score},
        {QStringLiteral("rating"), rating},
        {QStringLiteral("sampleSize"), sampleSize},
        {QStringLiteral("consistencyScore"), consistencyScore},
        {QStringLiteral("freshnessScore"), freshnessScore},
        {QStringLiteral("matchPrecision"), matchPrecision},
        {QStringLiteral("conflictingEvidencePenalty"), conflictingEvidencePenalty},
        {QStringLiteral("explanation"), explanation}
    };
}

PredictionConfidence PredictionConfidence::fromJson(const QJsonObject& obj)
{
    PredictionConfidence conf;
    conf.score = obj.value(QStringLiteral("score")).toDouble(0.0);
    conf.rating = obj.value(QStringLiteral("rating")).toString(QStringLiteral("INSUFFICIENT_EVIDENCE"));
    conf.sampleSize = obj.value(QStringLiteral("sampleSize")).toInt(0);
    conf.consistencyScore = obj.value(QStringLiteral("consistencyScore")).toDouble(0.0);
    conf.freshnessScore = obj.value(QStringLiteral("freshnessScore")).toDouble(1.0);
    conf.matchPrecision = obj.value(QStringLiteral("matchPrecision")).toDouble(0.0);
    conf.conflictingEvidencePenalty = obj.value(QStringLiteral("conflictingEvidencePenalty")).toDouble(0.0);
    conf.explanation = obj.value(QStringLiteral("explanation")).toString();
    return conf;
}

QJsonObject PredictionContract::toJson() const
{
    return QJsonObject{
        {QStringLiteral("predictionId"), predictionId},
        {QStringLiteral("taskId"), taskId},
        {QStringLiteral("taskSignature"), taskSignature.toJson()},
        {QStringLiteral("schemaVersion"), schemaVersion},
        {QStringLiteral("taskClassification"), taskClassification},
        {QStringLiteral("predictedScopes"), predictedItemsToArray(predictedScopes)},
        {QStringLiteral("predictedFiles"), predictedItemsToArray(predictedFiles)},
        {QStringLiteral("predictedValidation"), predictedItemsToArray(predictedValidation)},
        {QStringLiteral("predictedRiskCategories"), predictedItemsToArray(predictedRiskCategories)},
        {QStringLiteral("predictedChangeBreadth"), predictedChangeBreadth},
        {QStringLiteral("breadthRationale"), breadthRationale},
        {QStringLiteral("confidence"), confidence.toJson()},
        {QStringLiteral("evidenceReferences"), stringListToArray(evidenceReferences)},
        {QStringLiteral("provenance"), provenance},
        {QStringLiteral("createdAt"), createdAt},
        {QStringLiteral("sourceProjectId"), sourceProjectId},
        {QStringLiteral("sourceProjectPath"), sourceProjectPath},
        {QStringLiteral("advisoryStatus"), advisoryStatus},
        {QStringLiteral("status"), status}
    };
}

bool PredictionContract::fromJson(const QJsonObject& obj, PredictionContract* result, QString* error)
{
    if (!result) return false;
    const QString pid = obj.value(QStringLiteral("predictionId")).toString();
    if (pid.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Prediction contract requires a non-empty predictionId.");
        return false;
    }

    PredictionContract contract;
    contract.predictionId = pid;
    contract.taskId = obj.value(QStringLiteral("taskId")).toString();
    contract.schemaVersion = obj.value(QStringLiteral("schemaVersion")).toString(QStringLiteral("1.0"));
    contract.taskClassification = obj.value(QStringLiteral("taskClassification")).toObject();
    contract.taskSignature = TaskSignature::fromJson(obj.value(QStringLiteral("taskSignature")).toObject());
    contract.predictedScopes = arrayToPredictedItems(obj.value(QStringLiteral("predictedScopes")).toArray());
    contract.predictedFiles = arrayToPredictedItems(obj.value(QStringLiteral("predictedFiles")).toArray());
    contract.predictedValidation = arrayToPredictedItems(obj.value(QStringLiteral("predictedValidation")).toArray());
    contract.predictedRiskCategories = arrayToPredictedItems(obj.value(QStringLiteral("predictedRiskCategories")).toArray());
    contract.predictedChangeBreadth = obj.value(QStringLiteral("predictedChangeBreadth")).toString();
    contract.breadthRationale = obj.value(QStringLiteral("breadthRationale")).toString();
    contract.confidence = PredictionConfidence::fromJson(obj.value(QStringLiteral("confidence")).toObject());
    contract.evidenceReferences = arrayToStringList(obj.value(QStringLiteral("evidenceReferences")).toArray());
    contract.provenance = obj.value(QStringLiteral("provenance")).toObject();
    contract.createdAt = obj.value(QStringLiteral("createdAt")).toString();
    contract.sourceProjectId = obj.value(QStringLiteral("sourceProjectId")).toString();
    contract.sourceProjectPath = obj.value(QStringLiteral("sourceProjectPath")).toString();
    contract.advisoryStatus = obj.value(QStringLiteral("advisoryStatus")).toString(QStringLiteral("ADVISORY"));
    contract.status = obj.value(QStringLiteral("status")).toString(QStringLiteral("READY"));

    *result = contract;
    return true;
}

QJsonObject PredictionEvaluation::toJson() const
{
    return QJsonObject{
        {QStringLiteral("predictionId"), predictionId},
        {QStringLiteral("actualTaskId"), actualTaskId},
        {QStringLiteral("predictedFiles"), stringListToArray(predictedFiles)},
        {QStringLiteral("actualModifiedFiles"), stringListToArray(actualModifiedFiles)},
        {QStringLiteral("truePositiveFiles"), stringListToArray(truePositiveFiles)},
        {QStringLiteral("falsePositiveFiles"), stringListToArray(falsePositiveFiles)},
        {QStringLiteral("falseNegativeFiles"), stringListToArray(falseNegativeFiles)},
        {QStringLiteral("filePrecision"), filePrecision},
        {QStringLiteral("fileRecall"), fileRecall},
        {QStringLiteral("changeBreadthComparison"), changeBreadthComparison},
        {QStringLiteral("predictedScopes"), stringListToArray(predictedScopes)},
        {QStringLiteral("actualScopes"), stringListToArray(actualScopes)},
        {QStringLiteral("matchedScopes"), stringListToArray(matchedScopes)},
        {QStringLiteral("missedScopes"), stringListToArray(missedScopes)},
        {QStringLiteral("unnecessaryScopes"), stringListToArray(unnecessaryScopes)},
        {QStringLiteral("scopeMatch"), scopeMatch},
        {QStringLiteral("predictedValidation"), stringListToArray(predictedValidation)},
        {QStringLiteral("actualValidation"), stringListToArray(actualValidation)},
        {QStringLiteral("matchedValidation"), stringListToArray(matchedValidation)},
        {QStringLiteral("missedValidation"), stringListToArray(missedValidation)},
        {QStringLiteral("unnecessaryValidation"), stringListToArray(unnecessaryValidation)},
        {QStringLiteral("predictedRisks"), stringListToArray(predictedRisks)},
        {QStringLiteral("actualFailures"), stringListToArray(actualFailures)},
        {QStringLiteral("anticipatedFailures"), stringListToArray(anticipatedFailures)},
        {QStringLiteral("unanticipatedFailures"), stringListToArray(unanticipatedFailures)},
        {QStringLiteral("evaluatedAt"), evaluatedAt}
    };
}

bool PredictionEvaluation::fromJson(const QJsonObject& obj, PredictionEvaluation* result, QString* error)
{
    if (!result) return false;
    PredictionEvaluation eval;
    eval.predictionId = obj.value(QStringLiteral("predictionId")).toString();
    eval.actualTaskId = obj.value(QStringLiteral("actualTaskId")).toString();

    eval.predictedFiles = arrayToStringList(obj.value(QStringLiteral("predictedFiles")).toArray());
    eval.actualModifiedFiles = arrayToStringList(obj.value(QStringLiteral("actualModifiedFiles")).toArray());
    eval.truePositiveFiles = arrayToStringList(obj.value(QStringLiteral("truePositiveFiles")).toArray());
    eval.falsePositiveFiles = arrayToStringList(obj.value(QStringLiteral("falsePositiveFiles")).toArray());
    eval.falseNegativeFiles = arrayToStringList(obj.value(QStringLiteral("falseNegativeFiles")).toArray());
    eval.filePrecision = obj.value(QStringLiteral("filePrecision")).toDouble();
    eval.fileRecall = obj.value(QStringLiteral("fileRecall")).toDouble();
    eval.changeBreadthComparison = obj.value(QStringLiteral("changeBreadthComparison")).toString();

    eval.predictedScopes = arrayToStringList(obj.value(QStringLiteral("predictedScopes")).toArray());
    eval.actualScopes = arrayToStringList(obj.value(QStringLiteral("actualScopes")).toArray());
    eval.matchedScopes = arrayToStringList(obj.value(QStringLiteral("matchedScopes")).toArray());
    eval.missedScopes = arrayToStringList(obj.value(QStringLiteral("missedScopes")).toArray());
    eval.unnecessaryScopes = arrayToStringList(obj.value(QStringLiteral("unnecessaryScopes")).toArray());
    eval.scopeMatch = obj.value(QStringLiteral("scopeMatch")).toBool();

    eval.predictedValidation = arrayToStringList(obj.value(QStringLiteral("predictedValidation")).toArray());
    eval.actualValidation = arrayToStringList(obj.value(QStringLiteral("actualValidation")).toArray());
    eval.matchedValidation = arrayToStringList(obj.value(QStringLiteral("matchedValidation")).toArray());
    eval.missedValidation = arrayToStringList(obj.value(QStringLiteral("missedValidation")).toArray());
    eval.unnecessaryValidation = arrayToStringList(obj.value(QStringLiteral("unnecessaryValidation")).toArray());

    eval.predictedRisks = arrayToStringList(obj.value(QStringLiteral("predictedRisks")).toArray());
    eval.actualFailures = arrayToStringList(obj.value(QStringLiteral("actualFailures")).toArray());
    eval.anticipatedFailures = arrayToStringList(obj.value(QStringLiteral("anticipatedFailures")).toArray());
    eval.unanticipatedFailures = arrayToStringList(obj.value(QStringLiteral("unanticipatedFailures")).toArray());

    eval.evaluatedAt = obj.value(QStringLiteral("evaluatedAt")).toString();
    *result = eval;
    return true;
}

QString PredictiveOptimizationService::predictionsDirectory(const QString& projectRoot)
{
    return QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/predictions"));
}

QString PredictiveOptimizationService::predictionsRegistryPath(const QString& projectRoot)
{
    return QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/predictions/prediction-registry.jsonl"));
}

QString PredictiveOptimizationService::evaluationsPath(const QString& projectRoot)
{
    return QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/predictions/evaluations.jsonl"));
}

PredictionContract PredictiveOptimizationService::predict(const ProjectModel& model,
                                                          const TaskSignature& signature,
                                                          const QJsonObject& callerProvenance)
{
    TaskSignature normalized = signature;
    normalized.normalize();

    PredictionContract contract;
    contract.predictionId = QStringLiteral("pred-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    contract.taskId = normalized.fingerprint();
    contract.taskSignature = normalized;
    contract.schemaVersion = QStringLiteral("1.0");
    contract.taskClassification = QJsonObject{
        {QStringLiteral("category"), normalized.taskCategory},
        {QStringLiteral("operationType"), normalized.operationType},
        {QStringLiteral("targetSubsystem"), normalized.targetSubsystem}
    };
    contract.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    contract.sourceProjectId = model.projectId();
    contract.sourceProjectPath = model.projectPath();
    contract.advisoryStatus = QStringLiteral("ADVISORY");

    // Provenance
    QJsonObject prov = callerProvenance;
    if (prov.isEmpty() || !prov.contains(QStringLiteral("actor"))) {
        prov = QJsonObject{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("antigravity")},
            {QStringLiteral("tool"), QStringLiteral("aramf-p3")}
        };
    }
    contract.provenance = prov;

    // Retrieve historical evidence
    ProjectMemory memory;
    QString memoryErr;
    const QList<QJsonObject> allEvents = memory.events(model.projectPath(), &memoryErr);

    // Group historical events by task name
    QMap<QString, HistoricalTaskCluster> taskClusters;
    for (const auto& ev : allEvents) {
        const QString taskName = ev.value(QStringLiteral("task")).toString().trimmed();
        if (taskName.isEmpty()) continue;

        auto& cluster = taskClusters[taskName];
        cluster.taskName = taskName;
        const QString eventId = ev.value(QStringLiteral("eventId")).toString();
        if (!eventId.isEmpty() && !cluster.eventIds.contains(eventId)) {
            cluster.eventIds.append(eventId);
        }
        const int seq = ev.value(QStringLiteral("sequenceNumber")).toInt();
        if (seq > 0) cluster.sequenceNumbers.append(seq);

        if (ev.contains(QStringLiteral("category")) && cluster.category.isEmpty()) {
            cluster.category = ev.value(QStringLiteral("category")).toString();
        }
        if (ev.contains(QStringLiteral("scope")) && cluster.scope.isEmpty()) {
            cluster.scope = ev.value(QStringLiteral("scope")).toString();
        }

        const QString eventType = ev.value(QStringLiteral("eventType")).toString();
        const QString status = ev.value(QStringLiteral("status")).toString();
        if (eventType == QStringLiteral("TASK_COMPLETED")) {
            cluster.completed = true;
            if (status == QStringLiteral("FAIL")) cluster.success = false;
        } else if (status == QStringLiteral("FAIL")) {
            cluster.success = false;
            cluster.failures.append(ev.value(QStringLiteral("detail")).toString());
        }

        if (eventType == QStringLiteral("TEST_RESULT")) {
            const QString suite = ev.value(QStringLiteral("suite")).toString();
            if (!suite.isEmpty() && !cluster.testSuites.contains(suite)) {
                cluster.testSuites.append(suite);
            }
        }

        const QString detail = ev.value(QStringLiteral("detail")).toString();
        const QString summary = ev.value(QStringLiteral("summary")).toString();
        for (const auto& text : {detail, summary}) {
            if (text.contains(QStringLiteral("src/")) || text.contains(QStringLiteral("tests/"))) {
                for (const auto& part : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
                    if ((part.startsWith(QStringLiteral("src/")) || part.startsWith(QStringLiteral("tests/")))
                        && !cluster.mentionedFiles.contains(part)) {
                        cluster.mentionedFiles.append(part);
                    }
                }
            }
        }
    }

    // Evaluate matching historical clusters
    QList<HistoricalTaskCluster> matchingClusters;
    QSet<QString> allMatchingEvidence;
    int successCount = 0;
    int failureCount = 0;
    double totalSim = 0.0;
    double totalFreshness = 0.0;

    for (auto it = taskClusters.begin(); it != taskClusters.end(); ++it) {
        HistoricalTaskCluster& cluster = it.value();
        TaskSignature histSig = TaskSignature::fromTaskAndCategory(
            cluster.taskName, cluster.category,
            cluster.scope.isEmpty() ? QStringList{} : QStringList{cluster.scope},
            cluster.mentionedFiles);

        const double sim = normalized.similarity(histSig);
        if (sim >= 0.30) {
            cluster.similarity = sim;

            // Freshness: events after legacy cutoff (311) are fully fresh
            bool hasPostCutoff = false;
            for (int s : cluster.sequenceNumbers) {
                if (s > 311) { hasPostCutoff = true; break; }
            }
            cluster.freshness = hasPostCutoff ? 1.0 : 0.70;

            matchingClusters.append(cluster);
            for (const auto& eid : cluster.eventIds) allMatchingEvidence.insert(eid);
            totalSim += sim;
            totalFreshness += cluster.freshness;
            if (cluster.success) ++successCount;
            else ++failureCount;
        }
    }

    contract.evidenceReferences = allMatchingEvidence.values();
    contract.evidenceReferences.sort();

    // 1. Predicted Scopes
    QMap<QString, PredictedItem> scopesMap;
    for (const auto& scope : normalized.relevantScopes) {
        scopesMap.insert(scope, PredictedItem{
            scope,
            QStringLiteral("KNOWN"),
            QStringLiteral("Explicitly declared in task signature"),
            {}
        });
    }

    QMap<QString, int> histScopeFreq;
    QMap<QString, QStringList> histScopeEvidence;
    for (const auto& mc : matchingClusters) {
        if (!mc.scope.isEmpty()) {
            histScopeFreq[mc.scope]++;
            for (const auto& eid : mc.eventIds) {
                if (!histScopeEvidence[mc.scope].contains(eid)) histScopeEvidence[mc.scope].append(eid);
            }
        }
    }

    for (auto it = histScopeFreq.begin(); it != histScopeFreq.end(); ++it) {
        const QString scope = it.key();
        if (!scopesMap.contains(scope)) {
            scopesMap.insert(scope, PredictedItem{
                scope,
                QStringLiteral("LIKELY"),
                QStringLiteral("Observed in %1 comparable historical tasks").arg(it.value()),
                histScopeEvidence.value(scope)
            });
        }
    }

    // Default scope if none found
    if (scopesMap.isEmpty()) {
        const QString defaultScope = normalized.targetSubsystem == QStringLiteral("memory") ? QStringLiteral("project") : QStringLiteral("project");
        scopesMap.insert(defaultScope, PredictedItem{
            defaultScope,
            QStringLiteral("LIKELY"),
            QStringLiteral("Standard canonical scope for project subsystem"),
            {}
        });
    }
    contract.predictedScopes = scopesMap.values();

    // 2. Predicted Files / Components
    QMap<QString, PredictedItem> filesMap;
    for (const auto& file : normalized.referencedFiles) {
        filesMap.insert(file, PredictedItem{
            file,
            QStringLiteral("KNOWN"),
            QStringLiteral("Explicitly referenced in task signature"),
            {}
        });
    }

    // Match subsystem files
    if (normalized.targetSubsystem == QStringLiteral("memory")) {
        const QStringList memFiles{
            QStringLiteral("src/core/ProjectMemory.h"),
            QStringLiteral("src/core/ProjectMemory.cpp"),
            QStringLiteral("src/core/MemoryCommand.cpp")
        };
        for (const auto& f : memFiles) {
            if (!filesMap.contains(f)) {
                filesMap.insert(f, PredictedItem{
                    f,
                    QStringLiteral("LIKELY"),
                    QStringLiteral("Canonical implementation file for memory subsystem"),
                    contract.evidenceReferences
                });
            }
        }
    } else if (normalized.targetSubsystem == QStringLiteral("orchestration")) {
        const QStringList orchFiles{
            QStringLiteral("src/core/ExecutionOrchestrator.h"),
            QStringLiteral("src/core/ExecutionOrchestrator.cpp")
        };
        for (const auto& f : orchFiles) {
            if (!filesMap.contains(f)) {
                filesMap.insert(f, PredictedItem{
                    f,
                    QStringLiteral("LIKELY"),
                    QStringLiteral("Canonical implementation file for orchestration subsystem"),
                    contract.evidenceReferences
                });
            }
        }
    } else if (normalized.targetSubsystem == QStringLiteral("worker")) {
        const QStringList workerFiles{
            QStringLiteral("src/core/WorkerTaskServices.h"),
            QStringLiteral("src/core/WorkerTaskServices.cpp"),
            QStringLiteral("src/core/RuntimeOwnershipService.cpp")
        };
        for (const auto& f : workerFiles) {
            if (!filesMap.contains(f)) {
                filesMap.insert(f, PredictedItem{
                    f,
                    QStringLiteral("LIKELY"),
                    QStringLiteral("Canonical implementation file for worker/governance subsystem"),
                    contract.evidenceReferences
                });
            }
        }
    } else if (normalized.targetSubsystem == QStringLiteral("context")) {
        const QStringList ctxFiles{
            QStringLiteral("src/core/ContextCoordinationService.h"),
            QStringLiteral("src/core/ContextCoordinationService.cpp")
        };
        for (const auto& f : ctxFiles) {
            if (!filesMap.contains(f)) {
                filesMap.insert(f, PredictedItem{
                    f,
                    QStringLiteral("LIKELY"),
                    QStringLiteral("Canonical implementation file for context coordination subsystem"),
                    contract.evidenceReferences
                });
            }
        }
    }

    // Historical file co-occurrence
    for (const auto& mc : matchingClusters) {
        for (const auto& f : mc.mentionedFiles) {
            if (!filesMap.contains(f)) {
                filesMap.insert(f, PredictedItem{
                    f,
                    QStringLiteral("LIKELY"),
                    QStringLiteral("Co-modified in historical task '%1'").arg(mc.taskName),
                    mc.eventIds
                });
            }
        }
    }
    contract.predictedFiles = filesMap.values();

    // 3. Predicted Validation Suites
    QMap<QString, PredictedItem> valMap;
    // Core test suite is always required for C++ core changes
    valMap.insert(QStringLiteral("aramf_core_tests"), PredictedItem{
        QStringLiteral("aramf_core_tests"),
        QStringLiteral("KNOWN"),
        QStringLiteral("Primary unit and regression test suite for core framework services"),
        {}
    });

    if (normalized.targetSubsystem == QStringLiteral("memory")) {
        valMap.insert(QStringLiteral("memory cold-start"), PredictedItem{
            QStringLiteral("memory cold-start"),
            QStringLiteral("LIKELY"),
            QStringLiteral("Memory subsystem modification requires cold-start verification"),
            contract.evidenceReferences
        });
        valMap.insert(QStringLiteral("memory validate"), PredictedItem{
            QStringLiteral("memory validate"),
            QStringLiteral("LIKELY"),
            QStringLiteral("Memory consistency validation required after memory modifications"),
            contract.evidenceReferences
        });
    }

    if (normalized.targetSubsystem == QStringLiteral("ui")) {
        valMap.insert(QStringLiteral("aramf_workflow_tests"), PredictedItem{
            QStringLiteral("aramf_workflow_tests"),
            QStringLiteral("LIKELY"),
            QStringLiteral("UI workflow navigation regression suite"),
            {}
        });
    }

    // Historical test suites observed
    for (const auto& mc : matchingClusters) {
        for (const auto& suite : mc.testSuites) {
            if (!valMap.contains(suite)) {
                valMap.insert(suite, PredictedItem{
                    suite,
                    QStringLiteral("LIKELY"),
                    QStringLiteral("Historically executed in comparable task '%1'").arg(mc.taskName),
                    mc.eventIds
                });
            }
        }
    }
    contract.predictedValidation = valMap.values();

    // 4. Predicted Risk Categories
    QMap<QString, PredictedItem> riskMap;
    if (normalized.governanceClass == QStringLiteral("destructive")) {
        riskMap.insert(QStringLiteral("DESTRUCTIVE_OPERATION"), PredictedItem{
            QStringLiteral("DESTRUCTIVE_OPERATION"),
            QStringLiteral("KNOWN"),
            QStringLiteral("Task requests destructive operations requiring explicit verification"),
            {}
        });
    }

    if (normalized.targetSubsystem == QStringLiteral("memory")) {
        riskMap.insert(QStringLiteral("EVIDENCE_INTEGRITY"), PredictedItem{
            QStringLiteral("EVIDENCE_INTEGRITY"),
            QStringLiteral("LIKELY"),
            QStringLiteral("Memory changes risk corrupting sequence numbers or append-only log"),
            contract.evidenceReferences
        });
    }

    if (normalized.targetSubsystem == QStringLiteral("orchestration") || contract.predictedFiles.size() > 3) {
        riskMap.insert(QStringLiteral("OWNERSHIP_COLLISION"), PredictedItem{
            QStringLiteral("OWNERSHIP_COLLISION"),
            QStringLiteral("LIKELY"),
            QStringLiteral("Broad file modifications increase runtime ownership contestation risk"),
            {}
        });
    }

    if (failureCount > 0) {
        riskMap.insert(QStringLiteral("REGRESSION_RISK"), PredictedItem{
            QStringLiteral("REGRESSION_RISK"),
            QStringLiteral("LIKELY"),
            QStringLiteral("%1 historical failure(s) observed across comparable tasks").arg(failureCount),
            contract.evidenceReferences
        });
    }
    contract.predictedRiskCategories = riskMap.values();

    // 5. Change Breadth
    const int fileCount = contract.predictedFiles.size();
    if (fileCount <= 2) {
        contract.predictedChangeBreadth = QStringLiteral("NARROW");
        contract.breadthRationale = QStringLiteral("Predicted modifications are isolated to %1 file(s).").arg(fileCount);
    } else if (fileCount <= 5) {
        contract.predictedChangeBreadth = QStringLiteral("MODERATE");
        contract.breadthRationale = QStringLiteral("Predicted modifications span %1 files across %2 scope(s).")
            .arg(fileCount).arg(contract.predictedScopes.size());
    } else {
        contract.predictedChangeBreadth = QStringLiteral("BROAD");
        contract.breadthRationale = QStringLiteral("Broad change predicted spanning %1 files across multiple components.")
            .arg(fileCount);
    }

    // 6. Explainable Confidence Calculation
    PredictionConfidence conf;
    conf.sampleSize = matchingClusters.size();
    if (conf.sampleSize == 0) {
        conf.score = 0.0;
        conf.rating = QStringLiteral("INSUFFICIENT_EVIDENCE");
        conf.explanation = QStringLiteral("No comparable historical evidence found for task signature; returning insufficient evidence.");
        contract.status = QStringLiteral("INSUFFICIENT_EVIDENCE");
    } else {
        conf.consistencyScore = static_cast<double>(successCount) / static_cast<double>(conf.sampleSize);
        conf.freshnessScore = totalFreshness / static_cast<double>(conf.sampleSize);
        conf.matchPrecision = totalSim / static_cast<double>(conf.sampleSize);

        // Conflicting evidence penalty
        if (failureCount > 0 && successCount > 0) {
            conf.conflictingEvidencePenalty = 0.25;
        } else {
            conf.conflictingEvidencePenalty = 0.0;
        }

        const double sampleFactor = qMin(1.0, static_cast<double>(conf.sampleSize) / 4.0);
        double rawScore = (conf.matchPrecision * 0.40)
                        + (conf.consistencyScore * 0.30)
                        + (conf.freshnessScore * 0.15)
                        + (sampleFactor * 0.15)
                        - conf.conflictingEvidencePenalty;

        conf.score = qBound(0.05, rawScore, 1.0);

        if (conf.sampleSize < 2 || conf.score < 0.35) {
            conf.rating = QStringLiteral("LOW");
        } else if (conf.score >= 0.75 && conf.sampleSize >= 3 && conf.conflictingEvidencePenalty == 0.0) {
            conf.rating = QStringLiteral("HIGH");
        } else {
            conf.rating = QStringLiteral("MEDIUM");
        }

        conf.explanation = QStringLiteral("Derived from %1 historical task(s) with %2% consistency, %3% match precision, and freshness factor %4.")
            .arg(conf.sampleSize)
            .arg(qRound(conf.consistencyScore * 100))
            .arg(qRound(conf.matchPrecision * 100))
            .arg(QString::number(conf.freshnessScore, 'f', 2));
        contract.status = QStringLiteral("READY");
    }

    contract.confidence = conf;
    return contract;
}

PredictionContract PredictiveOptimizationService::predictFromRequest(const ProjectModel& model,
                                                                     const WorkerTaskRequest& request,
                                                                     const QJsonObject& callerProvenance)
{
    const TaskSignature signature = TaskSignature::fromWorkerTaskRequest(request, &model);
    return predict(model, signature, callerProvenance);
}

PredictionEvaluation PredictiveOptimizationService::evaluate(const PredictionContract& prediction,
                                                             const QJsonObject& actualExecutionResult,
                                                             const WorkerTaskRequest* actualRequest)
{
    PredictionEvaluation eval;
    eval.predictionId = prediction.predictionId;
    eval.actualTaskId = actualExecutionResult.value(QStringLiteral("sourceTaskId")).toString();
    eval.evaluatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    // Extract predicted files
    for (const auto& item : prediction.predictedFiles) {
        if (!eval.predictedFiles.contains(item.item)) eval.predictedFiles.append(item.item);
    }

    // Extract actual modified files from execution result or contract
    const auto changed = actualExecutionResult.value(QStringLiteral("changedResources")).toArray();
    for (const auto& f : changed) {
        const QString file = f.toString().trimmed().replace('\\', '/');
        if (!file.isEmpty() && !eval.actualModifiedFiles.contains(file)) {
            eval.actualModifiedFiles.append(file);
        }
    }
    if (eval.actualModifiedFiles.isEmpty() && actualRequest) {
        eval.actualModifiedFiles = actualRequest->files;
    }

    // File metrics (TP, FP, FN, precision, recall)
    for (const auto& f : eval.predictedFiles) {
        if (eval.actualModifiedFiles.contains(f)) {
            eval.truePositiveFiles.append(f);
        } else {
            eval.falsePositiveFiles.append(f);
        }
    }
    for (const auto& f : eval.actualModifiedFiles) {
        if (!eval.predictedFiles.contains(f)) {
            eval.falseNegativeFiles.append(f);
        }
    }

    const int tp = eval.truePositiveFiles.size();
    const int fp = eval.falsePositiveFiles.size();
    const int fn = eval.falseNegativeFiles.size();

    if (tp + fp > 0) {
        eval.filePrecision = static_cast<double>(tp) / static_cast<double>(tp + fp);
    } else {
        eval.filePrecision = (eval.predictedFiles.isEmpty() && eval.actualModifiedFiles.isEmpty()) ? 1.0 : 0.0;
    }

    if (tp + fn > 0) {
        eval.fileRecall = static_cast<double>(tp) / static_cast<double>(tp + fn);
    } else {
        eval.fileRecall = (eval.predictedFiles.isEmpty() && eval.actualModifiedFiles.isEmpty()) ? 1.0 : 0.0;
    }

    const int actualCount = eval.actualModifiedFiles.size();
    if (prediction.predictedChangeBreadth == QStringLiteral("NARROW")) {
        eval.changeBreadthComparison = actualCount <= 2 ? QStringLiteral("ACCURATE") : QStringLiteral("UNDERPREDICTED");
    } else if (prediction.predictedChangeBreadth == QStringLiteral("MODERATE")) {
        eval.changeBreadthComparison = (actualCount >= 3 && actualCount <= 5) ? QStringLiteral("ACCURATE") :
            (actualCount < 3 ? QStringLiteral("OVERPREDICTED") : QStringLiteral("UNDERPREDICTED"));
    } else {
        eval.changeBreadthComparison = actualCount >= 6 ? QStringLiteral("ACCURATE") : QStringLiteral("OVERPREDICTED");
    }

    // Scopes metric
    for (const auto& item : prediction.predictedScopes) {
        if (!eval.predictedScopes.contains(item.item)) eval.predictedScopes.append(item.item);
    }
    if (actualRequest) {
        eval.actualScopes = actualRequest->scopes;
    }
    for (const auto& s : eval.predictedScopes) {
        if (eval.actualScopes.contains(s)) {
            eval.matchedScopes.append(s);
        } else {
            eval.unnecessaryScopes.append(s);
        }
    }
    for (const auto& s : eval.actualScopes) {
        if (!eval.predictedScopes.contains(s)) {
            eval.missedScopes.append(s);
        }
    }
    eval.scopeMatch = eval.missedScopes.isEmpty();

    // Validation metric
    for (const auto& item : prediction.predictedValidation) {
        if (!eval.predictedValidation.contains(item.item)) eval.predictedValidation.append(item.item);
    }
    const auto evidence = actualExecutionResult.value(QStringLiteral("validationEvidence")).toArray();
    for (const auto& val : evidence) {
        const QString suite = val.toObject().value(QStringLiteral("suite")).toString();
        if (!suite.isEmpty() && !eval.actualValidation.contains(suite)) {
            eval.actualValidation.append(suite);
        }
    }
    for (const auto& v : eval.predictedValidation) {
        if (eval.actualValidation.contains(v)) {
            eval.matchedValidation.append(v);
        } else {
            eval.unnecessaryValidation.append(v);
        }
    }
    for (const auto& v : eval.actualValidation) {
        if (!eval.predictedValidation.contains(v)) {
            eval.missedValidation.append(v);
        }
    }

    // Risk metric
    for (const auto& item : prediction.predictedRiskCategories) {
        eval.predictedRisks.append(item.item);
    }
    const QString completionState = actualExecutionResult.value(QStringLiteral("completionState")).toString();
    if (completionState == QStringLiteral("FAILED")) {
        const QString failure = actualExecutionResult.value(QStringLiteral("failure")).toString();
        if (!failure.isEmpty()) eval.actualFailures.append(failure);
    }
    for (const auto& fail : eval.actualFailures) {
        if (eval.predictedRisks.contains(fail)) {
            eval.anticipatedFailures.append(fail);
        } else {
            eval.unanticipatedFailures.append(fail);
        }
    }

    return eval;
}

bool PredictiveOptimizationService::savePrediction(const QString& projectRoot,
                                                   const PredictionContract& prediction,
                                                   QString* error)
{
    const QString dirPath = predictionsDirectory(projectRoot);
    if (!QDir().mkpath(dirPath)) {
        if (error) *error = QStringLiteral("Failed to create predictions directory: ") + dirPath;
        return false;
    }

    // Save individual file
    const QString filePath = QDir(dirPath).filePath(prediction.predictionId + QStringLiteral(".json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("Failed to write prediction file: ") + filePath;
        return false;
    }
    file.write(QJsonDocument(prediction.toJson()).toJson(QJsonDocument::Indented));
    file.close();

    // Append to registry
    const QString registryPath = predictionsRegistryPath(projectRoot);
    QFile registry(registryPath);
    if (registry.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        registry.write(QJsonDocument(prediction.toJson()).toJson(QJsonDocument::Compact));
        registry.write("\n");
        registry.close();
    }

    return true;
}

bool PredictiveOptimizationService::loadPrediction(const QString& projectRoot,
                                                   const QString& predictionId,
                                                   PredictionContract* prediction,
                                                   QString* error)
{
    if (!prediction) return false;
    const QString filePath = QDir(predictionsDirectory(projectRoot)).filePath(predictionId + QStringLiteral(".json"));
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Prediction file not found: ") + filePath;
        return false;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return PredictionContract::fromJson(doc.object(), prediction, error);
}

bool PredictiveOptimizationService::saveEvaluation(const QString& projectRoot,
                                                   const PredictionEvaluation& evaluation,
                                                   QString* error)
{
    const QString dirPath = predictionsDirectory(projectRoot);
    if (!QDir().mkpath(dirPath)) {
        if (error) *error = QStringLiteral("Failed to create predictions directory: ") + dirPath;
        return false;
    }
    const QString evalPath = evaluationsPath(projectRoot);
    QFile file(evalPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Failed to write evaluation file: ") + evalPath;
        return false;
    }
    file.write(QJsonDocument(evaluation.toJson()).toJson(QJsonDocument::Compact));
    file.write("\n");
    file.close();
    return true;
}

QList<PredictionContract> PredictiveOptimizationService::listPredictions(const QString& projectRoot,
                                                                         QString* error)
{
    QList<PredictionContract> results;
    const QString registryPath = predictionsRegistryPath(projectRoot);
    QFile file(registryPath);
    if (!file.exists()) return results;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Failed to read predictions registry.");
        return results;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) continue;
        const auto doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            PredictionContract contract;
            if (PredictionContract::fromJson(doc.object(), &contract)) {
                results.append(contract);
            }
        }
    }
    file.close();
    return results;
}
