#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QListWidget;
class CategoryPage final : public QWidget { public: CategoryPage(ProjectModel* model,const QString& title,const QString& description,QStringList options,QWidget* parent=nullptr); private: ProjectModel* model_; QListWidget* options_; };
