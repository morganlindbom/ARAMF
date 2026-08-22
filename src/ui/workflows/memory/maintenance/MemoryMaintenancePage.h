#pragma once

#include <QPair>
#include <QList>
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

    static QList<QPair<QString, qint64>> memorySizePresets();

private:
    void refresh();
    void refreshMemorySizeControls();
    void persist();
    void toggleAll();
    void updateSelectAllText();
    void applyPreset(int index);
    void updateCustomSize();
    void updateCustomDisplay();
    void updateCustomRange();
    void setNaturalCustomUnit(qint64 bytes);

    ProjectModel* model_ = nullptr;
    QComboBox* updateStrategy_ = nullptr;
    QComboBox* memorySizePreset_ = nullptr;
    QWidget* customSizeWidget_ = nullptr;
    QDoubleSpinBox* customSize_ = nullptr;
    QComboBox* customUnit_ = nullptr;
    QProgressBar* usage_ = nullptr;
    QPushButton* selectAll_ = nullptr;
    QList<CapabilityCheckGroup*> groups_;
    bool updatingCustomSize_ = false;
};
