#pragma once

#include <QWidget>

#include "core/ProjectModel.h"
#include "core/Services.h"

class QComboBox;
class QGroupBox;
class QCheckBox;
class QWidget;
class QLabel;
class QPushButton;
class QResizeEvent;

class TemplateSelector final : public QWidget {
public:
    TemplateSelector(ProjectModel* model, TemplateManager* manager, QWidget* parent = nullptr);

private slots:
    void refreshFromModel();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ProjectModel* model_;
    TemplateManager* manager_;
    QComboBox* selector_;
    QGroupBox* moduleFrame_;
    QWidget* moduleGrid_;
    QList<QCheckBox*> moduleChecks_;
    QGroupBox* templateFrame_;
    QWidget* templateGrid_;
    QList<QCheckBox*> templateChecks_;
    QLabel* status_;
    QPushButton* remove_;
    void reflowGrids();
    int moduleColumns_ = 0;
    int templateColumns_ = 0;
};
