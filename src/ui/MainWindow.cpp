#include "MainWindow.h"
#include "WorkflowWidget.h"
#include "pages/ProjectDetailsPage.h"
#include "pages/TemplatePage.h"
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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      projectModel_(this),
      templateManager_(this),
      generationServices_(this)
{
    setWindowTitle(QStringLiteral("AR&MF"));
    resize(1100, 700);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QHBoxLayout(central);

    workflow_ = new WorkflowWidget(central);
    workflow_->setFixedWidth(245);
    stack_ = new QStackedWidget(central);
    layout->addWidget(workflow_);
    layout->addWidget(stack_, 1);

    projectPage_ = new ProjectDetailsPage(&projectModel_, stack_);
    templatePage_ = new TemplatePage(&projectModel_, &templateManager_, stack_);
    developmentPage_ = new DevelopmentEnvironmentPage(&projectModel_, stack_);
    aiToolsPage_ = new AiToolsPage(&projectModel_, stack_);
    rulesPage_ = new RulesRoutingPage(stack_);
    memoryPage_ = new MemoryPage(stack_);
    resourcesPage_ = new ResourcesPage(&projectModel_, stack_);
    reviewPage_ = new ReviewPage(&projectModel_, stack_);
    generatePage_ = new GeneratePage(&projectModel_, &generationServices_, stack_);
    verificationPage_ = new VerificationPage(stack_);
    finalizePage_ = new FinalizePage(&projectModel_, stack_);

    const QList<QWidget*> pages{
        projectPage_, templatePage_, developmentPage_, aiToolsPage_, resourcesPage_,
        rulesPage_, memoryPage_, reviewPage_, generatePage_, verificationPage_,
        finalizePage_
    };
    for (QWidget* page : pages) {
        stack_->addWidget(page);
    }

    rulesPage_->setModel(&projectModel_);
    memoryPage_->setModel(&projectModel_);
    verificationPage_->setModel(&projectModel_);
    workflow_->setStepCount(stack_->count());

    connect(workflow_, &WorkflowWidget::stepSelected,
            this, &MainWindow::setWorkflowPage);
    connect(workflow_, &WorkflowWidget::backRequested,
            this, &MainWindow::goBack);
    connect(workflow_, &WorkflowWidget::forwardRequested,
            this, &MainWindow::goForward);

    workflow_->setCurrentStep(0);
}

void MainWindow::setWorkflowPage(int index)
{
    if (index >= 0 && index < stack_->count()) {
        stack_->setCurrentIndex(index);
    }
}

void MainWindow::goBack()
{
    setWorkflowPage(qMax(0, stack_->currentIndex() - 1));
    workflow_->setCurrentStep(stack_->currentIndex());
}

void MainWindow::goForward()
{
    setWorkflowPage(qMin(stack_->count() - 1, stack_->currentIndex() + 1));
    workflow_->setCurrentStep(stack_->currentIndex());
}
