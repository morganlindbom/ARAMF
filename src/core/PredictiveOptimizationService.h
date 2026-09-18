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

// Single predicted dimension item with deterministic rationale and evidence references
struct PredictedItem final
{
    QString item;
    QString status;                  // "KNOWN", "LIKELY", "UNKNOWN"
    QString rationale;               // Deterministic reason: WHY was this predicted?
    QStringList evidenceReferences;  // Event IDs, sequence numbers, rule names, contract IDs

    QJsonObject toJson() const;
    static PredictedItem fromJson(const QJsonObject& obj);
};

// Explainable confidence model based on measurable historical metrics
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
    QString predictedChangeBreadth;           // "NARROW", "MODERATE", "BROAD"
    QString breadthRationale;
    PredictionConfidence confidence;
    QStringList evidenceReferences;
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

// Deterministic Evidence-Based Prediction Engine (P3.1.1)
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
};
