#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class ProjectModel;
class ReleaseManagementService;

class ProductVersionPage final : public QWidget
{
    Q_OBJECT
public:
    ProductVersionPage(ProjectModel* model, ReleaseManagementService* service,
                       QWidget* parent = nullptr);

private:
    void refresh();
    ProjectModel* model_;
    ReleaseManagementService* service_;
    QLabel* version_;
    QLabel* mode_;
    QComboBox* target_;
};

