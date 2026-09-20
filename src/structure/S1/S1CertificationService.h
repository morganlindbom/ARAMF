#pragma once

#include "ResponsibilityOwnership.h"
#include <QJsonObject>

namespace S1 {

class CertificationServiceAdapter final
{
public:
    static bool certify(const QString& projectRoot,
                        const Configuration& configuration,
                        const QString& sourceRevision,
                        const QJsonObject& testResult,
                        int iteration = 1,
                        QJsonObject* issuedCertificate = nullptr,
                        QString* error = nullptr);
};

} // namespace S1
