#pragma once

#include <QWidget>

class CapabilityCheckGroup;
class QComboBox;
class ProjectModel;

class RuleRoutingPage final : public QWidget
{
    Q_OBJECT
public:
    explicit RuleRoutingPage(ProjectModel* model, QWidget* parent = nullptr);
private:
    void refresh();
    void persist();
    ProjectModel* model_ = nullptr;
    QComboBox* loading_ = nullptr;
    QComboBox* conflict_ = nullptr;
    CapabilityCheckGroup* workScopes_ = nullptr;
    CapabilityCheckGroup* projectScopes_ = nullptr;
    CapabilityCheckGroup* contextPolicies_ = nullptr;
};
