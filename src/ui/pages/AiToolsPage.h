#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
class QCheckBox;
class AiToolsPage final:public QWidget{public:explicit AiToolsPage(ProjectModel*,QWidget* parent=nullptr);public slots:void refreshFromModel();private:ProjectModel*model_;QCheckBox*chatgpt_;QCheckBox*codex_;};
