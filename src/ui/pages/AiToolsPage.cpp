#include "AiToolsPage.h"
#include <QCheckBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>
AiToolsPage::AiToolsPage(ProjectModel*m,QWidget*p):QWidget(p),model_(m),chatgpt_(new QCheckBox(tr("ChatGPT"),this)),codex_(new QCheckBox(tr("OpenAI Codex"),this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(tr("<h2>AI Tools / Platforms</h2>Selections are stored in ProjectModel."),this));l->addWidget(chatgpt_);l->addWidget(codex_);l->addStretch();auto update=[this]{QStringList p;if(chatgpt_->isChecked())p<<"chatgpt";if(codex_->isChecked())p<<"openai-codex";model_->setAiPlatforms(p);};connect(chatgpt_,&QCheckBox::toggled,this,[update](bool){update();});connect(codex_,&QCheckBox::toggled,this,[update](bool){update();});connect(model_,&ProjectModel::aiPlatformsChanged,this,&AiToolsPage::refreshFromModel);refreshFromModel();}
void AiToolsPage::refreshFromModel(){const QSignalBlocker a(chatgpt_),b(codex_);const auto p=model_->aiPlatforms();chatgpt_->setChecked(p.contains("chatgpt"));codex_->setChecked(p.contains("openai-codex"));}
