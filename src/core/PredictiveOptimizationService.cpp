#include "PredictiveOptimizationService.h"
#include "AramfPaths.h"
#include "FrameworkKnowledge.h"
#include "ProjectMemory.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QUuid>
#include <algorithm>

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

QString evidenceSourceTypeToString(EvidenceSourceType type)
{
    switch (type) {
    case EvidenceSourceType::LocalOperationalEvent:
        return QStringLiteral("LOCAL_OPERATIONAL_EVENT");
    case EvidenceSourceType::ApprovedGlobalKnowledge:
        return QStringLiteral("APPROVED_GLOBAL_KNOWLEDGE");
    case EvidenceSourceType::ScopePolicy:
        return QStringLiteral("SCOPE_POLICY");
    case EvidenceSourceType::ContractHistory:
        return QStringLiteral("CONTRACT_HISTORY");
    }
    return QStringLiteral("LOCAL_OPERATIONAL_EVENT");
}

EvidenceSourceType stringToEvidenceSourceType(const QString& str)
{
    if (str == QStringLiteral("APPROVED_GLOBAL_KNOWLEDGE"))
        return EvidenceSourceType::ApprovedGlobalKnowledge;
    if (str == QStringLiteral("SCOPE_POLICY"))
        return EvidenceSourceType::ScopePolicy;
    if (str == QStringLiteral("CONTRACT_HISTORY"))
        return EvidenceSourceType::ContractHistory;
    return EvidenceSourceType::LocalOperationalEvent;
}

QJsonObject RankedEvidence::toJson() const
{
    return QJsonObject{
        {QStringLiteral("evidenceId"), evidenceId},
        {QStringLiteral("sourceType"), evidenceSourceTypeToString(sourceType)},
        {QStringLiteral("originProjectId"), originProjectId},
        {QStringLiteral("relevance"), relevance},
        {QStringLiteral("freshness"), freshness},
        {QStringLiteral("outcomeWeight"), outcomeWeight},
        {QStringLiteral("sourceMultiplier"), sourceMultiplier},
        {QStringLiteral("totalRank"), totalRank},
        {QStringLiteral("summary"), summary},
        {QStringLiteral("rawPayload"), rawPayload}
    };
}

RankedEvidence RankedEvidence::fromJson(const QJsonObject& obj)
{
    RankedEvidence ev;
    ev.evidenceId = obj.value(QStringLiteral("evidenceId")).toString();
    ev.sourceType = stringToEvidenceSourceType(obj.value(QStringLiteral("sourceType")).toString());
    ev.originProjectId = obj.value(QStringLiteral("originProjectId")).toString();
    ev.relevance = obj.value(QStringLiteral("relevance")).toDouble(0.0);
    ev.freshness = obj.value(QStringLiteral("freshness")).toDouble(1.0);
    ev.outcomeWeight = obj.value(QStringLiteral("outcomeWeight")).toDouble(1.0);
    ev.sourceMultiplier = obj.value(QStringLiteral("sourceMultiplier")).toDouble(1.0);
    ev.totalRank = obj.value(QStringLiteral("totalRank")).toDouble(0.0);
    ev.summary = obj.value(QStringLiteral("summary")).toString();
    ev.rawPayload = obj.value(QStringLiteral("rawPayload")).toObject();
    return ev;
}

QJsonObject PredictedItem::toJson() const
{
    return QJsonObject{
        {QStringLiteral("item"), item},
        {QStringLiteral("status"), status},
        {QStringLiteral("rationale"), rationale},
        {QStringLiteral("evidenceReferences"), stringListToArray(evidenceReferences)},
        {QStringLiteral("primaryEvidenceId"), primaryEvidenceId},
        {QStringLiteral("contributionScore"), contributionScore},
        {QStringLiteral("sourceTypes"), stringListToArray(sourceTypes)}
    };
}

