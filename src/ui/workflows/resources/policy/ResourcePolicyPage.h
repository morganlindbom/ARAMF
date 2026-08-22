#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class CapabilityCheckGroup;
class QComboBox;

class ResourcePolicyPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ResourcePolicyPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void persist();
    ProjectModel* model_ = nullptr;
    CapabilityCheckGroup* options_ = nullptr;
    QComboBox* loadingStrategy_ = nullptr;
};
