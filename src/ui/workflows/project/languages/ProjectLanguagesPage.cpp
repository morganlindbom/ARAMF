#include "ProjectLanguagesPage.h"
#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"
#include <QLabel>
#include <QVBoxLayout>

ProjectLanguagesPage::ProjectLanguagesPage(ProjectModel* model, QWidget* parent,
                                           QList<EnvironmentOption> options, QStringList managedIds)
// Brief: Build a language catalog page for either the full catalog or one categorized subset.
// Subset pages merge only their stable IDs into the existing selection so values from sibling pages are preserved.
    : QWidget(parent), model_(model), managedIds_(std::move(managedIds))
{
    auto* layout = new QVBoxLayout(this);
    const bool allOptions = options.isEmpty();
    if (allOptions) options = EnvironmentCatalog::languages();
    if (managedIds_.isEmpty()) for (const auto& option : options) managedIds_ << option.second;
    layout->addWidget(new QLabel(tr("<h2>Which languages are used?</h2>Select every programming or markup language used by the project."), this));
    languages_ = new CapabilityCheckGroup(tr("Languages"), options, 3, this);
    layout->addWidget(languages_);
    details_ = new CapabilityDetailsEditor(model_, languages_, CapabilityDetailsEditor::Domain::Languages, this);
    layout->addWidget(details_);
    layout->addStretch();
    connect(languages_, &CapabilityCheckGroup::selectionChanged, this, [this](const QStringList& value) {
        auto capabilities = model_->developmentCapabilities();
        QStringList merged;
        for (const auto& id : capabilities.languages) {
            const bool customManaged = managedIds_.contains(QStringLiteral("custom")) && id.startsWith(QStringLiteral("custom:"));
            if (!managedIds_.contains(id) && !customManaged) merged << id;
        }
        merged << value;
        merged.removeDuplicates();
        capabilities.languages = merged;
        model_->setDevelopmentCapabilities(capabilities);
    });
    connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &ProjectLanguagesPage::refresh);
    refresh();
}

void ProjectLanguagesPage::refresh() { languages_->setSelectedIds(model_->developmentCapabilities().languages); }
