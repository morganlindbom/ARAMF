#include "ComponentVersionsPage.h"

#include "core/ComponentVersion.h"

#include <QFormLayout>
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ComponentVersionsPage::ComponentVersionsPage(ReleaseManagementService* service, QWidget* parent)
    : QWidget(parent), service_(service), table_(new QTableWidget(this)),
      target_(new QSpinBox(this)), status_(new QLabel(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Page & component versions</h2>Review the canonical four-part identity. This is not semantic versioning."), this));
    table_->setObjectName(QStringLiteral("componentVersionsTable"));
    table_->setColumnCount(7);
    table_->setHorizontalHeaderLabels({tr("Component"), tr("Version"), tr("Approved release"), tr("Parent / area"), tr("Item"), tr("Revision"), tr("Status")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_, 1);

    auto* approval = new QHBoxLayout;
    target_->setObjectName(QStringLiteral("componentApprovalTarget"));
    target_->setRange(1, 99);
    target_->setValue(1);
    approval->addWidget(new QLabel(tr("Explicit target:"), this));
    approval->addWidget(target_);
    auto* approve = new QPushButton(tr("Approve selected component"), this);
    approve->setObjectName(QStringLiteral("approveComponent"));
    approval->addWidget(approve);
    approval->addStretch();
    layout->addLayout(approval);
    status_->setObjectName(QStringLiteral("approvalStatus"));
    status_->setWordWrap(true);
    layout->addWidget(status_);

    connect(approve, &QPushButton::clicked, this, [this] {
        const int row = table_->currentRow();
        if (row < 0 || !service_) { status_->setText(tr("Select a component first.")); return; }
        const QString id = table_->item(row, 0)->data(Qt::UserRole).toString();
        QString error;
        if (service_->approveComponentForRelease(id, target_->value(), QStringLiteral("user"), &error))
            status_->setText(tr("Explicit approval recorded for %1.").arg(id));
        else
            status_->setText(tr("Approval not recorded: %1").arg(error));
    });
    if (service_) connect(service_, &ReleaseManagementService::registryChanged, this, &ComponentVersionsPage::refresh);
    refresh();
}

void ComponentVersionsPage::refresh()
{
    if (!service_) return;
    const auto components = service_->components();
    table_->setRowCount(components.size());
    for (int row = 0; row < components.size(); ++row) {
        const auto& component = components.at(row);
        auto* name = new QTableWidgetItem(component.displayName);
        name->setData(Qt::UserRole, component.id);
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, new QTableWidgetItem(component.version.toString()));
        table_->setItem(row, 2, new QTableWidgetItem(QString::number(component.version.approvedRelease)));
        table_->setItem(row, 3, new QTableWidgetItem(QString::number(component.version.parent)));
        table_->setItem(row, 4, new QTableWidgetItem(QString::number(component.version.item)));
        table_->setItem(row, 5, new QTableWidgetItem(QString::number(component.version.revision)));
        table_->setItem(row, 6, new QTableWidgetItem(component.releaseRequired
                                                          ? tr("Required") : tr("Optional")));
    }
}