PredictedItem PredictedItem::fromJson(const QJsonObject& obj)
{
    PredictedItem result;
    result.item = obj.value(QStringLiteral("item")).toString();
    result.status = obj.value(QStringLiteral("status")).toString();
    result.rationale = obj.value(QStringLiteral("rationale")).toString();
    result.evidenceReferences = arrayToStringList(obj.value(QStringLiteral("evidenceReferences")).toArray());
    result.primaryEvidenceId = obj.value(QStringLiteral("primaryEvidenceId")).toString();
    result.contributionScore = obj.value(QStringLiteral("contributionScore")).toDouble(0.0);
    result.sourceTypes = arrayToStringList(obj.value(QStringLiteral("sourceTypes")).toArray());
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
    QJsonArray rankedArr;
    for (const auto& re : rankedEvidence) {
        rankedArr.append(re.toJson());
    }

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
        {QStringLiteral("rankedEvidence"), rankedArr},
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
    if (obj.isEmpty()) {
        if (error) *error = QStringLiteral("Empty PredictionContract JSON object.");
        return false;
    }

    result->predictionId = obj.value(QStringLiteral("predictionId")).toString();
    result->taskId = obj.value(QStringLiteral("taskId")).toString();
    result->taskSignature = TaskSignature::fromJson(obj.value(QStringLiteral("taskSignature")).toObject());
    result->schemaVersion = obj.value(QStringLiteral("schemaVersion")).toString(QStringLiteral("1.0"));
    result->taskClassification = obj.value(QStringLiteral("taskClassification")).toObject();
    result->predictedScopes = arrayToPredictedItems(obj.value(QStringLiteral("predictedScopes")).toArray());
    result->predictedFiles = arrayToPredictedItems(obj.value(QStringLiteral("predictedFiles")).toArray());
    result->predictedValidation = arrayToPredictedItems(obj.value(QStringLiteral("predictedValidation")).toArray());
    result->predictedRiskCategories = arrayToPredictedItems(obj.value(QStringLiteral("predictedRiskCategories")).toArray());
    result->predictedChangeBreadth = obj.value(QStringLiteral("predictedChangeBreadth")).toString();
    result->breadthRationale = obj.value(QStringLiteral("breadthRationale")).toString();
    result->confidence = PredictionConfidence::fromJson(obj.value(QStringLiteral("confidence")).toObject());
    result->evidenceReferences = arrayToStringList(obj.value(QStringLiteral("evidenceReferences")).toArray());

    result->rankedEvidence.clear();
    const auto rankedArr = obj.value(QStringLiteral("rankedEvidence")).toArray();
    for (const auto& v : rankedArr) {
        result->rankedEvidence.append(RankedEvidence::fromJson(v.toObject()));
    }

    result->provenance = obj.value(QStringLiteral("provenance")).toObject();
    result->createdAt = obj.value(QStringLiteral("createdAt")).toString();
    result->sourceProjectId = obj.value(QStringLiteral("sourceProjectId")).toString();
    result->sourceProjectPath = obj.value(QStringLiteral("sourceProjectPath")).toString();
    result->advisoryStatus = obj.value(QStringLiteral("advisoryStatus")).toString(QStringLiteral("ADVISORY"));
    result->status = obj.value(QStringLiteral("status")).toString(QStringLiteral("READY"));

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
    if (obj.isEmpty()) {
        if (error) *error = QStringLiteral("Empty PredictionEvaluation JSON object.");
        return false;
    }

    result->predictionId = obj.value(QStringLiteral("predictionId")).toString();
    result->actualTaskId = obj.value(QStringLiteral("actualTaskId")).toString();
    result->predictedFiles = arrayToStringList(obj.value(QStringLiteral("predictedFiles")).toArray());
    result->actualModifiedFiles = arrayToStringList(obj.value(QStringLiteral("actualModifiedFiles")).toArray());
    result->truePositiveFiles = arrayToStringList(obj.value(QStringLiteral("truePositiveFiles")).toArray());
    result->falsePositiveFiles = arrayToStringList(obj.value(QStringLiteral("falsePositiveFiles")).toArray());
    result->falseNegativeFiles = arrayToStringList(obj.value(QStringLiteral("falseNegativeFiles")).toArray());
    result->filePrecision = obj.value(QStringLiteral("filePrecision")).toDouble(0.0);
    result->fileRecall = obj.value(QStringLiteral("fileRecall")).toDouble(0.0);
    result->changeBreadthComparison = obj.value(QStringLiteral("changeBreadthComparison")).toString();
    result->predictedScopes = arrayToStringList(obj.value(QStringLiteral("predictedScopes")).toArray());
    result->actualScopes = arrayToStringList(obj.value(QStringLiteral("actualScopes")).toArray());
    result->matchedScopes = arrayToStringList(obj.value(QStringLiteral("matchedScopes")).toArray());
    result->missedScopes = arrayToStringList(obj.value(QStringLiteral("missedScopes")).toArray());
    result->unnecessaryScopes = arrayToStringList(obj.value(QStringLiteral("unnecessaryScopes")).toArray());
    result->scopeMatch = obj.value(QStringLiteral("scopeMatch")).toBool(false);
    result->predictedValidation = arrayToStringList(obj.value(QStringLiteral("predictedValidation")).toArray());
    result->actualValidation = arrayToStringList(obj.value(QStringLiteral("actualValidation")).toArray());
    result->matchedValidation = arrayToStringList(obj.value(QStringLiteral("matchedValidation")).toArray());
    result->missedValidation = arrayToStringList(obj.value(QStringLiteral("missedValidation")).toArray());
    result->unnecessaryValidation = arrayToStringList(obj.value(QStringLiteral("unnecessaryValidation")).toArray());
    result->predictedRisks = arrayToStringList(obj.value(QStringLiteral("predictedRisks")).toArray());
    result->actualFailures = arrayToStringList(obj.value(QStringLiteral("actualFailures")).toArray());
    result->anticipatedFailures = arrayToStringList(obj.value(QStringLiteral("anticipatedFailures")).toArray());
    result->unanticipatedFailures = arrayToStringList(obj.value(QStringLiteral("unanticipatedFailures")).toArray());
    result->evaluatedAt = obj.value(QStringLiteral("evaluatedAt")).toString();

    return true;
}

QJsonObject PredictionDriftReport::toJson() const
{
    return QJsonObject{
        {QStringLiteral("status"), status},
        {QStringLiteral("driftDetected"), driftDetected},
        {QStringLiteral("rollingPrecision"), rollingPrecision},
        {QStringLiteral("rollingRecall"), rollingRecall},
        {QStringLiteral("missedValidationCount"), missedValidationCount},
        {QStringLiteral("overconfidenceCount"), overconfidenceCount},
        {QStringLiteral("evaluationCount"), evaluationCount},
        {QStringLiteral("alerts"), stringListToArray(alerts)},
        {QStringLiteral("evaluatedAt"), evaluatedAt}
    };
}

bool PredictionDriftReport::fromJson(const QJsonObject& obj, PredictionDriftReport* result, QString* error)
{
    if (!result) return false;
    result->status = obj.value(QStringLiteral("status")).toString(QStringLiteral("STABLE"));
    result->driftDetected = obj.value(QStringLiteral("driftDetected")).toBool(false);
    result->rollingPrecision = obj.value(QStringLiteral("rollingPrecision")).toDouble(1.0);
    result->rollingRecall = obj.value(QStringLiteral("rollingRecall")).toDouble(1.0);
    result->missedValidationCount = obj.value(QStringLiteral("missedValidationCount")).toInt(0);
    result->overconfidenceCount = obj.value(QStringLiteral("overconfidenceCount")).toInt(0);
    result->evaluationCount = obj.value(QStringLiteral("evaluationCount")).toInt(0);
    result->alerts = arrayToStringList(obj.value(QStringLiteral("alerts")).toArray());
    result->evaluatedAt = obj.value(QStringLiteral("evaluatedAt")).toString();
    return true;
}

