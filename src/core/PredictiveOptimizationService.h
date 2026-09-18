#pragma once

#include "ProjectModel.h"
#include "TaskSignature.h"
#include "WorkerTaskServices.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// Evidence Source Taxonomy (P3.1.2)
enum class EvidenceSourceType {
    LocalOperationalEvent,   // P2 historical task/test/build event from local project
    ApprovedGlobalKnowledge, // Explicitly approved Framework Knowledge entry
    ScopePolicy,             // Validation policy or routing rule specification
    ContractHistory          // Historical TaskContract handoff
};

QString evidenceSourceTypeToString(EvidenceSourceType type);
EvidenceSourceType stringToEvidenceSourceType(const QString& str);

// Ranked Evidence Structure (P3.1.2)
struct RankedEvidence final
{
    QString evidenceId;
    EvidenceSourceType sourceType = EvidenceSourceType::LocalOperationalEvent;
    QString originProjectId;
    double relevance = 0.0;          // [0.0, 1.0]
    double freshness = 1.0;          // [0.0, 1.0]
    double outcomeWeight = 1.0;      // 1.0 for pass/success, 0.5 for failure/risk, 0.0 for invalid
    double sourceMultiplier = 1.0;   // 1.0 for local, 0.85 for global, 0.70 for policy, 0.90 for contract
    double totalRank = 0.0;          // sourceMultiplier * (0.50 * relevance + 0.25 * freshness + 0.25 * outcomeWeight)
    QString summary;
    QJsonObject rawPayload;

    QJsonObject toJson() const;
    static RankedEvidence fromJson(const QJsonObject& obj);
};

// Single predicted dimension item with deterministic rationale and evidence references
struct PredictedItem final
{
    QString item;
    QString status;                  // "KNOWN", "LIKELY", "UNKNOWN"
    QString rationale;               // Deterministic reason: WHY was this predicted?
    QStringList evidenceReferences;  // Event IDs, sequence numbers, rule names, contract IDs

    // Explainability Decomposition (P3.1.2)
    QString primaryEvidenceId;       // Top-ranked evidence item that directly justified it
    double contributionScore = 0.0;  // Aggregated evidence rank/relevance
    QStringList sourceTypes;         // Distinct sources contributing to this item

    QJsonObject toJson() const;
    static PredictedItem fromJson(const QJsonObject& obj);
};

// Explainable confidence model based on measurable historical metrics (P3.1.1, P3.1.4)
struct PredictionConfidence final
{
    double score = 0.0;                       // [0.0, 1.0]
    QString rating;                           // "HIGH", "MEDIUM", "LOW", "INSUFFICIENT_EVIDENCE"
    int sampleSize = 0;                       // Number of matching historical tasks
    double consistencyScore = 0.0;            // Successful / total matching runs
    double freshnessScore = 1.0;              // Freshness factor [0.0, 1.0]
    double matchPrecision = 0.0;              // Average similarity score of matched tasks
    double conflictingEvidencePenalty = 0.0;  // Penalty when historical outcomes conflicted
    QString explanation;                      // Deterministic rationale

    QJsonObject toJson() const;
    static PredictionConfidence fromJson(const QJsonObject& obj);
};

// Canonical, versioned P3 Prediction Contract
// P3 is ADVISORY. P3 predictions carry NO execution authority.
struct PredictionContract final
{
    QString predictionId;
    QString taskId;
    TaskSignature taskSignature;
    QString schemaVersion = QStringLiteral("1.0");
    QJsonObject taskClassification;
    QList<PredictedItem> predictedScopes;
    QList<PredictedItem> predictedFiles;
    QList<PredictedItem> predictedValidation;
    QList<PredictedItem> predictedRiskCategories;
    QString predictedChangeBreadth;           // "LOCAL", "COMPONENT", "MULTI_COMPONENT", "CROSS_LAYER", "PROJECT_WIDE", "UNKNOWN"
    QString breadthRationale;
    PredictionConfidence confidence;
    QStringList evidenceReferences;
    QList<RankedEvidence> rankedEvidence;     // Structured ranked evidence list (P3.1.2)
    QJsonObject provenance;
    QString createdAt;
    QString sourceProjectId;
    QString sourceProjectPath;
    QString advisoryStatus = QStringLiteral("ADVISORY");
    QString status = QStringLiteral("READY"); // "READY", "INSUFFICIENT_EVIDENCE", "INVALID"

