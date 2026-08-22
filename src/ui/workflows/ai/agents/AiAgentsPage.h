#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class CapabilityCheckGroup;
class QComboBox;
class QLineEdit;

class AiAgentsPage final : public QWidget
{
    Q_OBJECT
public:
    explicit AiAgentsPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void persist();
    void refreshAdditionalAgentAvailability();

    ProjectModel* model_ = nullptr;
    QComboBox* primary_ = nullptr;
    QLineEdit* custom_ = nullptr;
    QList<CapabilityCheckGroup*> additionalGroups_;
};
