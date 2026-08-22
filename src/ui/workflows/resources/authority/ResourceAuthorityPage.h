#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class CapabilityCheckGroup;
class QComboBox;
class QListWidget;

class ResourceAuthorityPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ResourceAuthorityPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void refreshSelected();
    void saveSelected();
    ProjectResource* selectedResource(QList<ProjectResource>& resources) const;

    ProjectModel* model_ = nullptr;
    QListWidget* resources_ = nullptr;
    QComboBox* authority_ = nullptr;
    CapabilityCheckGroup* scopes_ = nullptr;
    QString selectedResourceId_;
};
