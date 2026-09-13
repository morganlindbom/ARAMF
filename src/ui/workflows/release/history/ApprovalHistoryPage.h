#pragma once

#include <QWidget>

class QTableWidget;
class ReleaseManagementService;

class ApprovalHistoryPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ApprovalHistoryPage(ReleaseManagementService* service,
                                 QWidget* parent = nullptr);

private:
    void refresh();
    ReleaseManagementService* service_;
    QTableWidget* table_;
};