    // Architectural guarantee: P3 is advisory and never an execution authority.
    static constexpr bool isExecutionAuthority() { return false; }

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, PredictionContract* result, QString* error = nullptr);
};

// Measurement and comparison between PREDICTED and ACTUAL execution results
struct PredictionEvaluation final
{
    QString predictionId;
    QString actualTaskId;

    // Files metric
    QStringList predictedFiles;
    QStringList actualModifiedFiles;
    QStringList truePositiveFiles;
    QStringList falsePositiveFiles;
    QStringList falseNegativeFiles;
    double filePrecision = 0.0;
    double fileRecall = 0.0;
    QString changeBreadthComparison; // "ACCURATE", "OVERPREDICTED", "UNDERPREDICTED"

    // Scopes metric
    QStringList predictedScopes;
    QStringList actualScopes;
    QStringList matchedScopes;
    QStringList missedScopes;
    QStringList unnecessaryScopes;
    bool scopeMatch = false;

    // Validation metric
    QStringList predictedValidation;
    QStringList actualValidation;
    QStringList matchedValidation;
    QStringList missedValidation;   // CRITICAL: required validation missed
    QStringList unnecessaryValidation;

    // Risk metric
    QStringList predictedRisks;
    QStringList actualFailures;
    QStringList anticipatedFailures;
    QStringList unanticipatedFailures;

    QString evaluatedAt;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, PredictionEvaluation* result, QString* error = nullptr);
};

// Drift Report and Drift Detector (P3.1.4)
struct PredictionDriftReport final
{
    QString status = QStringLiteral("STABLE"); // "STABLE", "DRIFT_SUSPECTED", "DRIFT_CONFIRMED"
    bool driftDetected = false;
    double rollingPrecision = 1.0;
    double rollingRecall = 1.0;
    int missedValidationCount = 0;
    int overconfidenceCount = 0;
    int evaluationCount = 0;
    QStringList alerts;
    QString evaluatedAt;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& obj, PredictionDriftReport* result, QString* error = nullptr);
};

class PredictionDriftDetector final
{
public:
    static PredictionDriftReport evaluateDrift(const QList<PredictionEvaluation>& evaluations,
                                               int windowSize = 10);
    static bool saveDriftReport(const QString& projectRoot,
                                const PredictionDriftReport& report,
                                QString* error = nullptr);
    static bool loadDriftReport(const QString& projectRoot,
                                PredictionDriftReport* report,
                                QString* error = nullptr);
    static QString driftReportPath(const QString& projectRoot);
};

// Deterministic Evidence-Based Prediction Engine
class PredictiveOptimizationService final
{
public:
    // Generate advisory prediction from task signature and historical P2 evidence
    static PredictionContract predict(const ProjectModel& model,
                                      const TaskSignature& signature,
                                      const QJsonObject& callerProvenance = {});

    // Generate advisory prediction directly from a P0 WorkerTaskRequest
    static PredictionContract predictFromRequest(const ProjectModel& model,
                                                 const WorkerTaskRequest& request,
                                                 const QJsonObject& callerProvenance = {});

    // Evaluate prediction against actual P2 execution results
    static PredictionEvaluation evaluate(const PredictionContract& prediction,
                                         const QJsonObject& actualExecutionResult,
                                         const WorkerTaskRequest* actualRequest = nullptr);

    // Persistence & retrieval
    static bool savePrediction(const QString& projectRoot,
                               const PredictionContract& prediction,
                               QString* error = nullptr);

    static bool loadPrediction(const QString& projectRoot,
                               const QString& predictionId,
                               PredictionContract* prediction,
                               QString* error = nullptr);

    static bool saveEvaluation(const QString& projectRoot,
                               const PredictionEvaluation& evaluation,
                               QString* error = nullptr);

    static QList<PredictionContract> listPredictions(const QString& projectRoot,
                                                    QString* error = nullptr);

    static QString predictionsDirectory(const QString& projectRoot);
    static QString predictionsRegistryPath(const QString& projectRoot);
    static QString evaluationsPath(const QString& projectRoot);

    // Ranking and Classification Helpers
    static double calculateEvidenceRank(EvidenceSourceType type,
                                        double relevance,
                                        double freshness,
                                        double outcomeWeight);

    static QString determineChangeBreadth(const QStringList& predictedFiles,
                                          const TaskSignature& signature);

    static QString determineValidationLevel(const QString& breadth,
                                            const QList<PredictedItem>& risks,
                                            double confidenceScore);

    static QStringList canonicalRiskCategories();
};
