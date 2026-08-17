#pragma once
#include <QWidget>
#include "core/Services.h"
class QListWidget;
class TemplatePage final : public QWidget { public: TemplatePage(ProjectModel*,TemplateManager*,QWidget* parent=nullptr); public slots: void refreshFromModel(); private: ProjectModel* model_; TemplateManager* manager_; QListWidget* templates_; };
