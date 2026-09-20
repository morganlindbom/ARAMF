#pragma once

#include "PhysicalStructure.h"

namespace S2 {

class CertificationServiceAdapter final {
public:
    static bool certify(const QString& projectRoot,
                        const Configuration& configuration,
                        const S1::Configuration& ownership,
                        const QString& sourceRevision,
                        const QJsonObject& testResult,
                        int iteration,
                        QJsonObject* issuedCertificate = nullptr,
                        QString* error = nullptr);
};

} // namespace S2
