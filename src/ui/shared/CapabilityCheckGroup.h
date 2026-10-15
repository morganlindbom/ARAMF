#pragma once

#include "core/EnvironmentCatalog.h"
#include "core/ProjectModel.h"

#include <QGroupBox>

class QLineEdit;
class QCheckBox;
class QGridLayout;
class QResizeEvent;
class QVBoxLayout;

class CapabilityCheckGroup final : public QGroupBox
{
    Q_OBJECT
public:
    explicit CapabilityCheckGroup(const QString& title, const QList<EnvironmentOption>& options, int columns = 3, QWidget* parent = nullptr);
    QStringList selectedIds() const;
    void setSelectedIds(const QStringList& ids);
    void setOptionEnabled(const QString& id, bool enabled);
    bool allSelectableSelected(bool includeCustom = false) const;
    void setAllSelected(bool selected, bool includeCustom = false);

signals:
    void selectionChanged(const QStringList& ids);

private:
    void reflow();
    QList<QCheckBox*> checks_;
    QLineEdit* customEdit_ = nullptr;
    QGridLayout* layout_ = nullptr;
    int requestedColumns_ = 1;
    int activeColumns_ = 0;

protected:
    void resizeEvent(QResizeEvent* event) override;
};

class CapabilityDetailsEditor final : public QGroupBox
{
public:
    enum class Domain { Languages, Frameworks, Toolchains };

    CapabilityDetailsEditor(ProjectModel* model, CapabilityCheckGroup* selection,
                            Domain domain, QWidget* parent = nullptr);
    // Brief: Create optional version and notes editors for selected catalog entries.
    // Metadata is keyed by stable capability ID, so custom values and legacy selections remain compatible.

private:
    void refresh();
    // Brief: Rebuild the detail rows from the current selection and model.
    // Rebuilding only follows model changes; local edits are committed directly to the metadata maps.
    void store(const QString& id, const QString& field, const QString& value);
    // Brief: Persist one changed field in the matching metadata map.
    // Updating one key leaves all capability selections and unrelated custom values intact.

    ProjectModel* model_ = nullptr;
    CapabilityCheckGroup* selection_ = nullptr;
    Domain domain_ = Domain::Languages;
    QVBoxLayout* layout_ = nullptr;
    bool storing_ = false;
};
