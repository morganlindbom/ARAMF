#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class CapabilityCheckGroup;
class QPushButton;

class AiAutonomyPage final : public QWidget
{
    Q_OBJECT
public:
    explicit AiAutonomyPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void persist();
    void toggleAll();
    void updateSelectAllText();
    ProjectModel* model_ = nullptr;
    QList<CapabilityCheckGroup*> groups_;
    QList<CapabilityCheckGroup*> highRiskGroups_;
    QPushButton* selectAll_ = nullptr;
};
