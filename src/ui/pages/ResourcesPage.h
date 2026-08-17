#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QListWidget;
class ResourcesPage final:public QWidget{public:explicit ResourcesPage(ProjectModel*,QWidget* parent=nullptr);public slots:void refreshFromModel();private:ProjectModel*model_;QListWidget*resources_;};
