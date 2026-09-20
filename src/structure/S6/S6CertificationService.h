#pragma once
#include "StructuralEvolutionEnforcement.h"
namespace S6 {
class CertificationServiceAdapter final {
public:
    static bool certify(const QString& projectRoot, const Configuration& configuration,
                        const QString& sourceRevision, const QJsonObject& testResult, int iteration,
                        QJsonObject* issuedCertificate = nullptr, QString* error = nullptr);
};
}
