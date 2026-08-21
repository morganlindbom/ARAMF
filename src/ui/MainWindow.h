#pragma once
#include <QMainWindow>
#include "core/ProjectModel.h"
#include "core/Services.h"
class QStackedWidget; class WorkflowWidget; class ProjectDetailsPage; class TemplatePage; class DevelopmentEnvironmentPage; class AiToolsPage; class ResourcesPage; class ReviewPage; class GeneratePage; class FinalizePage; class RulesRoutingPage; class MemoryPage; class VerificationPage;
class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
private slots:
    void setWorkflowPage(int index);
    void goBack();
    void goForward();
private:
    ProjectModel projectModel_;
    TemplateManager templateManager_;
    GenerationServices generationServices_;
    WorkflowWidget* workflow_;
    QStackedWidget* stack_;
    ProjectDetailsPage* projectPage_;
    TemplatePage* templatePage_;
    DevelopmentEnvironmentPage* developmentPage_;
    AiToolsPage* aiToolsPage_;
    RulesRoutingPage* rulesPage_;
    MemoryPage* memoryPage_;
    ResourcesPage* resourcesPage_;
    ReviewPage* reviewPage_;
    GeneratePage* generatePage_;
    VerificationPage* verificationPage_;
    FinalizePage* finalizePage_;
};
