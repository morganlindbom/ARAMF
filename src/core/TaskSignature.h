#pragma once

#include "WorkerTaskServices.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

class ProjectModel;

// Deterministic Task Signature for P3 Predictive Task Optimization.
// Normalizes task characteristics into a canonical representation and computes
// a stable SHA-256 fingerprint without neural embeddings or non-deterministic heuristics.
struct TaskSignature final
{
    QString taskCategory;              // e.g. "implementation", "refactoring", "testing", "memory", "governance", etc.
    QString operationType;             // e.g. "modify", "create", "delete", "validate", "inspect", "update", "record"
    QStringList relevantScopes;        // Normalized, unique, sorted scopes
    QStringList referencedFiles;       // Normalized, unique, sorted relative paths (forward slashes)
    QString targetSubsystem;           // e.g. "memory", "worker", "orchestration", "ui", "templates", "rules", "build", "general"
    QString languageFramework;         // e.g. "cpp17/qt6", "cmake", "json"
    QStringList validationRequirements;// e.g. "unit", "subsystem", "full-regression", "cold-start", "memory-consistency"
    QString governanceClass;           // e.g. "governed-write", "read-only", "admin-override"
    QString resourceOwnershipClass;    // e.g. "exclusive-file", "shared-resource", "none"
    QString signatureVersion = QStringLiteral("1.0");

    void normalize();
    QString fingerprint() const;
    bool isValid(QString* error = nullptr) const;

    QJsonObject toJson() const;
    static TaskSignature fromJson(const QJsonObject& value, QString* error = nullptr);
    static TaskSignature fromWorkerTaskRequest(const WorkerTaskRequest& request, const ProjectModel* model = nullptr);
    static TaskSignature fromTaskAndCategory(const QString& task, const QString& category,
                                             const QStringList& scopes = {}, const QStringList& files = {});

    // Deterministic similarity score [0.0, 1.0] comparing this signature to another.
    double similarity(const TaskSignature& other) const;
    bool matches(const TaskSignature& other, double threshold = 0.5, double* score = nullptr) const;
};
