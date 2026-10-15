#include "ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectFrameworksPage::ProjectFrameworksPage(ProjectModel* model, QWidget* parent,
                                             QList<EnvironmentOption> options, QStringList managedIds)
// Brief: Build a framework catalog page for the complete catalog or a domain subset.
// Subset updates merge their own IDs, preserving every selected framework from other groups.
    : QWidget(parent), model_(model), managedIds_(std::move(managedIds))
{
    auto* layout = new QVBoxLayout(this);
    if (options.isEmpty()) options = EnvironmentCatalog::frameworks();
    if (managedIds_.isEmpty()) for (const auto& option : options) managedIds_ << option.second;
    layout->addWidget(new QLabel(tr("<h2>Which frameworks / SDKs are used?</h2>Select the frameworks and SDKs the project depends on."), this));
    frameworks_ = new CapabilityCheckGroup(tr("Frameworks / SDKs"), options, 3, this);
    layout->addWidget(frameworks_);
    details_ = new CapabilityDetailsEditor(model_, frameworks_, CapabilityDetailsEditor::Domain::Frameworks, this);
    layout->addWidget(details_);
    layout->addStretch();
    connect(frameworks_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) {
        auto capabilities = model_->developmentCapabilities();
        QStringList merged;
        for (const auto& id : capabilities.frameworks) {
            const bool customManaged = managedIds_.contains(QStringLiteral("custom")) && id.startsWith(QStringLiteral("custom:"));
            if (!managedIds_.contains(id) && !customManaged) merged << id;
        }
        merged << value;
        merged.removeDuplicates();
        capabilities.frameworks = merged;
        model_->setDevelopmentCapabilities(capabilities);
    });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectFrameworksPage::refresh);
    refresh();
}

void ProjectFrameworksPage::refresh() { frameworks_->setSelectedIds(model_->developmentCapabilities().frameworks); }
