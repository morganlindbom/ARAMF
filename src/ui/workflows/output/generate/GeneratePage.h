#pragma once
#include <QWidget>
#include "core/ProjectModel.h"
#include "core/Services.h"
class QPlainTextEdit;
class GeneratePage final:public QWidget{public:GeneratePage(ProjectModel*,GenerationServices*,QWidget* parent=nullptr);private:ProjectModel*model_;GenerationServices*services_;QPlainTextEdit*result_;};
