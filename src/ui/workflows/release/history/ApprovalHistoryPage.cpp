#include "ApprovalHistoryPage.h"

#include "core/ComponentVersion.h"

#include <QHeaderView>
#include <QAbstractItemView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ApprovalHistoryPage::ApprovalHistoryPage(ReleaseManagementService* service, QWidget* parent)
    : QWidget(parent), service_(service), table_(new QTableWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Approval & release history</h2>Only explicit authorized human actions create approval records. Build, validation, Generate, Save Work and completion markers never approve releases."), this));
    table_->setObjectName(QStringLiteral("approvalHistoryTable"));
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({tr("Component"), tr("Version"), tr("Target"), tr("Product"), tr("Authority"), tr("Approved at")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);
    layout->addWidget(new QLabel(tr("Approval history is append-only in the application-owned release registry."), this));
    if (service_) connect(service_, &ReleaseManagementService::registryChanged, this, &ApprovalHistoryPage::refresh);
    refresh();
}

void ApprovalHistoryPage::refresh()
{
    if (!service_) return;
    const auto history = service_->approvalHistory();
    table_->setRowCount(history.size());
    for (int row = 0; row < history.size(); ++row) {
        const auto& approval = history.at(row);
        table_->setItem(row, 0, new QTableWidgetItem(approval.componentId));
        table_->setItem(row, 1, new QTableWidgetItem(approval.componentVersion));
        table_->setItem(row, 2, new QTableWidgetItem(QString::number(approval.targetRelease)));
        table_->setItem(row, 3, new QTableWidgetItem(approval.productVersion));
        table_->setItem(row, 4, new QTableWidgetItem(approval.authority));
        table_->setItem(row, 5, new QTableWidgetItem(approval.approvedAt));
    }
}
