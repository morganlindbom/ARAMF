#pragma once

#include "core/ProjectModel.h"
#include <QWidget>

class CapabilityCheckGroup;
class ProjectDevelopmentToolsPage final : public QWidget
{
public:
    enum class Section { All, Ide, VersionControl, Support };
    explicit ProjectDevelopmentToolsPage(ProjectModel* model, QWidget* parent = nullptr, Section section = Section::All);
private:
    ProjectModel* model_;
    Section section_;
    CapabilityCheckGroup* ides_;
    CapabilityCheckGroup* versionControl_;
    CapabilityCheckGroup* support_;
    void refresh();
};
