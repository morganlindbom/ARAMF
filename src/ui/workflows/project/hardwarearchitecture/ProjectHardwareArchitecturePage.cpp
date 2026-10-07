#include "ProjectHardwareArchitecturePage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectHardwareArchitecturePage::ProjectHardwareArchitecturePage(ProjectModel* model, QWidget* parent, Section section)
    : QWidget(parent), model_(model), section_(section)
{
    auto* layout = new QVBoxLayout(this);
    const QString title = section_ == Section::Architectures ? tr("Target architectures")
        : section_ == Section::Processors ? tr("Processor families")
        : section_ == Section::Targets ? tr("Hardware & deployment targets") : tr("Hardware & architecture");
    layout->addWidget(new QLabel(QStringLiteral("<h2>%1</h2>Select the processor architectures and physical hardware targeted by the project.").arg(title), this));
    architectures_ = new CapabilityCheckGroup(tr("Target Architectures"), EnvironmentCatalog::architectures(), 3, this);
    processors_ = new CapabilityCheckGroup(tr("MCU / Processor Families"), EnvironmentCatalog::processorFamilies(), 3, this);
    hardware_ = new CapabilityCheckGroup(tr("Hardware / Deployment Targets"), EnvironmentCatalog::hardwareTargets(), 3, this);
    architectures_->setObjectName(QStringLiteral("targetArchitectures"));
    processors_->setObjectName(QStringLiteral("processorFamilies"));
    hardware_->setObjectName(QStringLiteral("hardwareTargets"));
    layout->addWidget(architectures_); layout->addWidget(processors_); layout->addWidget(hardware_); layout->addStretch();
    connect(architectures_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.targetArchitectures=value; model_->setDevelopmentCapabilities(c); });
    connect(processors_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.processorFamilies=value; model_->setDevelopmentCapabilities(c); });
    connect(hardware_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.hardwareTargets=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectHardwareArchitecturePage::refresh);
    const bool all = section_ == Section::All;
    architectures_->setVisible(all || section_ == Section::Architectures);
    processors_->setVisible(all || section_ == Section::Processors);
    hardware_->setVisible(all || section_ == Section::Targets);
    refresh();
}

void ProjectHardwareArchitecturePage::refresh() { const auto c=model_->developmentCapabilities(); architectures_->setSelectedIds(c.targetArchitectures); processors_->setSelectedIds(c.processorFamilies); hardware_->setSelectedIds(c.hardwareTargets); }
