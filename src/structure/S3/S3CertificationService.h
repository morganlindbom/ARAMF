#pragma once

#include "DependencyInterfaces.h"

namespace S3 {

class CertificationServiceAdapter final {
public:
    static bool certify(const QString& projectRoot,
                        const Configuration& configuration,
                        const S1::Configuration& ownership,
                        const S2::Configuration& physicalStructure,
                        const QString& sourceRevision,
                        const QJsonObject& testResult,
                        int iteration,
                        QJsonObject* issuedCertificate = nullptr,
                        QString* error = nullptr);
};

} // namespace S3
