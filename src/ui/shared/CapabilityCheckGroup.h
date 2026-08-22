#pragma once

#include "core/EnvironmentCatalog.h"

#include <QGroupBox>

class QLineEdit;
class QCheckBox;

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
    QList<QCheckBox*> checks_;
    QLineEdit* customEdit_ = nullptr;
};
