#pragma once

#include "core/EnvironmentCatalog.h"
#include "core/ProjectModel.h"
#include <QWidget>

class CapabilityCheckGroup;
class CapabilityDetailsEditor;
class ProjectFrameworksPage : public QWidget
{
public:
    explicit ProjectFrameworksPage(ProjectModel* model, QWidget* parent = nullptr,
                                   QList<EnvironmentOption> options = {}, QStringList managedIds = {});
private:
    ProjectModel* model_;
    CapabilityCheckGroup* frameworks_;
    CapabilityDetailsEditor* details_ = nullptr;
    QStringList managedIds_;
    void refresh();
};
