// MainWindow.cpp

#include "MainWindow.h"

#include "ui/workflow/WorkflowWidget.h"

#include "ui/workflows/project/setup/ProjectSetupPage.h"
#include "ui/workflows/project/academic/ProjectAcademicPage.h"
#include "ui/workflows/project/languages/ProjectLanguagesPage.h"
#include "ui/workflows/project/frameworks/ProjectFrameworksPage.h"
#include "ui/workflows/project/developmenttools/ProjectDevelopmentToolsPage.h"
#include "ui/workflows/project/platforms/ProjectPlatformsPage.h"
#include "ui/workflows/project/hardwarearchitecture/ProjectHardwareArchitecturePage.h"
#include "ui/workflows/project/builddelivery/ProjectBuildDeliveryPage.h"
#include "ui/workflows/ai/agents/AiAgentsPage.h"
#include "ui/workflows/ai/responsibilities/AiResponsibilitiesPage.h"
#include "ui/workflows/ai/autonomy/AiAutonomyPage.h"
#include "ui/workflows/ai/integration/AiIntegrationPage.h"
#include "ui/workflows/resources/inventory/ResourceInventoryPage.h"
#include "ui/workflows/resources/authority/ResourceAuthorityPage.h"
#include "ui/workflows/resources/policy/ResourcePolicyPage.h"

#include "ui/workflows/rules/selection/RuleSelectionPage.h"
#include "ui/workflows/rules/routing/RuleRoutingPage.h"
#include "ui/workflows/memory/capture/MemoryCapturePage.h"
#include "ui/workflows/memory/maintenance/MemoryMaintenancePage.h"

#include "ui/workflows/output/review/ReviewPage.h"
#include "ui/workflows/output/generate/GeneratePage.h"
#include "ui/workflows/output/verify/VerifyPage.h"
#include "ui/workflows/output/finalize/FinalizePage.h"

#include <QFrame>
#include <QApplication>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QList>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStackedWidget>
#include <QWheelEvent>
#include <QtMath>
#include <QWidget>

namespace
{

    QScreen *screenForIndex(int index)
    {
        /*Returns the requested physical screen.

        The screen index is zero-based. If the requested screen does not exist,
        the primary screen is returned as a safe fallback.
        */

        const QList<QScreen *> screens = QGuiApplication::screens();

        if (index >= 0 && index < screens.size())
        {
            return screens.at(index);
        }

        return QGuiApplication::primaryScreen();
    }

    const QList<WorkflowPageId> &workflowSequence()
    {
        /*Returns the canonical workflow navigation order.

        Keeping the workflow sequence in one location prevents Back and Forward
        navigation from maintaining separate copies of the page order.
        */

        static const QList<WorkflowPageId> sequence{
            WorkflowPageId::Setup,
            WorkflowPageId::Academic,
            WorkflowPageId::Languages,
            WorkflowPageId::Frameworks,
            WorkflowPageId::DevelopmentTools,
            WorkflowPageId::Platforms,
            WorkflowPageId::HardwareArchitecture,
            WorkflowPageId::BuildDelivery,
            WorkflowPageId::AiAgents,
            WorkflowPageId::AiResponsibilities,
            WorkflowPageId::AiAutonomy,
            WorkflowPageId::AiIntegration,
            WorkflowPageId::ResourceInventory,
            WorkflowPageId::ResourceAuthority,
            WorkflowPageId::ResourcePolicy,
            WorkflowPageId::RuleSelection,
            WorkflowPageId::RuleRouting,
            WorkflowPageId::MemoryCapture,
            WorkflowPageId::MemoryMaintenance,
            WorkflowPageId::Review,
            WorkflowPageId::Generate,
            WorkflowPageId::Verify,
            WorkflowPageId::Finalize};

        return sequence;
    }

}

