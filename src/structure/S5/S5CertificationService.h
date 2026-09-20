#pragma once
#include "DecompositionModularity.h"
namespace S5 {
class CertificationServiceAdapter final {
public:
    static bool certify(const QString& projectRoot, const Configuration& configuration,
                        const S1::Configuration& ownership, const S2::Configuration& physical,
                        const S3::Configuration& dependencies, const S4::Configuration& composition,
                        const QString& sourceRevision, const QJsonObject& testResult, int iteration,
                        QJsonObject* issuedCertificate = nullptr, QString* error = nullptr);
};
}