PredictionDriftReport PredictionDriftDetector::evaluateDrift(const QList<PredictionEvaluation>& evaluations,
                                                             int windowSize)
{
    PredictionDriftReport report;
    report.evaluatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    if (evaluations.isEmpty()) {
        report.status = QStringLiteral("STABLE");
        report.driftDetected = false;
        report.rollingPrecision = 1.0;
        report.rollingRecall = 1.0;
        report.evaluationCount = 0;
        return report;
    }

    const int startIdx = qMax(0, evaluations.size() - windowSize);
    const int count = evaluations.size() - startIdx;
    report.evaluationCount = count;

    double totalPrec = 0.0;
    double totalRec = 0.0;
    int missedVal = 0;
    int overconfident = 0;

    for (int i = startIdx; i < evaluations.size(); ++i) {
        const auto& ev = evaluations.at(i);
        totalPrec += ev.filePrecision;
        totalRec += ev.fileRecall;
        if (!ev.missedValidation.isEmpty()) {
            missedVal++;
        }
        if (ev.filePrecision < 0.50 || ev.fileRecall < 0.50) {
            overconfident++;
        }
    }

    report.rollingPrecision = totalPrec / static_cast<double>(count);
    report.rollingRecall = totalRec / static_cast<double>(count);
    report.missedValidationCount = missedVal;
    report.overconfidenceCount = overconfident;

    if (missedVal > 0) {
        report.driftDetected = true;
        report.status = QStringLiteral("DRIFT_CONFIRMED");
        report.alerts.append(QStringLiteral("CRITICAL: Missed required validation suites detected in %1 evaluation(s).")
            .arg(missedVal));
    }
    if (report.rollingPrecision < 0.60 || report.rollingRecall < 0.60) {
        report.driftDetected = true;
        if (report.status != QStringLiteral("DRIFT_CONFIRMED")) {
            report.status = QStringLiteral("DRIFT_SUSPECTED");
        }
        report.alerts.append(QStringLiteral("Degraded prediction accuracy: precision=%1, recall=%2")
            .arg(QString::number(report.rollingPrecision, 'f', 2))
            .arg(QString::number(report.rollingRecall, 'f', 2)));
    }
    if (overconfident > 0) {
        report.alerts.append(QStringLiteral("Suboptimal prediction accuracy detected in %1 evaluation(s).")
            .arg(overconfident));
    }

    if (!report.driftDetected) {
        report.status = QStringLiteral("STABLE");
    }

    return report;
}

QString PredictionDriftDetector::driftReportPath(const QString& projectRoot)
{
    return QDir(PredictiveOptimizationService::predictionsDirectory(projectRoot)).filePath(QStringLiteral("drift-report.json"));
}

