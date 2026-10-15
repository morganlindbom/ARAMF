#include "CapabilityCheckGroup.h"

#include <algorithm>
#include <QCheckBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QFormLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
bool isCustomId(const QString& id)
{
    return id == QStringLiteral("custom") || id == QStringLiteral("custom-agent");
}
}

CapabilityCheckGroup::CapabilityCheckGroup(const QString& title, const QList<EnvironmentOption>& options, int columns, QWidget* parent)
    : QGroupBox(title, parent), requestedColumns_(qMax(1, columns))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout_ = new QGridLayout(this);
    layout_->setHorizontalSpacing(18);
    layout_->setVerticalSpacing(7);
    for (int index = 0; index < options.size(); ++index) {
        const auto& option = options.at(index);
        auto* check = new QCheckBox(option.first, this);
        check->setProperty("capabilityId", option.second);
        checks_.append(check);
        layout_->addWidget(check, index / requestedColumns_, index % requestedColumns_);
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
    layout_->addWidget(customEdit_, (options.size() + requestedColumns_ - 1) / requestedColumns_, 0, 1, requestedColumns_);
    connect(customEdit_, &QLineEdit::textChanged, this, [this] { emit selectionChanged(selectedIds()); });
    reflow();
}

void CapabilityCheckGroup::resizeEvent(QResizeEvent* event)
{
    QGroupBox::resizeEvent(event);
    reflow();
}

void CapabilityCheckGroup::reflow()
{
    if (!layout_) return;
    int widest = 1;
    for (auto* check : checks_) widest = qMax(widest, check->sizeHint().width());
    const int available = qMax(1, contentsRect().width() - layout_->contentsMargins().left() - layout_->contentsMargins().right());
    const int spacing = qMax(0, layout_->horizontalSpacing());
    int columns = 1;
    for (int candidate = requestedColumns_; candidate >= 1; --candidate) {
        if (candidate * widest + (candidate - 1) * spacing <= available) {
            columns = candidate;
            break;
        }
    }
    if (columns == activeColumns_) return;
    activeColumns_ = columns;
    while (auto* item = layout_->takeAt(0)) delete item;
    for (int column = 0; column < requestedColumns_; ++column)
        layout_->setColumnStretch(column, column < columns ? 1 : 0);
    for (int index = 0; index < checks_.size(); ++index)
        layout_->addWidget(checks_.at(index), index / columns, index % columns);
    if (customEdit_)
        layout_->addWidget(customEdit_, (checks_.size() + columns - 1) / columns, 0, 1, columns);
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

CapabilityDetailsEditor::CapabilityDetailsEditor(ProjectModel* model, CapabilityCheckGroup* selection,
                                                 Domain domain, QWidget* parent)
// Brief: Bind editable metadata fields to one capability selection group.
// The editor keeps version and notes optional and stores changes under stable selected IDs.
    : QGroupBox(tr("Selected item details (optional)"), parent), model_(model), selection_(selection), domain_(domain),
      layout_(new QVBoxLayout(this))
{
    if (selection_) connect(selection_, &CapabilityCheckGroup::selectionChanged, this, &CapabilityDetailsEditor::refresh);
    if (model_) connect(model_, &ProjectModel::developmentCapabilitiesChanged, this, &CapabilityDetailsEditor::refresh);
    refresh();
}

void CapabilityDetailsEditor::refresh()
// Brief: Rebuild detail rows for currently selected IDs.
// Stored metadata is read first so refreshing after a round-trip restores every editable value.
{
    if (storing_) return;
    while (auto* item = layout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    if (!model_ || !selection_) return;
    const auto capabilities = model_->developmentCapabilities();
    for (const auto& id : selection_->selectedIds()) {
        auto* row = new QWidget(this);
        auto* form = new QFormLayout(row);
        const QString label = id.startsWith(QStringLiteral("custom:")) ? id.mid(7) : id;
        auto* version = new QLineEdit(row);
        auto* notes = new QLineEdit(row);
        QString versionValue;
        QString notesValue;
        if (domain_ == Domain::Languages) {
            versionValue = capabilities.languageVersions.value(id);
            notesValue = capabilities.languageNotes.value(id);
        } else if (domain_ == Domain::Frameworks) {
            versionValue = capabilities.frameworkVersions.value(id);
            notesValue = capabilities.frameworkNotes.value(id);
        } else {
            versionValue = capabilities.toolchainVersions.value(id);
            notesValue = capabilities.toolchainNotes.value(id);
        }
        version->setObjectName(QStringLiteral("capabilityVersion_") + id);
        notes->setObjectName(QStringLiteral("capabilityNotes_") + id);
        version->setPlaceholderText(tr("Version (optional)"));
        notes->setPlaceholderText(tr("Notes (optional)"));
        version->setText(versionValue);
        notes->setText(notesValue);
        form->addRow(label, version);
        form->addRow(tr("Notes"), notes);
        connect(version, &QLineEdit::textEdited, this, [this, id](const QString& value) { store(id, QStringLiteral("version"), value); });
        connect(notes, &QLineEdit::textEdited, this, [this, id](const QString& value) { store(id, QStringLiteral("notes"), value); });
        layout_->addWidget(row);
    }
    if (selection_->selectedIds().isEmpty()) layout_->addWidget(new QLabel(tr("Select an item to add optional details."), this));
    layout_->addStretch();
}

void CapabilityDetailsEditor::store(const QString& id, const QString& field, const QString& value)
// Brief: Persist one edited detail without dropping other capability selections.
// Empty values remove only that optional detail; project capabilities and sibling metadata remain untouched.
{
    if (!model_) return;
    auto capabilities = model_->developmentCapabilities();
    QMap<QString, QString>* values = nullptr;
    if (domain_ == Domain::Languages) values = field == QStringLiteral("version") ? &capabilities.languageVersions : &capabilities.languageNotes;
    else if (domain_ == Domain::Frameworks) values = field == QStringLiteral("version") ? &capabilities.frameworkVersions : &capabilities.frameworkNotes;
    else values = field == QStringLiteral("version") ? &capabilities.toolchainVersions : &capabilities.toolchainNotes;
    if (value.trimmed().isEmpty()) values->remove(id);
    else values->insert(id, value.trimmed());
    storing_ = true;
    model_->setDevelopmentCapabilities(capabilities);
    storing_ = false;
}
