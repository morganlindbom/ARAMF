// MainWindow.cpp

#include "MainWindow.h"

#include "ui/workflow/WorkflowWidget.h"

#include "ui/workflows/project/setup/ProjectSetupPage.h"
#include "ui/workflows/project/overview/ProjectOverviewPage.h"
#include "ui/workflows/project/modulestemplates/ProjectModulesTemplatesPage.h"
#include "ui/workflows/project/communication/ProjectCommunicationPage.h"
#include "ui/workflows/project/compatibility/ProjectCompatibilityPage.h"
#include "ui/workflows/project/academic/ProjectAcademicPage.h"
#include "ui/shared/ParentOverview.h"
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
#include "ui/workflows/update/review/FrameworkKnowledgeReviewPage.h"
#include "ui/workflows/update/apply/FrameworkKnowledgeApplyPage.h"
#include "ui/workflows/update/backlog/ImprovementBacklogPage.h"
#include "ui/workflows/update/configuration/UpdateConfigurationPage.h"
#include "ui/workflows/release/overview/ReleaseOverviewPage.h"
#include "ui/workflows/release/productversion/ProductVersionPage.h"
#include "ui/workflows/release/componentversions/ComponentVersionsPage.h"
#include "ui/workflows/release/schema/SchemaCompatibilityPage.h"
#include "ui/workflows/release/readiness/ReleaseReadinessPage.h"
#include "ui/workflows/release/history/ApprovalHistoryPage.h"
#include "ui/shared/PageSupport.h"
#include "ui/shared/FooterProgressDisplay.h"

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
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>
#include <QWidget>

namespace
{