bool PredictionDriftDetector::saveDriftReport(const QString& projectRoot,
                                              const PredictionDriftReport& report,
                                              QString* error)
{
    const QString dirPath = PredictiveOptimizationService::predictionsDirectory(projectRoot);
    if (!QDir().mkpath(dirPath)) {
        if (error) *error = QStringLiteral("Failed to create predictions directory: ") + dirPath;
        return false;
    }
    const QString path = driftReportPath(projectRoot);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("Failed to write drift report: ") + path;
        return false;
    }
    file.write(QJsonDocument(report.toJson()).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool PredictionDriftDetector::loadDriftReport(const QString& projectRoot,
                                              PredictionDriftReport* report,
                                              QString* error)
{
    if (!report) return false;
    const QString path = driftReportPath(projectRoot);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Drift report not found: ") + path;
        return false;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return PredictionDriftReport::fromJson(doc.object(), report, error);
}

double PredictiveOptimizationService::calculateEvidenceRank(EvidenceSourceType type,
                                                            double relevance,
                                                            double freshness,
                                                            double outcomeWeight)
{
    double sourceMultiplier = 1.0;
    switch (type) {
    case EvidenceSourceType::LocalOperationalEvent:
        sourceMultiplier = 1.0;
        break;
    case EvidenceSourceType::ApprovedGlobalKnowledge:
        sourceMultiplier = 0.85;
        break;
    case EvidenceSourceType::ScopePolicy:
        sourceMultiplier = 0.70;
        break;
    case EvidenceSourceType::ContractHistory:
        sourceMultiplier = 0.90;
        break;
    }
    const double raw = sourceMultiplier * (0.50 * relevance + 0.25 * freshness + 0.25 * outcomeWeight);
    return qBound(0.0, raw, 1.0);
}

QStringList PredictiveOptimizationService::canonicalRiskCategories()
{
    return QStringList{
        QStringLiteral("OWNERSHIP_CONFLICT"),
        QStringLiteral("SCOPE_EXPANSION"),
        QStringLiteral("STALE_CONTEXT"),
        QStringLiteral("STALE_VALIDATION"),
        QStringLiteral("RECORDER_CONSISTENCY"),
        QStringLiteral("PROVENANCE_MISSING"),
        QStringLiteral("PERSISTENCE_FAULT"),
        QStringLiteral("CROSS_LAYER_REGRESSION"),
        QStringLiteral("DEPENDENCY_REGRESSION"),
        QStringLiteral("RECOVERY_RETRY"),
        QStringLiteral("DESTRUCTIVE_OPERATION")
    };
}

QString PredictiveOptimizationService::determineChangeBreadth(const QStringList& predictedFiles,
                                                              const TaskSignature& signature)
{
    Q_UNUSED(signature);
    if (predictedFiles.isEmpty()) {
        return QStringLiteral("UNKNOWN");
    }

    bool touchesProjectWide = false;
    QSet<QString> topDirs;
    for (const auto& raw : predictedFiles) {
        const QString f = QString(raw).replace('\\', '/');
        if (f == QStringLiteral("CMakeLists.txt") ||
            f.endsWith(QStringLiteral("/CMakeLists.txt")) ||
            f.contains(QStringLiteral("AGENTS.md")) ||
            f.contains(QStringLiteral("PROJECT_STATUS.md")) ||
            f.contains(QStringLiteral("generated-rules.md")) ||
            f.startsWith(QStringLiteral("rules/"))) {
            touchesProjectWide = true;
        }
        const int slashIdx = f.indexOf(QLatin1Char('/'));
        if (slashIdx > 0) {
            const int secondSlash = f.indexOf(QLatin1Char('/'), slashIdx + 1);
            if (secondSlash > 0) {
                topDirs.insert(f.left(secondSlash));
            } else {
                topDirs.insert(f.left(slashIdx));
            }
        } else {
            topDirs.insert(QStringLiteral("root"));
        }
    }

    if (touchesProjectWide || topDirs.size() >= 4) {
        return QStringLiteral("PROJECT_WIDE");
    }
    if (topDirs.size() >= 3) {
        return QStringLiteral("CROSS_LAYER");
    }
    if (topDirs.size() == 2) {
        return QStringLiteral("MULTI_COMPONENT");
    }
    if (predictedFiles.size() == 1) {
        return QStringLiteral("LOCAL");
    }
    return QStringLiteral("COMPONENT");
}

QString PredictiveOptimizationService::determineValidationLevel(const QString& breadth,
                                                                const QList<PredictedItem>& risks,
                                                                double confidenceScore)
{
    bool hasCriticalRisk = false;
    for (const auto& r : risks) {
        if (r.item == QStringLiteral("CROSS_LAYER_REGRESSION") ||
            r.item == QStringLiteral("DESTRUCTIVE_OPERATION") ||
            r.item == QStringLiteral("OWNERSHIP_CONFLICT")) {
            hasCriticalRisk = true;
            break;
        }
    }

    if (breadth == QStringLiteral("PROJECT_WIDE") || hasCriticalRisk) {
        return QStringLiteral("FULL_REGRESSION");
    }
    if (breadth == QStringLiteral("CROSS_LAYER") || breadth == QStringLiteral("MULTI_COMPONENT") || !risks.isEmpty()) {
        return QStringLiteral("SUBSYSTEM");
    }
    if ((breadth == QStringLiteral("LOCAL") || breadth == QStringLiteral("COMPONENT")) &&
        confidenceScore >= 0.70 && risks.isEmpty()) {
        return QStringLiteral("FOCUSED");
    }
    return QStringLiteral("SUBSYSTEM");
}

PredictionContract PredictiveOptimizationService::predict(const ProjectModel& model,
                                                          const TaskSignature& signature,
                                                          const QJsonObject& callerProvenance)
{
    TaskSignature normalized = signature;
    normalized.normalize();

    PredictionContract contract;
    contract.predictionId = QStringLiteral("pred-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    contract.taskId = QStringLiteral("task-predicted-%1").arg(normalized.fingerprint().left(12));
    contract.taskSignature = normalized;
    contract.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    contract.sourceProjectId = model.projectId().isEmpty() ? QStringLiteral("ARAMF") : model.projectId();
    contract.sourceProjectPath = model.projectPath();
    contract.advisoryStatus = QStringLiteral("ADVISORY");

    // Provenance
    if (!callerProvenance.isEmpty()) {
        contract.provenance = callerProvenance;
    } else {
        contract.provenance = QJsonObject{
            {QStringLiteral("actor"), QStringLiteral("agent")},
            {QStringLiteral("agentId"), QStringLiteral("antigravity")},
            {QStringLiteral("tool"), QStringLiteral("aramf-cli")},
            {QStringLiteral("scope"), QStringLiteral("project")}
        };
    }

    // Task Classification
    contract.taskClassification = QJsonObject{
        {QStringLiteral("category"), normalized.taskCategory},
        {QStringLiteral("operation"), normalized.operationType},
        {QStringLiteral("targetSubsystem"), normalized.targetSubsystem},
        {QStringLiteral("governanceClass"), normalized.governanceClass},
        {QStringLiteral("resourceOwnershipClass"), normalized.resourceOwnershipClass}
    };

    // Query historical evidence from ProjectMemory API instead of raw file bypass
    const QString projectRoot = model.projectPath().isEmpty() ? AramfPaths::programRoot() : model.projectPath();
    ProjectMemory memory;
    const auto events = memory.events(projectRoot);

    // Read legacy cutoff from memory manifest
    const QString manifestPath = QDir(projectRoot).filePath(AramfPaths::resolveWorkerRelativePath(AramfPaths::Manifest));
    int legacyCutoff = 0;
    QFile manifestFile(manifestPath);
    if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const auto mDoc = QJsonDocument::fromJson(manifestFile.readAll());
        if (mDoc.isObject()) {
            legacyCutoff = mDoc.object().value(QStringLiteral("legacyProvenanceCutoffSequence")).toInt(0);
        }
    }

    QMap<QString, HistoricalTaskCluster> taskClusters;
    for (const auto& ev : events) {
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
        bool sharesFiles = false;
        for (const auto& rf : normalized.referencedFiles) {
            if (histSig.referencedFiles.contains(rf)) { sharesFiles = true; break; }
        }
        const bool domainMatch = (normalized.taskCategory == histSig.taskCategory) ||
                                 (normalized.targetSubsystem == histSig.targetSubsystem) ||
                                 sharesFiles;

        if (sim >= 0.30 && domainMatch) {
            cluster.similarity = sim;

            bool hasPostCutoff = false;
            for (int s : cluster.sequenceNumbers) {
                if (legacyCutoff > 0 && s > legacyCutoff) { hasPostCutoff = true; break; }
            }
            cluster.freshness = (legacyCutoff == 0 || hasPostCutoff) ? 1.0 : 0.70;

            matchingClusters.append(cluster);
            for (const auto& eid : cluster.eventIds) allMatchingEvidence.insert(eid);
            totalSim += sim;
            totalFreshness += cluster.freshness;
            if (cluster.success) ++successCount;
            else ++failureCount;

            // Build RankedEvidence for local operational event
            RankedEvidence re;
            re.evidenceId = cluster.eventIds.isEmpty() ? QStringLiteral("hist-%1").arg(cluster.sequenceNumbers.value(0, 0)) : cluster.eventIds.first();
            re.sourceType = EvidenceSourceType::LocalOperationalEvent;
            re.originProjectId = model.projectId().isEmpty() ? QStringLiteral("local") : model.projectId();
            re.relevance = cluster.similarity;
            re.freshness = cluster.freshness;
            re.outcomeWeight = cluster.success ? 1.0 : 0.50;
            re.sourceMultiplier = 1.0;
            re.totalRank = calculateEvidenceRank(re.sourceType, re.relevance, re.freshness, re.outcomeWeight);
            re.summary = QStringLiteral("Local task '%1' (similarity: %2, success: %3)")
                .arg(cluster.taskName).arg(cluster.similarity, 0, 'f', 2).arg(cluster.success ? "YES" : "NO");
            contract.rankedEvidence.append(re);
        }
    }

    // Multi-Project Evidence Integration via Approved Framework Knowledge (P3.1.2)
    FrameworkKnowledgeService fkService;
    QString fkErr;
    const auto projectApproved = fkService.approvedEntries(projectRoot, {}, &fkErr);
    const auto globalApproved = fkService.approvedGlobalEntries({}, &fkErr);

    QList<FrameworkKnowledgeEntry> allApprovedKnowledge = projectApproved;
    for (const auto& ge : globalApproved) {
        bool dup = false;
        for (const auto& pe : allApprovedKnowledge) {
            if (pe.id == ge.id) { dup = true; break; }
        }
        if (!dup) allApprovedKnowledge.append(ge);
    }

    // Strict Framework Knowledge review & isolation check:
    // Only approved entries (status == "approved" && reviewStatus == "approved") are permitted.
    // Candidate or unapproved entries are strictly ignored.
    for (const auto& entry : allApprovedKnowledge) {
        if (entry.status != QStringLiteral("approved") || entry.reviewStatus != QStringLiteral("approved")) {
            continue;
        }

        // Calculate domain relevance
        double relevance = 0.40;
        bool scopeMatch = false;
        for (const auto& sc : entry.scopes) {
            if (normalized.relevantScopes.contains(sc, Qt::CaseInsensitive) ||
                sc.compare(normalized.taskCategory, Qt::CaseInsensitive) == 0 ||
                sc.compare(normalized.targetSubsystem, Qt::CaseInsensitive) == 0) {
                scopeMatch = true;
                break;
            }
        }
        if (scopeMatch) relevance = 0.75;

        // Check if title or lesson matches
        if (entry.title.contains(normalized.taskCategory, Qt::CaseInsensitive) ||
            entry.lesson.contains(normalized.targetSubsystem, Qt::CaseInsensitive)) {
            relevance = qMax(relevance, 0.65);
        }

        if (relevance >= 0.40) {
            RankedEvidence re;
            re.evidenceId = entry.id;
            re.sourceType = EvidenceSourceType::ApprovedGlobalKnowledge;
            re.originProjectId = entry.originProjectId.isEmpty() ? QStringLiteral("global") : entry.originProjectId;
            re.relevance = relevance;
            re.freshness = 1.0;
            re.outcomeWeight = 1.0;
            re.sourceMultiplier = 0.85;
            re.totalRank = calculateEvidenceRank(re.sourceType, re.relevance, re.freshness, re.outcomeWeight);
            re.summary = QStringLiteral("%1: %2").arg(entry.title, entry.lesson);
            re.rawPayload = QJsonObject{
                {QStringLiteral("title"), entry.title},
                {QStringLiteral("lesson"), entry.lesson},
                {QStringLiteral("scopes"), stringListToArray(entry.scopes)}
            };
            contract.rankedEvidence.append(re);
            allMatchingEvidence.insert(entry.id);
        }
    }

    // Policy Evidence
    RankedEvidence policyEv;
    policyEv.evidenceId = QStringLiteral("policy:proportional-validation-routing");
    policyEv.sourceType = EvidenceSourceType::ScopePolicy;
    policyEv.originProjectId = QStringLiteral("ARAMF");
    policyEv.relevance = 0.80;
    policyEv.freshness = 1.0;
    policyEv.outcomeWeight = 1.0;
    policyEv.sourceMultiplier = 0.70;
    policyEv.totalRank = calculateEvidenceRank(policyEv.sourceType, policyEv.relevance, policyEv.freshness, policyEv.outcomeWeight);
    policyEv.summary = QStringLiteral("Proportional validation policy routing based on scope and risk");
    contract.rankedEvidence.append(policyEv);
    allMatchingEvidence.insert(policyEv.evidenceId);

    // Sort ranked evidence deterministically: totalRank DESC, freshness DESC, evidenceId ASC
    std::sort(contract.rankedEvidence.begin(), contract.rankedEvidence.end(), [](const RankedEvidence& a, const RankedEvidence& b) {
        if (qAbs(a.totalRank - b.totalRank) > 1e-6) {
            return a.totalRank > b.totalRank;
        }
        if (qAbs(a.freshness - b.freshness) > 1e-6) {
            return a.freshness > b.freshness;
        }
        return a.evidenceId < b.evidenceId;
    });

    contract.evidenceReferences = allMatchingEvidence.values();
    contract.evidenceReferences.sort();

    // Helper lambda to find top evidence and score for an item
    auto findEvidenceDecomposition = [&](const QString& searchKey) -> std::tuple<QString, double, QStringList> {
        QString topId;
        double sumScore = 0.0;
        QSet<QString> sources;

        for (const auto& re : contract.rankedEvidence) {
            bool matches = false;
            if (re.summary.contains(searchKey, Qt::CaseInsensitive) ||
                re.evidenceId.contains(searchKey, Qt::CaseInsensitive)) {
                matches = true;
            }
            if (matches) {
                if (topId.isEmpty()) topId = re.evidenceId;
                sumScore += re.totalRank;
                sources.insert(evidenceSourceTypeToString(re.sourceType));
            }
        }

        if (topId.isEmpty() && !contract.rankedEvidence.isEmpty()) {
            topId = contract.rankedEvidence.first().evidenceId;
            sumScore = contract.rankedEvidence.first().totalRank;
            sources.insert(evidenceSourceTypeToString(contract.rankedEvidence.first().sourceType));
        }

        QStringList srcList = sources.values();
        srcList.sort();
        return {topId, sumScore, srcList};
    };

    // 1. Predicted Scopes
    QMap<QString, PredictedItem> scopesMap;
    for (const auto& scope : normalized.relevantScopes) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(scope);
        PredictedItem item;
        item.item = scope;
        item.status = QStringLiteral("KNOWN");
        item.rationale = QStringLiteral("Explicitly declared in task signature [Primary: %1]").arg(topId);
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        scopesMap.insert(scope, item);
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
            auto [topId, score, srcTypes] = findEvidenceDecomposition(scope);
            PredictedItem item;
            item.item = scope;
            item.status = QStringLiteral("LIKELY");
            item.rationale = QStringLiteral("Observed in %1 comparable historical tasks [Primary: %2]").arg(it.value()).arg(topId);
            item.evidenceReferences = histScopeEvidence.value(scope);
            item.primaryEvidenceId = topId;
            item.contributionScore = score;
            item.sourceTypes = srcTypes;
            scopesMap.insert(scope, item);
        }
    }

    if (scopesMap.isEmpty()) {
        const QString defaultScope = QStringLiteral("project");
        auto [topId, score, srcTypes] = findEvidenceDecomposition(defaultScope);
        PredictedItem item;
        item.item = defaultScope;
        item.status = QStringLiteral("LIKELY");
        item.rationale = QStringLiteral("Standard canonical scope for project subsystem [Primary: %1]").arg(topId);
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        scopesMap.insert(defaultScope, item);
    }
    contract.predictedScopes = scopesMap.values();

    // 2. Predicted Files / Components
    // STRICT ISOLATION: Only local referenced files or local subsystem files are predicted.
    // Foreign project file paths from FrameworkKnowledge are NEVER injected!
    QMap<QString, PredictedItem> filesMap;
    for (const auto& file : normalized.referencedFiles) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(file);
        PredictedItem item;
        item.item = file;
        item.status = QStringLiteral("KNOWN");
        item.rationale = QStringLiteral("Explicitly referenced in task signature [Primary: %1]").arg(topId);
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        filesMap.insert(file, item);
    }

    // Match local subsystem files
    if (normalized.targetSubsystem == QStringLiteral("memory")) {
        const QStringList memFiles{
            QStringLiteral("src/core/ProjectMemory.h"),
            QStringLiteral("src/core/ProjectMemory.cpp"),
            QStringLiteral("src/core/MemoryCommand.cpp")
        };
        for (const auto& f : memFiles) {
            if (!filesMap.contains(f)) {
                auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("ProjectMemory"));
                PredictedItem item;
                item.item = f;
                item.status = QStringLiteral("LIKELY");
                item.rationale = QStringLiteral("Canonical implementation file for memory subsystem [Primary: %1]").arg(topId);
                item.evidenceReferences = contract.evidenceReferences;
                item.primaryEvidenceId = topId;
                item.contributionScore = score;
                item.sourceTypes = srcTypes;
                filesMap.insert(f, item);
            }
        }
    } else if (normalized.targetSubsystem == QStringLiteral("orchestration") ||
               normalized.targetSubsystem == QStringLiteral("worker")) {
        const QStringList orchFiles{
            QStringLiteral("src/core/ExecutionOrchestrator.h"),
            QStringLiteral("src/core/ExecutionOrchestrator.cpp"),
            QStringLiteral("src/core/WorkerTaskServices.h")
        };
        for (const auto& f : orchFiles) {
            if (!filesMap.contains(f)) {
                auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("ExecutionOrchestrator"));
                PredictedItem item;
                item.item = f;
                item.status = QStringLiteral("LIKELY");
                item.rationale = QStringLiteral("Canonical implementation file for execution orchestration subsystem [Primary: %1]").arg(topId);
                item.evidenceReferences = contract.evidenceReferences;
                item.primaryEvidenceId = topId;
                item.contributionScore = score;
                item.sourceTypes = srcTypes;
                filesMap.insert(f, item);
            }
        }
    } else if (normalized.targetSubsystem == QStringLiteral("context")) {
        const QStringList ctxFiles{
            QStringLiteral("src/core/ContextCoordinationService.h"),
            QStringLiteral("src/core/ContextCoordinationService.cpp")
        };
        for (const auto& f : ctxFiles) {
            if (!filesMap.contains(f)) {
                auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("ContextCoordinationService"));
                PredictedItem item;
                item.item = f;
                item.status = QStringLiteral("LIKELY");
                item.rationale = QStringLiteral("Canonical implementation file for context coordination subsystem [Primary: %1]").arg(topId);
                item.evidenceReferences = contract.evidenceReferences;
                item.primaryEvidenceId = topId;
                item.contributionScore = score;
                item.sourceTypes = srcTypes;
                filesMap.insert(f, item);
            }
        }
    }

    // Historical file co-occurrence from local matching clusters
    for (const auto& mc : matchingClusters) {
        for (const auto& f : mc.mentionedFiles) {
            if (!filesMap.contains(f)) {
                auto [topId, score, srcTypes] = findEvidenceDecomposition(f);
                PredictedItem item;
                item.item = f;
                item.status = QStringLiteral("LIKELY");
                item.rationale = QStringLiteral("Co-modified in historical task '%1' [Primary: %2]").arg(mc.taskName, topId);
                item.evidenceReferences = mc.eventIds;
                item.primaryEvidenceId = topId;
                item.contributionScore = score;
                item.sourceTypes = srcTypes;
                filesMap.insert(f, item);
            }
        }
    }
    contract.predictedFiles = filesMap.values();

    // 3. Predicted Validation Suites
    QMap<QString, PredictedItem> valMap;
    // Core test suite is required for C++ core changes
    {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("core"));
        PredictedItem item;
        item.item = QStringLiteral("aramf_core_tests");
        item.status = QStringLiteral("KNOWN");
        item.rationale = QStringLiteral("Primary regression suite for core framework services [Primary: %1]").arg(topId);
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        valMap.insert(item.item, item);
    }

    if (normalized.targetSubsystem == QStringLiteral("memory")) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("memory"));
        PredictedItem cs;
        cs.item = QStringLiteral("memory cold-start");
        cs.status = QStringLiteral("LIKELY");
        cs.rationale = QStringLiteral("Memory subsystem modification requires cold-start verification [Primary: %1]").arg(topId);
        cs.evidenceReferences = contract.evidenceReferences;
        cs.primaryEvidenceId = topId;
        cs.contributionScore = score;
        cs.sourceTypes = srcTypes;
        valMap.insert(cs.item, cs);

        PredictedItem mv;
        mv.item = QStringLiteral("memory validate");
        mv.status = QStringLiteral("LIKELY");
        mv.rationale = QStringLiteral("Memory consistency validation required after memory modifications [Primary: %1]").arg(topId);
        mv.evidenceReferences = contract.evidenceReferences;
        mv.primaryEvidenceId = topId;
        mv.contributionScore = score;
        mv.sourceTypes = srcTypes;
        valMap.insert(mv.item, mv);
    }

    if (normalized.targetSubsystem == QStringLiteral("ui")) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("ui"));
        PredictedItem wf;
        wf.item = QStringLiteral("aramf_workflow_tests");
        wf.status = QStringLiteral("LIKELY");
        wf.rationale = QStringLiteral("UI workflow navigation regression suite [Primary: %1]").arg(topId);
        wf.primaryEvidenceId = topId;
        wf.contributionScore = score;
        wf.sourceTypes = srcTypes;
        valMap.insert(wf.item, wf);
    }

    // Historical test suites observed
    for (const auto& mc : matchingClusters) {
        for (const auto& suite : mc.testSuites) {
            if (!valMap.contains(suite)) {
                auto [topId, score, srcTypes] = findEvidenceDecomposition(suite);
                PredictedItem item;
                item.item = suite;
                item.status = QStringLiteral("LIKELY");
                item.rationale = QStringLiteral("Historically executed in comparable task '%1' [Primary: %2]").arg(mc.taskName, topId);
                item.evidenceReferences = mc.eventIds;
                item.primaryEvidenceId = topId;
                item.contributionScore = score;
                item.sourceTypes = srcTypes;
                valMap.insert(suite, item);
            }
        }
    }
    contract.predictedValidation = valMap.values();

    // 4. Predicted Risk Categories (11 Canonical Categories)
    QMap<QString, PredictedItem> riskMap;
    if (normalized.governanceClass == QStringLiteral("destructive")) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("destructive"));
        PredictedItem item;
        item.item = QStringLiteral("DESTRUCTIVE_OPERATION");
        item.status = QStringLiteral("KNOWN");
        item.rationale = QStringLiteral("Task requests destructive operations requiring explicit verification [Primary: %1]").arg(topId);
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        riskMap.insert(item.item, item);
    }

    if (normalized.targetSubsystem == QStringLiteral("memory")) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("memory"));
        PredictedItem item;
        item.item = QStringLiteral("RECORDER_CONSISTENCY");
        item.status = QStringLiteral("LIKELY");
        item.rationale = QStringLiteral("Memory changes risk corrupting sequence numbers or append-only log [Primary: %1]").arg(topId);
        item.evidenceReferences = contract.evidenceReferences;
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        riskMap.insert(item.item, item);
    }

    if (normalized.targetSubsystem == QStringLiteral("orchestration") || contract.predictedFiles.size() > 3) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("ownership"));
        PredictedItem item;
        item.item = QStringLiteral("OWNERSHIP_CONFLICT");
        item.status = QStringLiteral("LIKELY");
        item.rationale = QStringLiteral("Multi-file operations increase runtime ownership contestation risk [Primary: %1]").arg(topId);
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        riskMap.insert(item.item, item);
    }

    if (failureCount > 0) {
        auto [topId, score, srcTypes] = findEvidenceDecomposition(QStringLiteral("fail"));
        PredictedItem item;
        item.item = QStringLiteral("CROSS_LAYER_REGRESSION");
        item.status = QStringLiteral("LIKELY");
        item.rationale = QStringLiteral("%1 historical failure(s) observed across comparable tasks [Primary: %2]").arg(failureCount).arg(topId);
        item.evidenceReferences = contract.evidenceReferences;
        item.primaryEvidenceId = topId;
        item.contributionScore = score;
        item.sourceTypes = srcTypes;
        riskMap.insert(item.item, item);
    }
    contract.predictedRiskCategories = riskMap.values();

    // 5. Change Breadth (6-Tier Taxonomy: LOCAL, COMPONENT, MULTI_COMPONENT, CROSS_LAYER, PROJECT_WIDE, UNKNOWN)
    QStringList fileList;
    for (const auto& p : contract.predictedFiles) fileList.append(p.item);
    contract.predictedChangeBreadth = determineChangeBreadth(fileList, normalized);

    const int fileCount = contract.predictedFiles.size();
    if (contract.predictedChangeBreadth == QStringLiteral("LOCAL")) {
        contract.breadthRationale = QStringLiteral("Isolated single-file modification (%1 file).").arg(fileCount);
    } else if (contract.predictedChangeBreadth == QStringLiteral("COMPONENT")) {
        contract.breadthRationale = QStringLiteral("Component-scoped modifications isolated to %1 files in one module.").arg(fileCount);
    } else if (contract.predictedChangeBreadth == QStringLiteral("MULTI_COMPONENT")) {
        contract.breadthRationale = QStringLiteral("Multi-component modifications spanning %1 files across 2 components.").arg(fileCount);
    } else if (contract.predictedChangeBreadth == QStringLiteral("CROSS_LAYER")) {
        contract.breadthRationale = QStringLiteral("Cross-layer change predicted spanning %1 files across architecture boundaries.").arg(fileCount);
    } else if (contract.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE")) {
        contract.breadthRationale = QStringLiteral("Project-wide modifications impacting build configuration, framework rules, or 4+ subsystems (%1 files).").arg(fileCount);
    } else {
        contract.breadthRationale = QStringLiteral("Insufficient file evidence to categorize change breadth.");
    }

    // 6. Explainable Confidence Calculation (Strict Calibration P3.1.4)
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

        // Sample size scaling rules (P3.1.4):
        // N=1: Score strictly capped at <= 0.35, rating LOW
        // N=2: Score strictly capped at <= 0.70, rating MEDIUM or LOW
        if (conf.sampleSize == 1) {
            rawScore = qMin(rawScore, 0.35);
        } else if (conf.sampleSize == 2) {
            rawScore = qMin(rawScore, 0.70);
        }

        conf.score = qBound(0.05, rawScore, 1.0);

        if (conf.sampleSize == 1 || conf.score < 0.35) {
            conf.rating = QStringLiteral("LOW");
        } else if (conf.sampleSize >= 3 &&
                   conf.score >= 0.75 &&
                   conf.consistencyScore >= 0.85 &&
                   conf.matchPrecision >= 0.75 &&
                   conf.freshnessScore >= 0.70 &&
                   conf.conflictingEvidencePenalty == 0.0) {
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
    if (prediction.predictedChangeBreadth == QStringLiteral("LOCAL") || prediction.predictedChangeBreadth == QStringLiteral("NARROW")) {
        eval.changeBreadthComparison = actualCount <= 1 ? QStringLiteral("ACCURATE") : QStringLiteral("UNDERPREDICTED");
    } else if (prediction.predictedChangeBreadth == QStringLiteral("COMPONENT")) {
        eval.changeBreadthComparison = (actualCount >= 2 && actualCount <= 3) ? QStringLiteral("ACCURATE") :
            (actualCount < 2 ? QStringLiteral("OVERPREDICTED") : QStringLiteral("UNDERPREDICTED"));
    } else if (prediction.predictedChangeBreadth == QStringLiteral("MULTI_COMPONENT") || prediction.predictedChangeBreadth == QStringLiteral("MODERATE")) {
        eval.changeBreadthComparison = (actualCount >= 3 && actualCount <= 5) ? QStringLiteral("ACCURATE") :
            (actualCount < 3 ? QStringLiteral("OVERPREDICTED") : QStringLiteral("UNDERPREDICTED"));
    } else if (prediction.predictedChangeBreadth == QStringLiteral("CROSS_LAYER") ||
               prediction.predictedChangeBreadth == QStringLiteral("PROJECT_WIDE") ||
               prediction.predictedChangeBreadth == QStringLiteral("BROAD")) {
        eval.changeBreadthComparison = actualCount >= 4 ? QStringLiteral("ACCURATE") : QStringLiteral("OVERPREDICTED");
    } else {
        eval.changeBreadthComparison = QStringLiteral("ACCURATE");
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

QString PredictiveOptimizationService::predictionsDirectory(const QString& projectRoot)
{
    return QDir(projectRoot).filePath(QStringLiteral("ARAMF_WORKER/predictions"));
}

QString PredictiveOptimizationService::predictionsRegistryPath(const QString& projectRoot)
{
    return QDir(predictionsDirectory(projectRoot)).filePath(QStringLiteral("prediction-registry.jsonl"));
}

QString PredictiveOptimizationService::evaluationsPath(const QString& projectRoot)
{
    return QDir(predictionsDirectory(projectRoot)).filePath(QStringLiteral("prediction-evaluations.jsonl"));
}
