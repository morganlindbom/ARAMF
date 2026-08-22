#pragma once

#include <QStringList>

struct ControlPlanePreparation
{
    bool success = false;
    bool legacyDetected = false;
    bool migrated = false;
    bool bothDetected = false;
    QStringList warnings;
    QString error;
};

ControlPlanePreparation prepareControlPlane(const QString& projectRoot);
