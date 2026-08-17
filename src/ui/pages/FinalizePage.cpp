#include "FinalizePage.h"
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
FinalizePage::FinalizePage(ProjectModel*,QWidget*p):QWidget(p),status_(new QLabel(this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(tr("<h2>Finalize</h2>Choose independent final actions; each can be run and reported separately."),this));auto*actions=new QGroupBox(tr("Final actions"),this);auto*actionLayout=new QVBoxLayout(actions);for(const auto&text:{tr("Deploy managed AGENTS.md to project root"),tr("Save custom template"),tr("Run external build verification"),tr("Run external test verification"),tr("Run launch verification"),tr("Create checkpoint"),tr("Record certification evidence")})actionLayout->addWidget(new QCheckBox(text,actions));l->addWidget(actions);l->addWidget(new QLabel(tr("Build: Not Run\nTests: Not Run\nLaunch: Not Run\n\nAGENTS deployment uses aramf/AGENTS.md and protects foreign files."),this));l->addWidget(status_);status_->setText(tr("Ready for explicit final actions."));l->addStretch();}
