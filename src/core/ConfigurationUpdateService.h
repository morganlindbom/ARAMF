#pragma once

#include "Services.h"
#include <QJsonObject>

struct ConfigurationUpdateResult {
    bool success = false;
    bool noChange = false;
    bool removalBlocked = false;
    QString error;
    QJsonObject plan;
};

class ConfigurationUpdateService final : public QObject
{
    Q_OBJECT
public:
    explicit ConfigurationUpdateService(QObject* parent = nullptr);
    ConfigurationUpdateResult validate(const ProjectModel& model) const;
    ConfigurationUpdateResult apply(const ProjectModel& model, const QString& validationFingerprint = {}) const;
    static QJsonObject canonicalState(const ProjectModel& model);
};
