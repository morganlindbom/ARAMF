#pragma once

#include "core/Services.h"

#include <QWidget>

class QLabel;
class QPushButton;

class FinalizePage final : public QWidget
{
    Q_OBJECT
public:
    FinalizePage(ProjectModel* model, FinalizationServices* services,
                 AgentEntryPointService* entryPointServices, QWidget* parent = nullptr);

private:
    void refreshReadiness();
    ProjectModel* model_ = nullptr;
    FinalizationServices* services_ = nullptr;
    AgentEntryPointService* entryPointServices_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* blockers_ = nullptr;
    QPushButton* finalizeButton_ = nullptr;
    QPushButton* entryPointButton_ = nullptr;
    QLabel* entryPointResult_ = nullptr;
};
