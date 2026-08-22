#include "MemoryMaintenancePage.h"

#include "core/MemoryCatalog.h"
#include "core/ProjectMemory.h"
#include "core/ProjectModel.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
constexpr qint64 BytesPerMiB = 1024LL * 1024LL;
constexpr qint64 BytesPerGiB = 1024LL * 1024LL * 1024LL;
constexpr qint64 MinimumCustomBytes = 100LL * BytesPerMiB;

int customIndex()
{
    return MemoryMaintenancePage::memorySizePresets().size();
}

qint64 unitMultiplier(const QComboBox* unit)
{
    return unit->currentData().toString() == QStringLiteral("gb") ? BytesPerGiB : BytesPerMiB;
}

int displayDecimals(qint64 bytes, qint64 multiplier)
{
    const double value = static_cast<double>(bytes) / static_cast<double>(multiplier);
    if (!std::isfinite(value)) {
        return 9;
    }

    for (int decimals = 0; decimals <= 9; ++decimals) {
        const double scale = std::pow(10.0, decimals);
        const double rounded = std::round(value * scale) / scale;
        if (std::abs(value - rounded) <= 1.0e-10) {
            return decimals;
        }
    }
    return 9;
}
}

QList<QPair<QString, qint64>> MemoryMaintenancePage::memorySizePresets()
{
    return {
        {QObject::tr("500 MB"), 500LL * BytesPerMiB},
        {QObject::tr("1 GB"), 1LL * BytesPerGiB},
        {QObject::tr("2 GB"), 2LL * BytesPerGiB},
        {QObject::tr("5 GB"), 5LL * BytesPerGiB},
        {QObject::tr("10 GB"), 10LL * BytesPerGiB},
        {QObject::tr("20 GB"), 20LL * BytesPerGiB},
        {QObject::tr("50 GB"), 50LL * BytesPerGiB},
        {QObject::tr("100 GB"), 100LL * BytesPerGiB}
    };
}

