#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QComboBox;
class DevelopmentEnvironmentPage final:public QWidget{public:explicit DevelopmentEnvironmentPage(ProjectModel*,QWidget* parent=nullptr);public slots:void refreshFromModel();private:ProjectModel*model_;QComboBox*ide_;QComboBox*compiler_;QComboBox*os_;QComboBox*target_;QComboBox*build_;};