    constexpr int kWorkflowFooterControlHeight = 40;

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
            WorkflowPageId::ProjectIdentity,
            WorkflowPageId::ProjectModulesTemplates,
            WorkflowPageId::ProjectCommunication,
            WorkflowPageId::ProjectCompatibility,
            WorkflowPageId::Academic,
            WorkflowPageId::AcademicDocumentation,
            WorkflowPageId::AcademicResearch,
            WorkflowPageId::AcademicInformation,
            WorkflowPageId::AcademicStandards,
            WorkflowPageId::AcademicDeliverables,
            WorkflowPageId::Languages,
            WorkflowPageId::Frameworks,
            WorkflowPageId::DevelopmentTools,
            WorkflowPageId::DevelopmentToolsIde,
            WorkflowPageId::DevelopmentToolsVersionControl,
            WorkflowPageId::DevelopmentToolsSupport,
            WorkflowPageId::Platforms,
            WorkflowPageId::PlatformsHosts,
            WorkflowPageId::PlatformsTargets,
            WorkflowPageId::HardwareArchitecture,
            WorkflowPageId::HardwareArchitectures,
            WorkflowPageId::HardwareProcessors,
            WorkflowPageId::HardwareTargets,
            WorkflowPageId::BuildDelivery,
            WorkflowPageId::BuildDeliveryToolchains,
            WorkflowPageId::BuildDeliveryBuildSystems,
            WorkflowPageId::BuildDeliveryTesting,
            WorkflowPageId::BuildDeliveryAutomation,
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
            WorkflowPageId::ReleaseOverview,
            WorkflowPageId::ProductVersion,
            WorkflowPageId::ComponentVersions,
            WorkflowPageId::SchemaCompatibility,
            WorkflowPageId::ReleaseReadiness,
            WorkflowPageId::ApprovalHistory,
            WorkflowPageId::Review,
            WorkflowPageId::Generate,
            WorkflowPageId::Verify,
            WorkflowPageId::Finalize,
            WorkflowPageId::UpdateReview,
            WorkflowPageId::UpdateApply,
            WorkflowPageId::ImprovementBacklog};

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
      generationServices_(this),
      releaseManagementService_(this)
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
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    workflow_ = new WorkflowWidget(central);
    workflow_->setMinimumWidth(220);
    workflow_->setMaximumWidth(340);
    workflow_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    pageScroll_ = new QScrollArea(central);
    pageScroll_->setObjectName(QStringLiteral("workflowPageScroll"));
    // Keep the content widget at its natural height.  Its width is updated
    // to the viewport width by refreshCurrentWorkflowLayout(), so the scroll host never
    // stretches compact page controls vertically just to fill the viewport.
    pageScroll_->setWidgetResizable(false);
    pageScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pageScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pageScroll_->setFrameShape(QFrame::NoFrame);
    pageScroll_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    pageScroll_->setMinimumSize(0, 0);
    pageScroll_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    stack_ = new QStackedWidget;
    // The scroll viewport owns horizontal width. Do not let the widest page
    // size hint turn into a minimum width for the entire stacked host.
    stack_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    stack_->setMinimumSize(0, 0);
    pageScroll_->setWidget(stack_);
    connect(stack_, &QStackedWidget::currentChanged, this, [this] {
        refreshCurrentWorkflowLayout();
        QTimer::singleShot(0, this, &MainWindow::refreshCurrentWorkflowLayout);
    });

    auto* pageArea = new QWidget(central);
    pageArea->setMinimumSize(0, 0);
    pageArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* pageAreaLayout = new QVBoxLayout(pageArea);
    pageAreaLayout->setContentsMargins(0, 0, 0, 0);
    pageAreaLayout->setSpacing(0);
    pageAreaLayout->addWidget(pageScroll_, 1);
    auto* footer = new QFrame(pageArea);
    footer->setObjectName(QStringLiteral("workflowPageFooter"));
    footer->setFrameShape(QFrame::StyledPanel);
    footer->setFrameShadow(QFrame::Plain);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(10, 8, 10, 8);
    saveWork_ = new QPushButton(tr("Save Work"), footer);
    saveWork_->setObjectName(QStringLiteral("saveWork"));
    saveWork_->setFixedHeight(kWorkflowFooterControlHeight);
    footerLayout->addWidget(saveWork_);
    completionProgress_ = new FooterProgressDisplay(footer);
    completionProgress_->setObjectName(QStringLiteral("setupProgress"));
    completionProgress_->setRange(0, 100);
    completionProgress_->setFormat(tr("%p% of your setup is done"));
    completionProgress_->setAlignment(Qt::AlignCenter);
    completionProgress_->setFixedHeight(kWorkflowFooterControlHeight);
    completionProgress_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    footerLayout->addWidget(completionProgress_, 1);
    pageAreaLayout->addWidget(footer, 0);
    layout->addWidget(workflow_);
    layout->addWidget(pageArea, 1);

    projectOverviewPage_ = new ProjectOverviewPage(stack_);

    projectPage_ =
        new ProjectSetupPage(
            &projectModel_,
            &templateManager_,
            &projectPersistence_,
            stack_);

    projectModulesTemplatesPage_ = new ProjectModulesTemplatesPage(
        &projectModel_, &templateManager_, stack_);
    projectCommunicationPage_ = new ProjectCommunicationPage(&projectModel_, stack_);
    projectCompatibilityPage_ = new ProjectCompatibilityPage(&projectModel_, stack_);

    academicPage_ = new ParentOverviewPage(
        tr("Academic"), tr("Configure academic documents, research and delivery requirements."),
        {{QStringLiteral("project.academic.documentation"), QStringLiteral("2.1"), tr("Documentation"),
          tr("Choose which academic documents this project produces.")},
         {QStringLiteral("project.academic.research"), QStringLiteral("2.2"), tr("Thesis & research"),
          tr("Set the thesis level, approach and research methods.")},
         {QStringLiteral("project.academic.information"), QStringLiteral("2.3"), tr("Academic information"),
          tr("Record the institution, programme, supervisor and examiner.")},
         {QStringLiteral("project.academic.standards"), QStringLiteral("2.4"), tr("Standards & languages"),
          tr("Choose a citation style and document languages.")},
         {QStringLiteral("project.academic.deliverables"), QStringLiteral("2.5"), tr("Requirements & deliverables"),
          tr("Select academic requirements and outputs.")}}, stack_);
    academicDocumentationPage_ = new ProjectAcademicPage(&projectModel_, stack_, ProjectAcademicPage::Section::Documentation);
    academicResearchPage_ = new ProjectAcademicPage(&projectModel_, stack_, ProjectAcademicPage::Section::Research);
    academicInformationPage_ = new ProjectAcademicPage(&projectModel_, stack_, ProjectAcademicPage::Section::Information);
    academicStandardsPage_ = new ProjectAcademicPage(&projectModel_, stack_, ProjectAcademicPage::Section::Standards);
    academicDeliverablesPage_ = new ProjectAcademicPage(&projectModel_, stack_, ProjectAcademicPage::Section::Deliverables);

    languagesPage_ =
        new ProjectLanguagesPage(
            &projectModel_,
            stack_);

    frameworksPage_ =
        new ProjectFrameworksPage(
            &projectModel_,
            stack_);

    developmentToolsPage_ = new ParentOverviewPage(
        tr("Development tools"), tr("Configure the editors, version control and support tools used by the project."),
        {{QStringLiteral("project.development-tools.ide"), QStringLiteral("5.1"), tr("IDE & editors"),
          tr("Select the editors used to develop the project.")},
         {QStringLiteral("project.development-tools.version-control"), QStringLiteral("5.2"), tr("Version control"),
          tr("Choose how project history and source changes are managed.")},
         {QStringLiteral("project.development-tools.support"), QStringLiteral("5.3"), tr("Development support"),
          tr("Select debuggers, analyzers, SDKs and related tools.")}}, stack_);
    developmentToolsIdePage_ = new ProjectDevelopmentToolsPage(
        &projectModel_, stack_, ProjectDevelopmentToolsPage::Section::Ide);
    developmentToolsVersionControlPage_ = new ProjectDevelopmentToolsPage(
        &projectModel_, stack_, ProjectDevelopmentToolsPage::Section::VersionControl);
    developmentToolsSupportPage_ = new ProjectDevelopmentToolsPage(
        &projectModel_, stack_, ProjectDevelopmentToolsPage::Section::Support);

    platformsPage_ = new ParentOverviewPage(
        tr("Platforms"), tr("Define where the project is developed and where it runs."),
        {{QStringLiteral("project.platforms.hosts"), QStringLiteral("6.1"), tr("Host operating systems"),
          tr("Select the operating systems used to develop or build the project.")},
         {QStringLiteral("project.platforms.targets"), QStringLiteral("6.2"), tr("Target platforms"),
          tr("Select the platforms supported by the finished project.")}}, stack_);
    platformsHostsPage_ = new ProjectPlatformsPage(
        &projectModel_, stack_, ProjectPlatformsPage::Section::Hosts);
    platformsTargetsPage_ = new ProjectPlatformsPage(
        &projectModel_, stack_, ProjectPlatformsPage::Section::Targets);

    hardwareArchitecturePage_ = new ParentOverviewPage(
        tr("Hardware & architecture"), tr("Describe processor architecture and physical deployment targets."),
        {{QStringLiteral("project.hardware-architecture.architectures"), QStringLiteral("7.1"), tr("Target architectures"),
          tr("Choose the instruction-set architectures targeted by the project.")},
         {QStringLiteral("project.hardware-architecture.processors"), QStringLiteral("7.2"), tr("Processor families"),
          tr("Choose MCU or processor families used by the project.")},
         {QStringLiteral("project.hardware-architecture.targets"), QStringLiteral("7.3"), tr("Hardware & deployment targets"),
          tr("Select boards and physical deployment targets.")}}, stack_);
    hardwareArchitecturesPage_ = new ProjectHardwareArchitecturePage(
        &projectModel_, stack_, ProjectHardwareArchitecturePage::Section::Architectures);
    hardwareProcessorsPage_ = new ProjectHardwareArchitecturePage(
        &projectModel_, stack_, ProjectHardwareArchitecturePage::Section::Processors);
    hardwareTargetsPage_ = new ProjectHardwareArchitecturePage(
        &projectModel_, stack_, ProjectHardwareArchitecturePage::Section::Targets);

    buildDeliveryPage_ = new ParentOverviewPage(
        tr("Build, test & delivery"), tr("Configure the toolchain, build process, testing and delivery pipeline."),
        {{QStringLiteral("project.build-delivery.toolchains"), QStringLiteral("8.1"), tr("Compiler & toolchain"),
          tr("Select the compiler, toolchain and runtime used to build the project.")},
         {QStringLiteral("project.build-delivery.build-systems"), QStringLiteral("8.2"), tr("Build systems & dependencies"),
          tr("Choose build systems, package management and build configurations.")},
         {QStringLiteral("project.build-delivery.testing"), QStringLiteral("8.3"), tr("Testing & quality"),
          tr("Select testing and quality analysis capabilities.")},
         {QStringLiteral("project.build-delivery.automation"), QStringLiteral("8.4"), tr("Automation & delivery"),
          tr("Configure automation and the formats used to deliver releases.")}}, stack_);
    buildDeliveryToolchainsPage_ = new ProjectBuildDeliveryPage(
        &projectModel_, stack_, ProjectBuildDeliveryPage::Section::Toolchains);
    buildDeliveryBuildSystemsPage_ = new ProjectBuildDeliveryPage(
        &projectModel_, stack_, ProjectBuildDeliveryPage::Section::BuildSystems);
    buildDeliveryTestingPage_ = new ProjectBuildDeliveryPage(
        &projectModel_, stack_, ProjectBuildDeliveryPage::Section::Testing);
    buildDeliveryAutomationPage_ = new ProjectBuildDeliveryPage(
        &projectModel_, stack_, ProjectBuildDeliveryPage::Section::Automation);

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

    updateReviewPage_ = new FrameworkKnowledgeReviewPage(&projectModel_, stack_);
    updateApplyPage_ = new FrameworkKnowledgeApplyPage(&projectModel_, stack_);
    updateConfigurationPage_ = new UpdateConfigurationPage(&projectModel_, projectPage_, &generationServices_, stack_);
    improvementBacklogPage_ = new ImprovementBacklogPage(&projectModel_, stack_);
    releaseOverviewPage_ = new ReleaseOverviewPage(stack_);
    productVersionPage_ = new ProductVersionPage(&projectModel_, &releaseManagementService_, stack_);
    componentVersionsPage_ = new ComponentVersionsPage(&releaseManagementService_, stack_);
    schemaCompatibilityPage_ = new SchemaCompatibilityPage(&projectModel_, &releaseManagementService_, stack_);
    releaseReadinessPage_ = new ReleaseReadinessPage(&projectModel_, &releaseManagementService_, stack_);
    approvalHistoryPage_ = new ApprovalHistoryPage(&releaseManagementService_, stack_);

    const auto registerPage =
        [this](WorkflowPageId id, QWidget *page)
    {
        pageStackIndices_.insert(
            id,
            stack_->addWidget(page));
    };

    // Setup remains the stable compatibility route for the former page 1;
    // the visible page is now the small parent overview.
    registerPage(WorkflowPageId::Setup, projectOverviewPage_);
    registerPage(WorkflowPageId::ProjectIdentity, projectPage_);
    registerPage(WorkflowPageId::ProjectModulesTemplates, projectModulesTemplatesPage_);
    registerPage(WorkflowPageId::ProjectCommunication, projectCommunicationPage_);
    registerPage(WorkflowPageId::ProjectCompatibility, projectCompatibilityPage_);
    registerPage(WorkflowPageId::Academic, academicPage_);
    registerPage(WorkflowPageId::AcademicDocumentation, academicDocumentationPage_);
    registerPage(WorkflowPageId::AcademicResearch, academicResearchPage_);
    registerPage(WorkflowPageId::AcademicInformation, academicInformationPage_);
    registerPage(WorkflowPageId::AcademicStandards, academicStandardsPage_);
    registerPage(WorkflowPageId::AcademicDeliverables, academicDeliverablesPage_);
    registerPage(WorkflowPageId::Languages, languagesPage_);
    registerPage(WorkflowPageId::Frameworks, frameworksPage_);
    registerPage(WorkflowPageId::DevelopmentTools, developmentToolsPage_);
    registerPage(WorkflowPageId::DevelopmentToolsIde, developmentToolsIdePage_);
    registerPage(WorkflowPageId::DevelopmentToolsVersionControl, developmentToolsVersionControlPage_);
    registerPage(WorkflowPageId::DevelopmentToolsSupport, developmentToolsSupportPage_);
    registerPage(WorkflowPageId::Platforms, platformsPage_);
    registerPage(WorkflowPageId::PlatformsHosts, platformsHostsPage_);
    registerPage(WorkflowPageId::PlatformsTargets, platformsTargetsPage_);
    registerPage(WorkflowPageId::HardwareArchitecture, hardwareArchitecturePage_);
    registerPage(WorkflowPageId::HardwareArchitectures, hardwareArchitecturesPage_);
    registerPage(WorkflowPageId::HardwareProcessors, hardwareProcessorsPage_);
    registerPage(WorkflowPageId::HardwareTargets, hardwareTargetsPage_);
    registerPage(WorkflowPageId::BuildDelivery, buildDeliveryPage_);
    registerPage(WorkflowPageId::BuildDeliveryToolchains, buildDeliveryToolchainsPage_);
    registerPage(WorkflowPageId::BuildDeliveryBuildSystems, buildDeliveryBuildSystemsPage_);
    registerPage(WorkflowPageId::BuildDeliveryTesting, buildDeliveryTestingPage_);
    registerPage(WorkflowPageId::BuildDeliveryAutomation, buildDeliveryAutomationPage_);
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
    registerPage(WorkflowPageId::UpdateReview, updateReviewPage_);
    registerPage(WorkflowPageId::UpdateApply, updateApplyPage_);
    registerPage(WorkflowPageId::UpdateConfiguration, updateConfigurationPage_);
    registerPage(WorkflowPageId::ImprovementBacklog, improvementBacklogPage_);
    registerPage(WorkflowPageId::ReleaseOverview, releaseOverviewPage_);
    registerPage(WorkflowPageId::ProductVersion, productVersionPage_);
    registerPage(WorkflowPageId::ComponentVersions, componentVersionsPage_);
    registerPage(WorkflowPageId::SchemaCompatibility, schemaCompatibilityPage_);
    registerPage(WorkflowPageId::ReleaseReadiness, releaseReadinessPage_);
    registerPage(WorkflowPageId::ApprovalHistory, approvalHistoryPage_);

    // All pages share the same scroll host.  Normalize their horizontal
    // policies once at the shell boundary so every workflow page follows the
    // viewport width without duplicating resize logic in each page.
    for (int index = 0; index < stack_->count(); ++index)
        AramfUi::normalizeWorkflowPage(stack_->widget(index));

    workflow_->setStepCount(stack_->count());
    workflow_->setCompletionModel(&projectModel_);
    connect(&projectModel_, &ProjectModel::modelChanged, this, [this] {
        refreshCompletionProgress();
        // Page refreshes may rebuild content after the model signal returns.
        // Run the same shell synchronization used at cold start after that
        // deferred layout work has settled.
        QTimer::singleShot(0, this, &MainWindow::refreshCurrentWorkflowLayout);
    });

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

    connect(saveWork_, &QPushButton::clicked, this, &MainWindow::saveWork);
    connect(projectOverviewPage_, &ParentOverviewPage::cardActivated, this,
            [this](const QString& id) {
                if (id == QStringLiteral("project.file-worker"))
                    setWorkflowPage(WorkflowPageId::ProjectIdentity);
                else if (id == QStringLiteral("project.modules-templates"))
                    setWorkflowPage(WorkflowPageId::ProjectModulesTemplates);
                else if (id == QStringLiteral("project.communication"))
                    setWorkflowPage(WorkflowPageId::ProjectCommunication);
                else if (id == QStringLiteral("project.compatibility"))
                    setWorkflowPage(WorkflowPageId::ProjectCompatibility);
            });
    connect(academicPage_, &ParentOverviewPage::cardActivated, this,
            [this](const QString& id) {
                if (id == QStringLiteral("project.academic.documentation"))
                    setWorkflowPage(WorkflowPageId::AcademicDocumentation);
                else if (id == QStringLiteral("project.academic.research"))
                    setWorkflowPage(WorkflowPageId::AcademicResearch);
                else if (id == QStringLiteral("project.academic.information"))
                    setWorkflowPage(WorkflowPageId::AcademicInformation);
                else if (id == QStringLiteral("project.academic.standards"))
                    setWorkflowPage(WorkflowPageId::AcademicStandards);
                else if (id == QStringLiteral("project.academic.deliverables"))
                    setWorkflowPage(WorkflowPageId::AcademicDeliverables);
            });
    const auto connectOverview = [this](ParentOverviewPage* overview,
                                        const QMap<QString, WorkflowPageId>& destinations) {
        connect(overview, &ParentOverviewPage::cardActivated, this,
                [this, destinations](const QString& id) {
                    const auto target = destinations.constFind(id);
                    if (target != destinations.constEnd()) setWorkflowPage(target.value());
                });
    };
    connectOverview(developmentToolsPage_, {
        {QStringLiteral("project.development-tools.ide"), WorkflowPageId::DevelopmentToolsIde},
        {QStringLiteral("project.development-tools.version-control"), WorkflowPageId::DevelopmentToolsVersionControl},
        {QStringLiteral("project.development-tools.support"), WorkflowPageId::DevelopmentToolsSupport}});
    connectOverview(platformsPage_, {
        {QStringLiteral("project.platforms.hosts"), WorkflowPageId::PlatformsHosts},
        {QStringLiteral("project.platforms.targets"), WorkflowPageId::PlatformsTargets}});
    connectOverview(hardwareArchitecturePage_, {
        {QStringLiteral("project.hardware-architecture.architectures"), WorkflowPageId::HardwareArchitectures},
        {QStringLiteral("project.hardware-architecture.processors"), WorkflowPageId::HardwareProcessors},
        {QStringLiteral("project.hardware-architecture.targets"), WorkflowPageId::HardwareTargets}});
    connectOverview(buildDeliveryPage_, {
        {QStringLiteral("project.build-delivery.toolchains"), WorkflowPageId::BuildDeliveryToolchains},
        {QStringLiteral("project.build-delivery.build-systems"), WorkflowPageId::BuildDeliveryBuildSystems},
        {QStringLiteral("project.build-delivery.testing"), WorkflowPageId::BuildDeliveryTesting},
        {QStringLiteral("project.build-delivery.automation"), WorkflowPageId::BuildDeliveryAutomation}});

    workflow_->setCurrentPage(currentPage_);
    refreshCompletionProgress();

    placeOnPreferredScreen();
    refreshCurrentWorkflowLayout();
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
    refreshCurrentWorkflowLayout();

    if (pageScroll_->verticalScrollBar())
    {
        pageScroll_->verticalScrollBar()->setValue(0);
    }

    workflow_->setCurrentPage(page);
}

