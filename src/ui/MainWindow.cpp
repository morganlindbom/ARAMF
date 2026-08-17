#include "MainWindow.h"
#include "WorkflowWidget.h"
#include "pages/ProjectDetailsPage.h"
#include "pages/ProjectProfilePage.h"
#include "pages/TemplatePage.h"
#include "pages/CategoryPage.h"
#include "pages/GuiFirstPages.h"
#include "pages/DevelopmentEnvironmentPage.h"
#include "pages/AiToolsPage.h"
#include "pages/ResourcesPage.h"
#include "pages/ReviewPage.h"
#include "pages/GeneratePage.h"
#include "pages/FinalizePage.h"
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QWidget>
MainWindow::MainWindow(QWidget*parent):QMainWindow(parent),projectModel_(this),templateManager_(this),generationServices_(this){setWindowTitle(QStringLiteral("AR&MF"));resize(1100,700);auto*central=new QWidget(this);setCentralWidget(central);auto*layout=new QHBoxLayout(central);workflow_=new WorkflowWidget(central);workflow_->setFixedWidth(245);stack_=new QStackedWidget(central);layout->addWidget(workflow_);layout->addWidget(stack_,1);
projectPage_=new ProjectDetailsPage(&projectModel_,stack_);profilePage_=new ProjectProfilePage(&projectModel_,stack_);templatePage_=new TemplatePage(&projectModel_,&templateManager_,stack_);contextPage_=new CategoryPage(&projectModel_,tr("Context"),tr("Choose the project context from the canonical catalog."),{QStringLiteral("embedded-firmware"),QStringLiteral("desktop-application"),QStringLiteral("web-application"),QStringLiteral("thesis")},stack_);developmentPage_=new DevelopmentEnvironmentPage(&projectModel_,stack_);strategyPage_=new AiStrategyPage(stack_);aiToolsPage_=new AiToolsPage(&projectModel_,stack_);rulesPage_=new RulesRoutingPage(stack_);memoryPage_=new MemoryPage(stack_);resourcesPage_=new ResourcesPage(&projectModel_,stack_);reviewPage_=new ReviewPage(&projectModel_,stack_);generatePage_=new GeneratePage(&projectModel_,&generationServices_,stack_);verificationPage_=new VerificationPage(stack_);finalizePage_=new FinalizePage(&projectModel_,stack_);
 const QList<QWidget*> pages{projectPage_,profilePage_,templatePage_,contextPage_,developmentPage_,strategyPage_,aiToolsPage_,rulesPage_,memoryPage_,resourcesPage_,reviewPage_,generatePage_,verificationPage_,finalizePage_};
 for(QWidget* page: pages) stack_->addWidget(page);strategyPage_->setModel(&projectModel_);rulesPage_->setModel(&projectModel_);memoryPage_->setModel(&projectModel_);verificationPage_->setModel(&projectModel_);workflow_->setStepCount(stack_->count());connect(workflow_,&WorkflowWidget::stepSelected,this,&MainWindow::setWorkflowPage);connect(workflow_,&WorkflowWidget::backRequested,this,&MainWindow::goBack);connect(workflow_,&WorkflowWidget::forwardRequested,this,&MainWindow::goForward);connect(profilePage_,&ProjectProfilePage::profileChanged,this,&MainWindow::applyProfile);workflow_->setCurrentStep(0);applyProfile(projectModel_.profileSelections());}
void MainWindow::setWorkflowPage(int index){if(index>=0&&index<stack_->count())stack_->setCurrentIndex(index);}
void MainWindow::goBack(){setWorkflowPage(qMax(0,stack_->currentIndex()-1));workflow_->setCurrentStep(stack_->currentIndex());}
void MainWindow::goForward(){setWorkflowPage(qMin(stack_->count()-1,stack_->currentIndex()+1));workflow_->setCurrentStep(stack_->currentIndex());}
void MainWindow::applyProfile(const QStringList& profileIds){const bool embedded=profileIds.contains("embedded")||profileIds.contains("microcontroller");const bool research=profileIds.contains("research");const bool hasFrontend=profileIds.contains("frontend")||profileIds.contains("react");const bool hasBackend=profileIds.contains("backend")||profileIds.contains("rest")||profileIds.contains("node")||profileIds.contains("python-web");const bool hasTesting=profileIds.contains("unit-test")||profileIds.contains("integration-test")||profileIds.contains("e2e-test");workflow_->setStepEnabled(2,!research,tr("Templates are optional for research projects."));workflow_->setStepEnabled(4,true);workflow_->setStepEnabled(5,!embedded||hasFrontend,tr("No desktop, web or embedded UI target is selected."));workflow_->setStepEnabled(6,true);workflow_->setStepEnabled(7,hasFrontend||hasBackend||profileIds.contains("ai-assisted"),tr("Rules are shown when an AI-facing project surface is selected."));workflow_->setStepEnabled(8,!embedded,tr("Memory settings are specialized for embedded projects."));workflow_->setStepEnabled(9,!embedded||profileIds.contains("documentation"),tr("Resources are shown for documentation or non-embedded projects."));workflow_->setStepEnabled(10,true);workflow_->setStepEnabled(11,!research,tr("Generation is optional for a research profile."));workflow_->setStepEnabled(12,hasTesting||profileIds.contains("cicd"),tr("Select at least one testing or CI capability."));workflow_->setStepEnabled(13,true);}