MainWindow::MainWindow(
    int preferredScreenIndex,
    int preferredWindowWidth,
    int preferredWindowHeight,
    QWidget *parent)
    : QMainWindow(parent),
      preferredScreenIndex_(preferredScreenIndex),
      preferredWindowWidth_(preferredWindowWidth),
      preferredWindowHeight_(preferredWindowHeight),
      projectModel_(this),
      templateManager_(this),
      generationServices_(this)
{
    /*Creates and configures the ARAMF main application window.

    MainWindow owns the visual application shell, workflow navigation,
    shared scroll host, workflow pages and developer-defined startup
    placement supplied by main.cpp.
    */

    baseApplicationFont_ = QApplication::font();
    baseStyleSheet_ = styleSheet();
    setUiZoom(100);
    qApp->installEventFilter(this);

    auto *zoomInShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    auto *zoomInAlternateShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    auto *zoomOutShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    auto *resetZoomShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);

    connect(zoomInShortcut, &QShortcut::activated,
            this, &MainWindow::zoomIn);
    connect(zoomInAlternateShortcut, &QShortcut::activated,
            this, &MainWindow::zoomIn);
    connect(zoomOutShortcut, &QShortcut::activated,
            this, &MainWindow::zoomOut);
    connect(resetZoomShortcut, &QShortcut::activated,
            this, &MainWindow::resetZoom);

    setWindowTitle(QStringLiteral("AR&MF"));
    setMinimumSize(760, 480);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *layout = new QHBoxLayout(central);

    workflow_ = new WorkflowWidget(central);
    workflow_->setFixedWidth(280);

    pageScroll_ = new QScrollArea(central);
    pageScroll_->setWidgetResizable(true);
    pageScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pageScroll_->setFrameShape(QFrame::NoFrame);

    stack_ = new QStackedWidget;
    pageScroll_->setWidget(stack_);

    layout->addWidget(workflow_);
    layout->addWidget(pageScroll_, 1);

    projectPage_ =
        new ProjectSetupPage(
            &projectModel_,
            &templateManager_,
            &projectPersistence_,
            stack_);

    academicPage_ = new ProjectAcademicPage(&projectModel_, stack_);

    languagesPage_ =
        new ProjectLanguagesPage(
            &projectModel_,
            stack_);

    frameworksPage_ =
        new ProjectFrameworksPage(
            &projectModel_,
            stack_);

    developmentToolsPage_ =
        new ProjectDevelopmentToolsPage(
            &projectModel_,
            stack_);

    platformsPage_ =
        new ProjectPlatformsPage(
            &projectModel_,
            stack_);

    hardwareArchitecturePage_ =
        new ProjectHardwareArchitecturePage(
            &projectModel_,
            stack_);

    buildDeliveryPage_ =
        new ProjectBuildDeliveryPage(
            &projectModel_,
            stack_);

    aiAgentsPage_ = new AiAgentsPage(&projectModel_, stack_);
    aiResponsibilitiesPage_ = new AiResponsibilitiesPage(&projectModel_, stack_);
    aiAutonomyPage_ = new AiAutonomyPage(&projectModel_, stack_);
    aiIntegrationPage_ = new AiIntegrationPage(&projectModel_, stack_);

    resourceInventoryPage_ = new ResourceInventoryPage(&projectModel_, stack_);
    resourceAuthorityPage_ = new ResourceAuthorityPage(&projectModel_, stack_);
    resourcePolicyPage_ = new ResourcePolicyPage(&projectModel_, stack_);

    ruleSelectionPage_ = new RuleSelectionPage(&projectModel_, stack_);
    ruleRoutingPage_ = new RuleRoutingPage(&projectModel_, stack_);
    memoryCapturePage_ = new MemoryCapturePage(&projectModel_, stack_);
    memoryMaintenancePage_ = new MemoryMaintenancePage(&projectModel_, stack_);

    reviewPage_ =
        new ReviewPage(
            &projectModel_,
            stack_);

    generatePage_ =
        new GeneratePage(
            &projectModel_,
            projectPage_,
            &generationServices_,
            stack_);

    verificationPage_ =
        new VerifyPage(&projectModel_, &verificationServices_, stack_);

    finalizePage_ =
        new FinalizePage(
            &projectModel_,
            &finalizationServices_,
            &agentEntryPointService_,
            stack_);

    const auto registerPage =
        [this](WorkflowPageId id, QWidget *page)
    {
        pageStackIndices_.insert(
            id,
            stack_->addWidget(page));
    };

    registerPage(WorkflowPageId::Setup, projectPage_);
    registerPage(WorkflowPageId::Academic, academicPage_);
    registerPage(WorkflowPageId::Languages, languagesPage_);
    registerPage(WorkflowPageId::Frameworks, frameworksPage_);
    registerPage(WorkflowPageId::DevelopmentTools, developmentToolsPage_);
    registerPage(WorkflowPageId::Platforms, platformsPage_);
    registerPage(WorkflowPageId::HardwareArchitecture, hardwareArchitecturePage_);
    registerPage(WorkflowPageId::BuildDelivery, buildDeliveryPage_);
    registerPage(WorkflowPageId::AiAgents, aiAgentsPage_);
    registerPage(WorkflowPageId::AiResponsibilities, aiResponsibilitiesPage_);
    registerPage(WorkflowPageId::AiAutonomy, aiAutonomyPage_);
    registerPage(WorkflowPageId::AiIntegration, aiIntegrationPage_);
    registerPage(WorkflowPageId::ResourceInventory, resourceInventoryPage_);
    registerPage(WorkflowPageId::ResourceAuthority, resourceAuthorityPage_);
    registerPage(WorkflowPageId::ResourcePolicy, resourcePolicyPage_);
    registerPage(WorkflowPageId::RuleSelection, ruleSelectionPage_);
    registerPage(WorkflowPageId::RuleRouting, ruleRoutingPage_);
    registerPage(WorkflowPageId::MemoryCapture, memoryCapturePage_);
    registerPage(WorkflowPageId::MemoryMaintenance, memoryMaintenancePage_);
    registerPage(WorkflowPageId::Review, reviewPage_);
    registerPage(WorkflowPageId::Generate, generatePage_);
    registerPage(WorkflowPageId::Verify, verificationPage_);
    registerPage(WorkflowPageId::Finalize, finalizePage_);

    workflow_->setStepCount(stack_->count());

    connect(
        workflow_,
        &WorkflowWidget::pageSelected,
        this,
        &MainWindow::setWorkflowPage);

    connect(
        workflow_,
        &WorkflowWidget::backRequested,
        this,
        &MainWindow::goBack);

    connect(
        workflow_,
        &WorkflowWidget::forwardRequested,
        this,
        &MainWindow::goForward);

    workflow_->setCurrentPage(currentPage_);

    placeOnPreferredScreen();
}