MemoryMaintenancePage::MemoryMaintenancePage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>How should project memory be maintained?</h2>"
           "Define how ARAMF updates, validates, limits and preserves project memory."),
        this));

    selectAll_ = new QPushButton(tr("Select All"), this);
    selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    layout->addWidget(selectAll_);

    auto* maintenance = new CapabilityCheckGroup(
        tr("Automatic Maintenance"), MemoryCatalog::maintenanceOptions(), 3, this);
    auto* validation = new CapabilityCheckGroup(
        tr("Validation"), MemoryCatalog::validationOptions(), 3, this);
    auto* history = new CapabilityCheckGroup(
        tr("History Policy"), MemoryCatalog::historyOptions(), 3, this);
    groups_ = {maintenance, validation, history};
    for (auto* group : groups_) {
        layout->addWidget(group);
        connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] {
            persist();
        });
    }

    auto* form = new QFormLayout;
    updateStrategy_ = new QComboBox(this);
    for (const auto& item : {
             qMakePair(tr("After every meaningful task"), QStringLiteral("meaningful-task")),
             qMakePair(tr("At explicit checkpoints"), QStringLiteral("checkpoint")),
             qMakePair(tr("Before commit / milestone"), QStringLiteral("before-milestone")),
             qMakePair(tr("At project close"), QStringLiteral("project-close")),
             qMakePair(tr("Manual only"), QStringLiteral("manual"))}) {
        updateStrategy_->addItem(item.first, item.second);
    }

    memorySizePreset_ = new QComboBox(this);
    memorySizePreset_->setObjectName(QStringLiteral("memorySizePreset"));
    for (const auto& preset : memorySizePresets()) {
        memorySizePreset_->addItem(preset.first, preset.second);
    }
    memorySizePreset_->addItem(tr("Custom"));

    customSize_ = new QDoubleSpinBox(this);
    customSize_->setObjectName(QStringLiteral("customMemorySize"));
    customSize_->setDecimals(6);
    customSize_->setRange(100.0, std::numeric_limits<double>::max());

    customUnit_ = new QComboBox(this);
    customUnit_->setObjectName(QStringLiteral("customMemoryUnit"));
    customUnit_->addItem(tr("MB"), QStringLiteral("mb"));
    customUnit_->addItem(tr("GB"), QStringLiteral("gb"));

    auto* customSizeRow = new QHBoxLayout;
    customSizeRow->addWidget(customSize_);
    customSizeRow->addWidget(customUnit_);
    customSizeWidget_ = new QWidget(this);
    customSizeWidget_->setLayout(customSizeRow);

    form->addRow(tr("When should memory be updated?"), updateStrategy_);
    form->addRow(tr("Maximum memory size"), memorySizePreset_);
    form->addRow(tr("Custom size"), customSizeWidget_);

    auto* limits = new QGroupBox(tr("Memory Size"), this);
    limits->setLayout(form);
    layout->addWidget(limits);

    usage_ = new QProgressBar(this);
    usage_->setRange(0, 100);
    usage_->setValue(0);
    usage_->setFormat(tr("Memory usage: unavailable / configured limit"));
    usage_->setTextVisible(true);
    layout->addWidget(usage_);
    layout->addStretch();

    connect(selectAll_, &QPushButton::clicked, this, &MemoryMaintenancePage::toggleAll);
    connect(updateStrategy_, &QComboBox::currentIndexChanged, this, [this] {
        persist();
    });
    connect(memorySizePreset_, &QComboBox::currentIndexChanged, this,
            &MemoryMaintenancePage::applyPreset);
    connect(customSize_, &QDoubleSpinBox::valueChanged, this,
            &MemoryMaintenancePage::updateCustomSize);
    connect(customUnit_, &QComboBox::currentIndexChanged, this, [this] {
        if (memorySizePreset_->currentIndex() == customIndex()) {
            updateCustomDisplay();
        }
    });
    connect(model_, &ProjectModel::modelChanged, this, &MemoryMaintenancePage::refresh);
    refresh();
}

void MemoryMaintenancePage::persist()
{
    auto value = model_->memoryConfiguration();
    value.maintenanceOptions = groups_.at(0)->selectedIds();
    value.validationOptions = groups_.at(1)->selectedIds();
    value.historyOptions = groups_.at(2)->selectedIds();
    value.updateStrategy = updateStrategy_->currentData().toString();
    model_->setMemoryConfiguration(value);
    updateSelectAllText();
}

void MemoryMaintenancePage::applyPreset(int index)
{
    if (index < 0) {
        return;
    }

    const auto presets = memorySizePresets();
    const bool custom = index == presets.size();
    customSizeWidget_->setVisible(custom);
    if (custom) {
        setNaturalCustomUnit(model_->memoryConfiguration().maximumSizeBytes);
        updateCustomDisplay();
        return;
    }

    if (index >= presets.size()) {
        return;
    }

    auto value = model_->memoryConfiguration();
    value.maximumSizeBytes = presets.at(index).second;
    model_->setMemoryConfiguration(value);
}

void MemoryMaintenancePage::updateCustomRange()
{
    const qint64 multiplier = unitMultiplier(customUnit_);
    double minimum = multiplier == BytesPerGiB
                         ? 1.0
                         : static_cast<double>(MinimumCustomBytes) / BytesPerMiB;
    const qint64 currentBytes = model_->memoryConfiguration().maximumSizeBytes;
    if (multiplier == BytesPerGiB && currentBytes < BytesPerGiB) {
        minimum = static_cast<double>(currentBytes) / BytesPerGiB;
    }
    const double maximum = static_cast<double>(std::numeric_limits<qint64>::max())
                           / static_cast<double>(multiplier);
    QSignalBlocker blocker(customSize_);
    customSize_->setRange(minimum, maximum);
}

