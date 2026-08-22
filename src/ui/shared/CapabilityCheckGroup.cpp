#include "CapabilityCheckGroup.h"

#include <algorithm>
#include <QCheckBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QSignalBlocker>

namespace {
bool isCustomId(const QString& id)
{
    return id == QStringLiteral("custom") || id == QStringLiteral("custom-agent");
}
}

CapabilityCheckGroup::CapabilityCheckGroup(const QString& title, const QList<EnvironmentOption>& options, int columns, QWidget* parent)
    : QGroupBox(title, parent)
{
    auto* layout = new QGridLayout(this);
    layout->setColumnStretch(columns, 1);
    for (int index = 0; index < options.size(); ++index) {
        const auto& option = options.at(index);
        auto* check = new QCheckBox(option.first, this);
        check->setProperty("capabilityId", option.second);
        checks_.append(check);
        layout->addWidget(check, index / columns, index % columns);
        connect(check, &QCheckBox::toggled, this, [this](bool) {
            if (customEdit_) {
                const auto values = selectedIds();
                customEdit_->setVisible(std::any_of(values.cbegin(), values.cend(), [](const QString& value) {
                    return value.startsWith(QStringLiteral("custom:"));
                }));
            }
            emit selectionChanged(selectedIds());
        });
    }
    customEdit_ = new QLineEdit(this);
    customEdit_->setPlaceholderText(tr("Custom value"));
    customEdit_->setVisible(false);
    layout->addWidget(customEdit_, (options.size() + columns - 1) / columns, 0, 1, columns);
    connect(customEdit_, &QLineEdit::textChanged, this, [this] { emit selectionChanged(selectedIds()); });
}

QStringList CapabilityCheckGroup::selectedIds() const
{
    QStringList result;
    for (auto* check : checks_) {
        if (!check->isChecked()) continue;
        const QString id = check->property("capabilityId").toString();
        if (isCustomId(id) && customEdit_ && !customEdit_->text().trimmed().isEmpty()) {
            result << QStringLiteral("custom:%1").arg(customEdit_->text().trimmed());
        } else {
            result << id;
        }
    }
    return result;
}

void CapabilityCheckGroup::setSelectedIds(const QStringList& ids)
{
    const QSignalBlocker blocker(this);
    QString customValue;
    for (const QString& value : ids) {
        if (value.startsWith(QStringLiteral("custom:"))) customValue = value.mid(7);
    }
    if (customEdit_) customEdit_->setText(customValue);
    for (auto* check : checks_) {
        const QString id = check->property("capabilityId").toString();
        check->setChecked(ids.contains(id) || (isCustomId(id) && !customValue.isEmpty()));
    }
    if (customEdit_) customEdit_->setVisible((ids.contains(QStringLiteral("custom")) || ids.contains(QStringLiteral("custom-agent"))) || !customValue.isEmpty());
}

void CapabilityCheckGroup::setOptionEnabled(const QString& id, bool enabled)
{
    for (auto* check : checks_) {
        if (check->property("capabilityId").toString() == id) {
            check->setEnabled(enabled);
            if (!enabled) check->setChecked(false);
        }
    }
}

bool CapabilityCheckGroup::allSelectableSelected(bool includeCustom) const
{
    bool found = false;
    for (auto* check : checks_) {
        const QString id = check->property("capabilityId").toString();
        if (!check->isEnabled() || (!includeCustom && isCustomId(id))) continue;
        found = true;
        if (!check->isChecked()) return false;
    }
    return found;
}

void CapabilityCheckGroup::setAllSelected(bool selected, bool includeCustom)
{
    bool changed = false;
    for (auto* check : checks_) {
        const QString id = check->property("capabilityId").toString();
        if (selected && (!check->isEnabled() || (!includeCustom && isCustomId(id)))) continue;
        if (check->isChecked() == selected) continue;
        check->setChecked(selected);
        changed = true;
    }
    if (changed) emit selectionChanged(selectedIds());
}
