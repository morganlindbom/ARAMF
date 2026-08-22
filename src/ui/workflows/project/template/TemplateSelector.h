#pragma once

#include <QWidget>

#include "core/ProjectModel.h"
#include "core/Services.h"

class QComboBox;

class TemplateSelector final : public QWidget {
public:
    TemplateSelector(ProjectModel* model, TemplateManager* manager, QWidget* parent = nullptr);

private slots:
    void refreshFromModel();

private:
    ProjectModel* model_;
    TemplateManager* manager_;
    QComboBox* selector_;
};
