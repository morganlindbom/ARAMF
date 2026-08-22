#include "MemoryCapturePage.h"

#include "core/MemoryCatalog.h"
#include "core/ProjectModel.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

MemoryCapturePage::MemoryCapturePage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>What should ARAMF remember?</h2>Select which project information may be stored as durable project memory."), this));
    auto* form = new QFormLayout; retention_ = new QComboBox(this);
    retention_->addItem(tr("Minimal"), QStringLiteral("minimal")); retention_->addItem(tr("Standard"), QStringLiteral("standard")); retention_->addItem(tr("Detailed"), QStringLiteral("detailed")); retention_->addItem(tr("Audit / Full History"), QStringLiteral("audit"));
    form->addRow(tr("Memory Retention"), retention_); layout->addLayout(form);
    selectAll_ = new QPushButton(tr("Select All"), this); selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed); layout->addWidget(selectAll_);
    const auto all = MemoryCatalog::captureCategories();
    const QList<QPair<QString, int>> sections{{tr("Project State"), 7}, {tr("Decisions"), 7}, {tr("Development History"), 7}, {tr("Knowledge"), 7}, {tr("Metrics"), 8}};
    int offset = 0; for (const auto& section : sections) { auto* group = new CapabilityCheckGroup(section.first, all.mid(offset, section.second), 3, this); groups_.append(group); layout->addWidget(group); connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); }); offset += section.second; }
    layout->addStretch(); connect(retention_, &QComboBox::currentIndexChanged, this, [this] { persist(); }); connect(selectAll_, &QPushButton::clicked, this, &MemoryCapturePage::toggleAll); connect(model_, &ProjectModel::modelChanged, this, &MemoryCapturePage::refresh); refresh();
}

void MemoryCapturePage::persist() { auto value = model_->memoryConfiguration(); value.retentionLevel = retention_->currentData().toString(); value.captureCategories.clear(); for (auto* group : groups_) value.captureCategories << group->selectedIds(); model_->setMemoryConfiguration(value); updateSelectAllText(); }
void MemoryCapturePage::toggleAll() { const bool select = !std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) { return group->allSelectableSelected(); }); for (auto* group : groups_) group->setAllSelected(select); persist(); }
void MemoryCapturePage::updateSelectAllText() { const bool all = !groups_.isEmpty() && std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) { return group->allSelectableSelected(); }); selectAll_->setText(all ? tr("Clear All") : tr("Select All")); }
void MemoryCapturePage::refresh() { const auto value = model_->memoryConfiguration(); retention_->setCurrentIndex(retention_->findData(value.retentionLevel)); for (auto* group : groups_) group->setSelectedIds(value.captureCategories); updateSelectAllText(); }
