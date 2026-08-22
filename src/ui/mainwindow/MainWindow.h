// MainWindow.h

#pragma once

#include <QMainWindow>
#include <QMap>
#include <QFont>

#include "core/ProjectModel.h"
#include "core/ProjectPersistence.h"
#include "core/Services.h"
#include "ui/workflow/WorkflowPageId.h"

class QScrollArea;
class QStackedWidget;
class WorkflowWidget;

class ProjectSetupPage;
class ProjectAcademicPage;
class ProjectLanguagesPage;
class ProjectFrameworksPage;
class ProjectDevelopmentToolsPage;
class ProjectPlatformsPage;
class ProjectHardwareArchitecturePage;
class ProjectBuildDeliveryPage;
class ResourceInventoryPage;
class ResourceAuthorityPage;
class ResourcePolicyPage;
class AiAgentsPage;
class AiResponsibilitiesPage;
class AiAutonomyPage;
class AiIntegrationPage;

class RuleSelectionPage;
class RuleRoutingPage;
class MemoryCapturePage;
class MemoryMaintenancePage;

class ReviewPage;
class GeneratePage;
class VerifyPage;
class FinalizePage;
class QEvent;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        int preferredScreenIndex,
        int preferredWindowWidth,
        int preferredWindowHeight,
        QWidget* parent = nullptr);

private slots:
    void setWorkflowPage(WorkflowPageId page);
    void goBack();
    void goForward();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void setUiZoom(int percent);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void placeOnPreferredScreen();

    QFont baseApplicationFont_;
    QString baseStyleSheet_;
    int uiZoomPercent_ = 100;

    int preferredScreenIndex_;
    int preferredWindowWidth_;
    int preferredWindowHeight_;

    ProjectModel projectModel_;
    ProjectPersistence projectPersistence_;
    TemplateManager templateManager_;
    GenerationServices generationServices_;
    VerificationServices verificationServices_;
    FinalizationServices finalizationServices_;
    AgentEntryPointService agentEntryPointService_;

    WorkflowWidget* workflow_ = nullptr;
    QScrollArea* pageScroll_ = nullptr;
    QStackedWidget* stack_ = nullptr;

    ProjectSetupPage* projectPage_ = nullptr;
    ProjectAcademicPage* academicPage_ = nullptr;
    ProjectLanguagesPage* languagesPage_ = nullptr;
    ProjectFrameworksPage* frameworksPage_ = nullptr;
    ProjectDevelopmentToolsPage* developmentToolsPage_ = nullptr;
    ProjectPlatformsPage* platformsPage_ = nullptr;
    ProjectHardwareArchitecturePage* hardwareArchitecturePage_ = nullptr;
    ProjectBuildDeliveryPage* buildDeliveryPage_ = nullptr;
    AiAgentsPage* aiAgentsPage_ = nullptr;
    AiResponsibilitiesPage* aiResponsibilitiesPage_ = nullptr;
    AiAutonomyPage* aiAutonomyPage_ = nullptr;
    AiIntegrationPage* aiIntegrationPage_ = nullptr;
    ResourceInventoryPage* resourceInventoryPage_ = nullptr;
    ResourceAuthorityPage* resourceAuthorityPage_ = nullptr;
    ResourcePolicyPage* resourcePolicyPage_ = nullptr;

    RuleSelectionPage* ruleSelectionPage_ = nullptr;
    RuleRoutingPage* ruleRoutingPage_ = nullptr;
    MemoryCapturePage* memoryCapturePage_ = nullptr;
    MemoryMaintenancePage* memoryMaintenancePage_ = nullptr;

    ReviewPage* reviewPage_ = nullptr;
    GeneratePage* generatePage_ = nullptr;
    VerifyPage* verificationPage_ = nullptr;
    FinalizePage* finalizePage_ = nullptr;

    QMap<WorkflowPageId, int> pageStackIndices_;

    WorkflowPageId currentPage_ = WorkflowPageId::Setup;
};
