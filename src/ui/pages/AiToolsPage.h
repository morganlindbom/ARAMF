#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QCheckBox; class QComboBox;
class AiToolsPage final:public QWidget{public:explicit AiToolsPage(ProjectModel*,QWidget* parent=nullptr);public slots:void refreshFromModel();private:ProjectModel*model_;QComboBox*primaryAgent_;QCheckBox*additionalAgent_;};
