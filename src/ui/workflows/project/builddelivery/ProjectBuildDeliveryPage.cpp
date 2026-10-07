#include "ProjectBuildDeliveryPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectBuildDeliveryPage::ProjectBuildDeliveryPage(ProjectModel* model, QWidget* parent, Section section)
    : QWidget(parent), model_(model), section_(section)
{
    auto* layout = new QVBoxLayout(this);
    const QString title = section_ == Section::Toolchains ? tr("Compiler & toolchain")
        : section_ == Section::BuildSystems ? tr("Build systems & dependencies")
        : section_ == Section::Testing ? tr("Testing & quality")
        : section_ == Section::Automation ? tr("Automation & delivery") : tr("Build, test & delivery");
    layout->addWidget(new QLabel(QStringLiteral("<h2>%1</h2>Select how the project is compiled, tested, automated and delivered.").arg(title), this));
    toolchains_ = new CapabilityCheckGroup(tr("Compiler / Toolchain / Runtime"), EnvironmentCatalog::toolchains(), 3, this);
    buildSystems_ = new CapabilityCheckGroup(tr("Build Systems"), EnvironmentCatalog::buildSystems(), 3, this);
    dependencies_ = new CapabilityCheckGroup(tr("Dependency / Package Management"), EnvironmentCatalog::dependencyManagers(), 3, this);
    configurations_ = new CapabilityCheckGroup(tr("Build Configurations"), EnvironmentCatalog::buildConfigurations(), 3, this);
    testing_ = new CapabilityCheckGroup(tr("Testing"), EnvironmentCatalog::testingCapabilities(), 3, this);
    quality_ = new CapabilityCheckGroup(tr("Quality / Analysis"), EnvironmentCatalog::qualityCapabilities(), 3, this);
    automation_ = new CapabilityCheckGroup(tr("Automation"), EnvironmentCatalog::automationCapabilities(), 3, this);
    delivery_ = new CapabilityCheckGroup(tr("Delivery"), EnvironmentCatalog::deliveryCapabilities(), 3, this);
    toolchains_->setObjectName(QStringLiteral("buildToolchains"));
    buildSystems_->setObjectName(QStringLiteral("buildSystems"));
    dependencies_->setObjectName(QStringLiteral("dependencyManagers"));
    configurations_->setObjectName(QStringLiteral("buildConfigurations"));
    testing_->setObjectName(QStringLiteral("testingCapabilities"));
    quality_->setObjectName(QStringLiteral("qualityCapabilities"));
    automation_->setObjectName(QStringLiteral("automationCapabilities"));
    delivery_->setObjectName(QStringLiteral("deliveryCapabilities"));
    layout->addWidget(toolchains_); layout->addWidget(buildSystems_); layout->addWidget(dependencies_); layout->addWidget(configurations_);
    layout->addWidget(testing_); layout->addWidget(quality_); layout->addWidget(automation_); layout->addWidget(delivery_); layout->addStretch();
    connect(toolchains_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.toolchains=value; model_->setDevelopmentCapabilities(c); });
    connect(buildSystems_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.buildSystems=value; model_->setDevelopmentCapabilities(c); });
    connect(dependencies_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.dependencyManagers=value; model_->setDevelopmentCapabilities(c); });
    connect(configurations_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.buildConfigurations=value; model_->setDevelopmentCapabilities(c); });
    connect(testing_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.testingCapabilities=value; model_->setDevelopmentCapabilities(c); });
    connect(quality_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.qualityCapabilities=value; model_->setDevelopmentCapabilities(c); });
    connect(automation_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.automationCapabilities=value; model_->setDevelopmentCapabilities(c); });
    connect(delivery_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.deliveryCapabilities=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectBuildDeliveryPage::refresh);
    const bool all = section_ == Section::All;
    const bool toolchain = all || section_ == Section::Toolchains;
    const bool build = all || section_ == Section::BuildSystems;
    const bool testing = all || section_ == Section::Testing;
    const bool automation = all || section_ == Section::Automation;
    toolchains_->setVisible(toolchain);
    buildSystems_->setVisible(build);
    dependencies_->setVisible(build);
    configurations_->setVisible(build);
    testing_->setVisible(testing);
    quality_->setVisible(testing);
    automation_->setVisible(automation);
    delivery_->setVisible(automation);
    refresh();
}

void ProjectBuildDeliveryPage::refresh() { const auto c=model_->developmentCapabilities(); toolchains_->setSelectedIds(c.toolchains); buildSystems_->setSelectedIds(c.buildSystems); dependencies_->setSelectedIds(c.dependencyManagers); configurations_->setSelectedIds(c.buildConfigurations); testing_->setSelectedIds(c.testingCapabilities); quality_->setSelectedIds(c.qualityCapabilities); automation_->setSelectedIds(c.automationCapabilities); delivery_->setSelectedIds(c.deliveryCapabilities); }
