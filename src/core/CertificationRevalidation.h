#pragma once

#include <QJsonObject>
#include <QString>

struct CertificationRevalidationResult final {
    QString subject;
    QString revalidationId;
    QString revalidationOfCertificateId;
    QString historicalLifecycle;
    QString evidenceArtifact;
    QString evidenceFingerprint;
    QString sourceRevision;
    QString sourceFingerprint;
    QString contractFingerprint;
    QString dependencyManifestFingerprint;
    QString status;

    QJsonObject toJson() const;
};

class CertificationRevalidationService final {
public:
    static bool revalidate(const QString& projectRoot,
                           const QString& subject,
                           const QJsonObject& regressionEvidence,
                           CertificationRevalidationResult* result = nullptr,
                           QString* error = nullptr);

    static bool latest(const QString& projectRoot,
                       const QString& subject,
                       QJsonObject* result,
                       QString* error = nullptr);
};
