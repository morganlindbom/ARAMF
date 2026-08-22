#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QTextEdit;
class QWidget;

class ResourceInventoryPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ResourceInventoryPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void refreshDetails();
    void saveDetails();
    void addResource(const ProjectResource& resource);
    void updateStatus(ProjectResource& resource);

    ProjectModel* model_ = nullptr;
    QListWidget* resources_ = nullptr;
    QWidget* detailsEditor_ = nullptr;
    QLabel* detailsEmptyState_ = nullptr;
    QLineEdit* name_ = nullptr;
    QComboBox* type_ = nullptr;
    QLineEdit* location_ = nullptr;
    QTextEdit* description_ = nullptr;
    QCheckBox* enabled_ = nullptr;
    QComboBox* locationMode_ = nullptr;
};
