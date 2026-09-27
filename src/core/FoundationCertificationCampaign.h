#pragma once

#include <QJsonObject>
#include <QString>

// External governance: no Foundation implementation includes this coordinator.
class FoundationCertificationCampaign final
{
public:
    static bool start(const QString& root, const QString& file, const QString& subject, QString* error);
    static bool verify(const QString& root, const QString& file, const QString& subject,
                       const QString& revision, QJsonObject* result, QString* error);
    static bool validateEvidence(const QString& root, const QString& subject, const QString& lifecycle,
                                 const QString& revision, const QString& artifact, QJsonObject* evidence, QString* error);
    static bool certify(const QString& root, const QString& file, const QString& subject,
                        const QString& revision, const QString& artifact, QJsonObject* result, QString* error);
    static bool complete(const QString& root, const QString& file, const QString& subject, QString* error);
    static bool acceptIntegration(const QString& root, const QString& file, QJsonObject* result, QString* error);
    static bool eligible(const QString& root, const QString& file, QString* error);
    static QJsonObject freshness(const QString& root);
    // Historical bytes are checked against the sealed certificate and its own
    // Git revision, never against today's manifest paths or working sources.
    static QJsonObject sourceSnapshot(const QString& root, const QString& subject, const QString& revision, QString* error);
    static bool validateHistorical(const QString& root, const QString& subject, const QJsonObject& snapshot, QString* error);
    static bool validateCurrentSource(const QString& root, const QString& subject, const QString& revision, QString* error);
};
