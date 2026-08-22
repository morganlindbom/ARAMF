#include "ProjectDevelopmentToolsPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectDevelopmentToolsPage::ProjectDevelopmentToolsPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Which development tools are used?</h2>Select the tools used to develop, inspect and version the project."), this));
    ides_ = new CapabilityCheckGroup(tr("IDE / Editor"), EnvironmentCatalog::ides(), 3, this);
    versionControl_ = new CapabilityCheckGroup(tr("Version Control"), EnvironmentCatalog::versionControlSystems(), 3, this);
    support_ = new CapabilityCheckGroup(tr("Development Support"), EnvironmentCatalog::developmentSupport(), 3, this);
    layout->addWidget(ides_); layout->addWidget(versionControl_); layout->addWidget(support_); layout->addStretch();
    connect(ides_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.ides=value; model_->setDevelopmentCapabilities(c); });
    connect(versionControl_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.versionControlSystems=value; model_->setDevelopmentCapabilities(c); });
    connect(support_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) { auto c=model_->developmentCapabilities(); c.developmentTools=value; model_->setDevelopmentCapabilities(c); });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectDevelopmentToolsPage::refresh);
    refresh();
}

void ProjectDevelopmentToolsPage::refresh() { const auto c=model_->developmentCapabilities(); ides_->setSelectedIds(c.ides); versionControl_->setSelectedIds(c.versionControlSystems); support_->setSelectedIds(c.developmentTools); }
