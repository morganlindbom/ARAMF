#pragma once

#include "core/ProjectModel.h"
#include <QWidget>

class CapabilityCheckGroup;
class ProjectFrameworksPage final : public QWidget
{
public:
    explicit ProjectFrameworksPage(ProjectModel* model, QWidget* parent = nullptr);
private:
    ProjectModel* model_;
    CapabilityCheckGroup* frameworks_;
    void refresh();
};
