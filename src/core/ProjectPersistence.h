#pragma once

#include <QString>

class ProjectModel;

class ProjectPersistence final {
public:
    bool save(const ProjectModel& model, const QString& filePath, QString* error = nullptr) const;
    bool load(ProjectModel* model, const QString& filePath, QString* error = nullptr) const;
};
