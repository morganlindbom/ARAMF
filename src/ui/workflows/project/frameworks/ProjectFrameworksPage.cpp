#include "ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectFrameworksPage::ProjectFrameworksPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Which frameworks / SDKs are used?</h2>Select the frameworks and SDKs the project depends on."), this));
    frameworks_ = new CapabilityCheckGroup(tr("Frameworks / SDKs"), EnvironmentCatalog::frameworks(), 3, this);
    layout->addWidget(frameworks_);
    layout->addStretch();
    connect(frameworks_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) {
        auto capabilities = model_->developmentCapabilities();
        capabilities.frameworks = value;
        model_->setDevelopmentCapabilities(capabilities);
    });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectFrameworksPage::refresh);
    refresh();
}

void ProjectFrameworksPage::refresh() { frameworks_->setSelectedIds(model_->developmentCapabilities().frameworks); }
