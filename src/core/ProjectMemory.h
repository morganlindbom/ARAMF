// ProjectMemory.h

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class ProjectModel;

class ProjectMemory final : public QObject
{
    Q_OBJECT

public:
    explicit ProjectMemory(QObject* parent = nullptr);

    bool initialize(const QString& projectRoot, const ProjectModel* model, QString* error = nullptr);
    bool appendEvent(const QString& projectRoot,
                     const QString& eventType,
                     const QString& task,
                     const QJsonObject& fields = {},
                     QString* error = nullptr);
    QJsonObject validate(const QString& projectRoot, QString* error = nullptr) const;
    qint64 memoryUsageBytes(const QString& projectRoot) const;

private:
    bool ensureDirectories(const QString& projectRoot, QString* error) const;
    bool writeInitialFiles(const QString& projectRoot, const ProjectModel* model, QString* error) const;
    bool generateCurrentState(const QString& projectRoot, QString* error) const;
    bool generateColdStartValidation(const QString& projectRoot, QString* error) const;
    bool writeValidationReport(const QString& projectRoot, const QJsonObject& report, QString* error) const;
    qint64 managedMemoryUsage(const QString& projectRoot) const;
    bool withinConfiguredLimit(const QString& projectRoot, qint64 additionalBytes, QString* error);
};
