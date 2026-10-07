#pragma once
#include "core/ProjectModel.h"
#include <QWidget>
class CapabilityCheckGroup;
class ProjectPlatformsPage final : public QWidget
{
public:
    enum class Section { All, Hosts, Targets };
    explicit ProjectPlatformsPage(ProjectModel* model, QWidget* parent = nullptr, Section section = Section::All);
private:
    ProjectModel* model_;
    Section section_;
    CapabilityCheckGroup* hosts_;
    CapabilityCheckGroup* targets_;
    void refresh();
};
