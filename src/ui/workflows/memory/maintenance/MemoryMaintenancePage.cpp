#include "MemoryMaintenancePage.h"

#include "core/MemoryCatalog.h"
#include "core/ProjectModel.h"
#include "core/ProjectMemory.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <limits>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

MemoryMaintenancePage::MemoryMaintenancePage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>How should project memory be maintained?</h2>Define how ARAMF updates, validates, limits and preserves project memory."), this));
    selectAll_ = new QPushButton(tr("Select All"), this); selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed); layout->addWidget(selectAll_);
    auto* maintenance = new CapabilityCheckGroup(tr("Automatic Maintenance"), MemoryCatalog::maintenanceOptions(), 3, this);
    auto* validation = new CapabilityCheckGroup(tr("Validation"), MemoryCatalog::validationOptions(), 3, this);
    auto* history = new CapabilityCheckGroup(tr("History Policy"), MemoryCatalog::historyOptions(), 3, this);
    groups_ = {maintenance, validation, history}; for (auto* group : groups_) { layout->addWidget(group); connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); }); }
    auto* form = new QFormLayout;
    updateStrategy_ = new QComboBox(this); for (const auto& item : {qMakePair(tr("After every meaningful task"), QStringLiteral("meaningful-task")), qMakePair(tr("At explicit checkpoints"), QStringLiteral("checkpoint")), qMakePair(tr("Before commit / milestone"), QStringLiteral("before-milestone")), qMakePair(tr("At project close"), QStringLiteral("project-close")), qMakePair(tr("Manual only"), QStringLiteral("manual"))}) updateStrategy_->addItem(item.first, item.second);
    maximum_ = new QDoubleSpinBox(this); maximum_->setRange(100, 1024 * 1024); maximum_->setDecimals(0);
    unit_ = new QComboBox(this); unit_->addItem(tr("MB"), QStringLiteral("mb")); unit_->addItem(tr("GB"), QStringLiteral("gb"));
    auto* sizeRow = new QHBoxLayout; sizeRow->addWidget(maximum_); sizeRow->addWidget(unit_); auto* sizeWidget = new QWidget(this); sizeWidget->setLayout(sizeRow);
    form->addRow(tr("When should memory be updated?"), updateStrategy_); form->addRow(tr("Maximum memory size"), sizeWidget);
    auto* limits = new QGroupBox(tr("Memory Size Limit"), this); limits->setLayout(form); layout->addWidget(limits);
    usage_ = new QProgressBar(this); usage_->setRange(0, 100); usage_->setValue(0); usage_->setFormat(tr("Memory usage: unavailable / configured limit")); usage_->setTextVisible(true); layout->addWidget(usage_);
    layout->addStretch();
    connect(selectAll_, &QPushButton::clicked, this, &MemoryMaintenancePage::toggleAll); connect(updateStrategy_, &QComboBox::currentIndexChanged, this, [this] { persist(); }); connect(maximum_, &QDoubleSpinBox::valueChanged, this, [this] { updateLimit(); }); connect(unit_, &QComboBox::currentIndexChanged, this, [this] { refresh(); }); connect(model_, &ProjectModel::modelChanged, this, &MemoryMaintenancePage::refresh); refresh();
}

void MemoryMaintenancePage::persist() { auto value = model_->memoryConfiguration(); value.maintenanceOptions = groups_.at(0)->selectedIds(); value.validationOptions = groups_.at(1)->selectedIds(); value.historyOptions = groups_.at(2)->selectedIds(); value.updateStrategy = updateStrategy_->currentData().toString(); model_->setMemoryConfiguration(value); updateSelectAllText(); }
void MemoryMaintenancePage::updateLimit() { auto value = model_->memoryConfiguration(); const qint64 multiplier = unit_->currentData().toString() == QStringLiteral("gb") ? 1024LL * 1024LL * 1024LL : 1024LL * 1024LL; const double requested = maximum_->value() * static_cast<double>(multiplier); if (requested < 100.0 * 1024.0 * 1024.0 || requested > static_cast<double>(std::numeric_limits<qint64>::max())) return; value.maximumSizeBytes = static_cast<qint64>(requested); model_->setMemoryConfiguration(value); }
void MemoryMaintenancePage::toggleAll() { const bool select = !std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) { return group->allSelectableSelected(); }); for (auto* group : groups_) group->setAllSelected(select); persist(); }
void MemoryMaintenancePage::updateSelectAllText() { const bool all = !groups_.isEmpty() && std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) { return group->allSelectableSelected(); }); selectAll_->setText(all ? tr("Clear All") : tr("Select All")); }
void MemoryMaintenancePage::refresh() { const auto value = model_->memoryConfiguration(); updateStrategy_->setCurrentIndex(updateStrategy_->findData(value.updateStrategy)); const bool gb = value.maximumSizeBytes >= 1024LL * 1024LL * 1024LL && value.maximumSizeBytes % (1024LL * 1024LL * 1024LL) == 0; unit_->setCurrentIndex(gb ? 1 : 0); const qint64 divisor = gb ? 1024LL * 1024LL * 1024LL : 1024LL * 1024LL; maximum_->setValue(static_cast<double>(value.maximumSizeBytes) / divisor); groups_.at(0)->setSelectedIds(value.maintenanceOptions); groups_.at(1)->setSelectedIds(value.validationOptions); groups_.at(2)->setSelectedIds(value.historyOptions); const qint64 current = model_->projectPath().isEmpty() ? 0 : ProjectMemory().memoryUsageBytes(model_->projectPath()); const int percent = value.maximumSizeBytes > 0 ? qBound(0, static_cast<int>((current * 100) / value.maximumSizeBytes), 100) : 0; usage_->setValue(percent); usage_->setFormat(model_->projectPath().isEmpty() ? tr("Memory usage: unavailable / configured limit") : tr("Memory usage: %1 / %2 %3").arg(current >= 1024LL * 1024LL * 1024LL ? QString::number(static_cast<double>(current) / (1024.0 * 1024.0 * 1024.0), 'f', 1) : QString::number(current / (1024LL * 1024LL))).arg(gb ? QString::number(static_cast<double>(value.maximumSizeBytes) / (1024.0 * 1024.0 * 1024.0), 'f', 1) : QString::number(value.maximumSizeBytes / (1024LL * 1024LL))).arg(gb ? tr("GB") : tr("MB"))); updateSelectAllText(); }
