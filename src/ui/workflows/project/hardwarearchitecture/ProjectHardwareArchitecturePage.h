#pragma once
#include "core/ProjectModel.h"
#include <QWidget>
class CapabilityCheckGroup;
class ProjectHardwareArchitecturePage final : public QWidget
{
public:
    enum class Section { All, Architectures, Processors, Targets };
    explicit ProjectHardwareArchitecturePage(ProjectModel* model, QWidget* parent = nullptr, Section section = Section::All);
private:
    ProjectModel* model_;
    Section section_;
    CapabilityCheckGroup* architectures_;
    CapabilityCheckGroup* processors_;
    CapabilityCheckGroup* hardware_;
    void refresh();
};