void MainWindow::setWorkflowPage(WorkflowPageId page)
{
    /*Activates the requested workflow page.

    The page identifier is explicitly mapped to its QStackedWidget index,
    avoiding any dependency on navigation row numbers or heading positions.
    The shared vertical scroll position is reset when a new page is opened.
    */

    const auto pageIndex = pageStackIndices_.constFind(page);

    if (pageIndex == pageStackIndices_.constEnd())
    {
        return;
    }

    currentPage_ = page;

    stack_->setCurrentIndex(pageIndex.value());

    if (pageScroll_->verticalScrollBar())
    {
        pageScroll_->verticalScrollBar()->setValue(0);
    }

    workflow_->setCurrentPage(page);
}

void MainWindow::placeOnPreferredScreen()
{
    /*Places MainWindow on the screen requested by main.cpp.

    The requested size is restricted to the selected screen's usable
    geometry and the window is centered. An invalid screen index safely
    falls back to the operating system's primary screen.
    */

    QScreen *screen = screenForIndex(preferredScreenIndex_);

    if (!screen)
    {
        return;
    }

    const QRect available = screen->availableGeometry();

    const int windowWidth =
        qMin(preferredWindowWidth_, available.width());

    const int windowHeight =
        qMin(preferredWindowHeight_, available.height());

    resize(windowWidth, windowHeight);

    const int windowX =
        available.left() +
        (available.width() - windowWidth) / 2;

    const int windowY =
        available.top() +
        (available.height() - windowHeight) / 2 - 10;

    move(windowX, windowY);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);

    if (widget && (widget == this || isAncestorOf(widget)) &&
        event->type() == QEvent::Wheel)
    {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);

        if (wheelEvent->modifiers().testFlag(Qt::ControlModifier))
        {
            const int delta = wheelEvent->angleDelta().y() != 0
                                  ? wheelEvent->angleDelta().y()
                                  : wheelEvent->pixelDelta().y();

            if (delta > 0)
            {
                zoomIn();
            }
            else if (delta < 0)
            {
                zoomOut();
            }

            wheelEvent->accept();
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setUiZoom(int percent)
{
    uiZoomPercent_ = qBound(30, percent, 150);

    const double zoomFactor = uiZoomPercent_ / 100.0;
    QFont scaledFont = baseApplicationFont_;
    scaledFont.setPointSizeF(
        baseApplicationFont_.pointSizeF() * zoomFactor);
    QApplication::setFont(scaledFont);
    setFont(scaledFont);

    constexpr int baseIndicatorSize = 16;
    constexpr int minimumIndicatorSize = 5;
    const int indicatorSize = qMax(
        minimumIndicatorSize,
        qRound(baseIndicatorSize * zoomFactor));
    const QString zoomStyle = QStringLiteral(
                                  "\nQCheckBox::indicator { width: %1px; height: %1px; }\n")
                                  .arg(indicatorSize);
    setStyleSheet(baseStyleSheet_ + zoomStyle);

    updateGeometry();

    if (pageScroll_)
    {
        pageScroll_->viewport()->updateGeometry();
    }
}

void MainWindow::zoomIn()
{
    setUiZoom(uiZoomPercent_ + 10);
}

void MainWindow::zoomOut()
{
    setUiZoom(uiZoomPercent_ - 10);
}

void MainWindow::resetZoom()
{
    setUiZoom(100);
}

void MainWindow::goBack()
{
    /*Navigates to the previous selectable workflow page.

    Workflow headings are not part of the sequence because navigation uses
    stable WorkflowPageId values rather than visible navigation rows.
    */

    const QList<WorkflowPageId> &sequence = workflowSequence();
    const int position = sequence.indexOf(currentPage_);

    if (position > 0)
    {
        setWorkflowPage(sequence.at(position - 1));
    }
}

void MainWindow::goForward()
{
    /*Navigates to the next selectable workflow page.

    Navigation stops at Finalize and follows the single canonical workflow
    sequence shared with Back navigation.
    */

    const QList<WorkflowPageId> &sequence = workflowSequence();
    const int position = sequence.indexOf(currentPage_);

    if (position >= 0 &&
        position + 1 < sequence.size())
    {

        setWorkflowPage(sequence.at(position + 1));
    }
}
