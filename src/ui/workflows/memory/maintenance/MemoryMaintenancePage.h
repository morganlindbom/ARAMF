#pragma once

#include <QWidget>

class CapabilityCheckGroup;
class QComboBox;
class QDoubleSpinBox;
class QProgressBar;
class QPushButton;
class ProjectModel;

class MemoryMaintenancePage final : public QWidget
{
    Q_OBJECT
public:
    explicit MemoryMaintenancePage(ProjectModel* model, QWidget* parent = nullptr);
private:
    void refresh(); void persist(); void toggleAll(); void updateSelectAllText(); void updateLimit();
    ProjectModel* model_ = nullptr; QComboBox* updateStrategy_ = nullptr; QDoubleSpinBox* maximum_ = nullptr; QComboBox* unit_ = nullptr; QProgressBar* usage_ = nullptr; QPushButton* selectAll_ = nullptr; QList<CapabilityCheckGroup*> groups_;
};
