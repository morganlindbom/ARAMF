#pragma once

#include <QWidget>

class ProjectModel;
class ReleaseManagementService;

class SchemaCompatibilityPage final : public QWidget
{
    Q_OBJECT
public:
    SchemaCompatibilityPage(ProjectModel* model, ReleaseManagementService* service,
                            QWidget* parent = nullptr);
};

