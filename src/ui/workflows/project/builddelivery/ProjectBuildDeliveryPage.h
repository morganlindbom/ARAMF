#pragma once
#include "core/ProjectModel.h"
#include <QWidget>
class CapabilityCheckGroup;
class ProjectBuildDeliveryPage final : public QWidget
{
public:
    enum class Section { All, Toolchains, BuildSystems, Testing, Automation };
    explicit ProjectBuildDeliveryPage(ProjectModel* model, QWidget* parent = nullptr, Section section = Section::All);
private:
    ProjectModel* model_;
    Section section_;
    CapabilityCheckGroup* toolchains_;
    CapabilityCheckGroup* buildSystems_;
    CapabilityCheckGroup* dependencies_;
    CapabilityCheckGroup* configurations_;
    CapabilityCheckGroup* testing_;
    CapabilityCheckGroup* quality_;
    CapabilityCheckGroup* automation_;
    CapabilityCheckGroup* delivery_;
    void refresh();
};
