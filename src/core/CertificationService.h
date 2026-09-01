#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QString>

class ProjectModel;

class CertificationService final
{
public:
    bool initialize(const QString& projectRoot, const ProjectModel* model, QString* error = nullptr) const;
    bool start(const QString& projectRoot,
               const QString& subject,
               const QString& certificateType,
               const QString& scope,
               const QString& verificationLevel,
               const QJsonArray& requirements,
               const QJsonObject& context = {},
               QJsonObject* result = nullptr,
               QString* error = nullptr) const;
    bool issue(const QString& projectRoot,
               const QJsonObject& certificate,
               const QString& status,
               const QJsonArray& evidence,
               QJsonObject* result = nullptr,
               QString* error = nullptr) const;
    QList<QJsonObject> certificates(const QString& projectRoot, QString* error = nullptr) const;
    QList<QJsonObject> certificatesForSubject(const QString& projectRoot,
                                              const QString& subject,
                                              QString* error = nullptr) const;
    bool latestForSubject(const QString& projectRoot,
                          const QString& subject,
                          QJsonObject* result,
                          QString* error = nullptr) const;
    QJsonObject currentState(const QString& projectRoot, QString* error = nullptr) const;
    QJsonObject contract(const QString& projectRoot, QString* error = nullptr) const;

private:
    bool appendCertificate(const QString& projectRoot, const QJsonObject& certificate, QString* error) const;
    bool refreshCurrentState(const QString& projectRoot, QString* error) const;
};
