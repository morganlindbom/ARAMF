#include "GeneratePage.h"
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>
GeneratePage::GeneratePage(ProjectModel*m,GenerationServices*s,QWidget*p):QWidget(p),model_(m),services_(s),result_(new QPlainTextEdit(this)){auto*l=new QVBoxLayout(this);l->addWidget(new QLabel(tr("<h2>Generate</h2>Select one or more output products, then generate explicitly to canonical aramf/."),this));auto*outputs=new QGroupBox(tr("Output products"),this);auto*outputLayout=new QVBoxLayout(outputs);for(const auto&text:{tr("AGENTS.md and generated rules"),tr("Routing manifests"),tr("Platform adapters"),tr("Resource manifest"),tr("Project Memory artifacts"),tr("Provenance and selection effects")}){auto*box=new QCheckBox(text,outputs);box->setChecked(true);outputLayout->addWidget(box);}l->addWidget(outputs);auto*b=new QPushButton(tr("Generate selected outputs"),this);l->addWidget(b);result_->setReadOnly(true);l->addWidget(result_);connect(b,&QPushButton::clicked,this,[this]{result_->setPlainText(services_->generate(*model_));});}
