#pragma once

#include <QJsonObject>
#include <QStringList>

enum class ValidationLevel { Focused, Subsystem, FullRegression };

struct ValidationPlan {
    ValidationLevel level = ValidationLevel::Focused;
    QStringList requiredChecks;
    QStringList optionalChecks;
    QStringList escalationTriggers;
    QString rationale;
};

class ValidationRouting final
{
public:
    static ValidationPlan route(const QStringList& changedFiles,
                                const QString& taskType = {},
                                bool explicitFullRegression = false,
                                bool focusedValidationFailed = false,
                                bool broadImpactUncertain = false);
    static QJsonObject policy();
    static QJsonObject taskPlan(const QStringList& changedFiles, const QString& taskType,
                                const QJsonObject& impact, const QString& risk);
    static QString levelName(ValidationLevel level);
};
