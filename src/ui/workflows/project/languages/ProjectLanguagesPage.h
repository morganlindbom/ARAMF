#pragma once

#include "core/ProjectModel.h"
#include <QWidget>

class CapabilityCheckGroup;
class ProjectLanguagesPage final : public QWidget
{
public:
    explicit ProjectLanguagesPage(ProjectModel* model, QWidget* parent = nullptr);
private:
    ProjectModel* model_;
    CapabilityCheckGroup* languages_;
    void refresh();
};
