#include "ReviewPage.h"
#include <QTextBrowser>
#include <QLabel>
#include <QVBoxLayout>
ReviewPage::ReviewPage(ProjectModel*m,QWidget*p):QWidget(p),model_(m),summary_(new QTextBrowser(this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(tr("<h2>Review</h2>Read-only canonical ProjectModel summary"),this));l->addWidget(summary_);connect(model_,&ProjectModel::modelChanged,this,&ReviewPage::refreshFromModel);refreshFromModel();}
void ReviewPage::refreshFromModel(){const auto e=model_->developmentEnvironment();summary_->setPlainText(QStringLiteral("Project\n  Name: %1\n  ID: %2\n  Path: %3\n\nTemplate: %4\nContext: %5\n\nDevelopment Environment\n  IDE: %6\n  Compiler: %7\n  OS: %8\n  Target: %9\n  Build: %10\n\nAI Platforms: %11").arg(model_->projectName(),model_->projectId(),model_->projectPath(),model_->templateId(),model_->context(),e.ide,e.compiler,e.operatingSystem,e.targetPlatform,e.buildSystem,model_->aiPlatforms().join(", ")));}
