#include "ProjectHardwareArchitecturePage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectHardwareArchitecturePage::ProjectHardwareArchitecturePage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Which hardware / architecture is used?</h2>Select the processor architectures and physical hardware targeted by the project."), this));
    architectures_ = new CapabilityCheckGroup(tr("Target Architectures"), EnvironmentCatalog::architectures(), 3, this);
    processors_ = new CapabilityCheckGroup(tr("MCU / Processor Families"), EnvironmentCatalog::processorFamilies(), 3, this);
    hardware_ = new CapabilityCheckGroup(tr("Hardware / Deployment Targets"), EnvironmentCatalog::hardwareTargets(), 3, this);
    layout->addWidget(architectures_); layout->addWidget(processors_); layout->addWidget(hardware_); layout->addStretch();
    connect(architectures_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.targetArchitectures=value; model_->setDevelopmentCapabilities(c); });
    connect(processors_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.processorFamilies=value; model_->setDevelopmentCapabilities(c); });
    connect(hardware_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.hardwareTargets=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectHardwareArchitecturePage::refresh);
    refresh();
}

void ProjectHardwareArchitecturePage::refresh() { const auto c=model_->developmentCapabilities(); architectures_->setSelectedIds(c.targetArchitectures); processors_->setSelectedIds(c.processorFamilies); hardware_->setSelectedIds(c.hardwareTargets); }
