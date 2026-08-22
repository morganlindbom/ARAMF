#pragma once

#include <QWidget>

class CapabilityCheckGroup;
class QComboBox;
class QPushButton;
class ProjectModel;

class RuleSelectionPage final : public QWidget
{
    Q_OBJECT
public:
    explicit RuleSelectionPage(ProjectModel* model, QWidget* parent = nullptr);
private:
    void refresh();
    void persist();
    void toggleAll();
    void updateSelectAllText();
    ProjectModel* model_ = nullptr;
    QComboBox* enforcement_ = nullptr;
    QPushButton* selectAll_ = nullptr;
    QList<CapabilityCheckGroup*> groups_;
};
