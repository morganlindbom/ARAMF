#pragma once

#include <QString>
#include <QJsonObject>

class ProjectModel;

class ProjectPersistence final {
public:
    QJsonObject toJson(const ProjectModel& model) const;
    bool fromJson(ProjectModel* model, const QJsonObject& root, QString* error = nullptr) const;
    // The same schema as project persistence, without project identity or template metadata.
    QJsonObject configuration(const ProjectModel& model) const;
    bool save(const ProjectModel& model, const QString& filePath, QString* error = nullptr) const;
    bool load(ProjectModel* model, const QString& filePath, QString* error = nullptr) const;
};