void MainWindow::refreshCurrentWorkflowLayout()
{
    if (!pageScroll_ || !stack_) return;
    auto* current = stack_->currentWidget();
    if (!current) return;
    const int width = qMax(0, pageScroll_->viewport()->width());
    if (width <= 0) return;

    // Establish the viewport width before asking the current page for its
    // natural height. This matters on cold start, when the page was built
    // before the first visible viewport geometry existed.
    stack_->setFixedWidth(width);
    if (auto* stackLayout = stack_->layout()) {
        stackLayout->invalidate();
        stackLayout->activate();
    }
    current->updateGeometry();
    if (auto* pageLayout = current->layout()) {
        pageLayout->invalidate();
        pageLayout->activate();
    }
    const int naturalHeight = qMax(current->sizeHint().height(), current->minimumSizeHint().height());
    stack_->setFixedSize(width, naturalHeight);
    if (auto* stackLayout = stack_->layout()) {
        stackLayout->setGeometry(stack_->contentsRect());
        stackLayout->activate();
    }
    current->setGeometry(stack_->contentsRect());
    if (auto* pageLayout = current->layout()) {
        pageLayout->invalidate();
        pageLayout->activate();
    }
    stack_->updateGeometry();
    pageScroll_->viewport()->updateGeometry();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    refreshCurrentWorkflowLayout();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // The first show establishes the real viewport size. Use the canonical
    // shell refresh once that geometry is available to the event loop.
    QTimer::singleShot(0, this, &MainWindow::refreshCurrentWorkflowLayout);
}

void MainWindow::saveWork()
{
    QString error;
    if (!projectPage_->saveCurrentProject(&error) && !error.isEmpty())
        QMessageBox::warning(this, tr("Save Work"), error);
}

void MainWindow::refreshCompletionProgress()
{
    if (!completionProgress_ || !workflow_) return;
    completionProgress_->setValue(workflow_->completionPercentage());
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
    if ((watched == pageScroll_ || (pageScroll_ && watched == pageScroll_->viewport()))
        && event->type() == QEvent::Resize) {
        refreshCurrentWorkflowLayout();
    }
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