void MemoryMaintenancePage::updateCustomDisplay()
{
    updateCustomRange();
    const qint64 bytes = qMax(model_->memoryConfiguration().maximumSizeBytes,
                              MinimumCustomBytes);
    const qint64 multiplier = unitMultiplier(customUnit_);
    QSignalBlocker blocker(customSize_);
    customSize_->setDecimals(displayDecimals(bytes, multiplier));
    customSize_->setValue(static_cast<double>(bytes) / multiplier);
}

void MemoryMaintenancePage::setNaturalCustomUnit(qint64 bytes)
{
    QSignalBlocker blocker(customUnit_);
    customUnit_->setCurrentIndex(bytes >= BytesPerGiB && bytes % BytesPerGiB == 0 ? 1 : 0);
}

void MemoryMaintenancePage::updateCustomSize()
{
    if (memorySizePreset_->currentIndex() != customIndex()) {
        return;
    }

    const double requested = customSize_->value() * static_cast<double>(unitMultiplier(customUnit_));
    if (requested < static_cast<double>(MinimumCustomBytes)
        || requested > static_cast<double>(std::numeric_limits<qint64>::max())) {
        return;
    }

    auto value = model_->memoryConfiguration();
    value.maximumSizeBytes = static_cast<qint64>(requested);
    updatingCustomSize_ = true;
    model_->setMemoryConfiguration(value);
    updatingCustomSize_ = false;
}

void MemoryMaintenancePage::refreshMemorySizeControls()
{
    const qint64 bytes = model_->memoryConfiguration().maximumSizeBytes;
    const auto presets = memorySizePresets();
    int matchingPreset = customIndex();
    for (int index = 0; index < presets.size(); ++index) {
        if (presets.at(index).second == bytes) {
            matchingPreset = index;
            break;
        }
    }

    QSignalBlocker presetBlocker(memorySizePreset_);
    memorySizePreset_->setCurrentIndex(matchingPreset);
    customSizeWidget_->setVisible(matchingPreset == customIndex());
    if (matchingPreset == customIndex()) {
        setNaturalCustomUnit(bytes);
        updateCustomDisplay();
    }
}

void MemoryMaintenancePage::toggleAll()
{
    const bool select = !std::all_of(
        groups_.cbegin(), groups_.cend(), [](auto* group) {
            return group->allSelectableSelected();
        });
    for (auto* group : groups_) {
        group->setAllSelected(select);
    }
    persist();
}

void MemoryMaintenancePage::updateSelectAllText()
{
    const bool all = !groups_.isEmpty()
                     && std::all_of(groups_.cbegin(), groups_.cend(), [](auto* group) {
                            return group->allSelectableSelected();
                        });
    selectAll_->setText(all ? tr("Clear All") : tr("Select All"));
}

void MemoryMaintenancePage::refresh()
{
    const auto value = model_->memoryConfiguration();
    updateStrategy_->setCurrentIndex(updateStrategy_->findData(value.updateStrategy));
    if (!updatingCustomSize_) {
        refreshMemorySizeControls();
    }
    groups_.at(0)->setSelectedIds(value.maintenanceOptions);
    groups_.at(1)->setSelectedIds(value.validationOptions);
    groups_.at(2)->setSelectedIds(value.historyOptions);

    const qint64 current = model_->projectPath().isEmpty()
                               ? 0
                               : ProjectMemory().memoryUsageBytes(model_->projectPath());
    const int percent = value.maximumSizeBytes > 0
                            ? qBound(0, static_cast<int>((current * 100) / value.maximumSizeBytes), 100)
                            : 0;
    usage_->setValue(percent);
    usage_->setFormat(model_->projectPath().isEmpty()
                          ? tr("Memory usage: unavailable / configured limit")
                          : tr("Memory usage: %1 / %2 GB")
                                .arg(QString::number(static_cast<double>(current) / BytesPerGiB, 'f', 1))
                                .arg(QString::number(static_cast<double>(value.maximumSizeBytes) / BytesPerGiB, 'f', 1)));
    updateSelectAllText();
}
