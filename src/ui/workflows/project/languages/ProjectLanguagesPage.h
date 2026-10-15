#pragma once

#include "core/EnvironmentCatalog.h"
#include "core/ProjectModel.h"
#include <QWidget>

class CapabilityCheckGroup;
class CapabilityDetailsEditor;
class ProjectLanguagesPage : public QWidget
{
public:
    explicit ProjectLanguagesPage(ProjectModel* model, QWidget* parent = nullptr,
                                  QList<EnvironmentOption> options = {}, QStringList managedIds = {});
private:
    ProjectModel* model_;
    CapabilityCheckGroup* languages_;
    CapabilityDetailsEditor* details_ = nullptr;
    QStringList managedIds_;
    void refresh();
};
