#pragma once

#include <QWidget>

class QLabel;
class QSpinBox;
class QTableWidget;
class ReleaseManagementService;

class ComponentVersionsPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ComponentVersionsPage(ReleaseManagementService* service,
                                    QWidget* parent = nullptr);

private:
    void refresh();
    ReleaseManagementService* service_;
    QTableWidget* table_;
    QSpinBox* target_;
    QLabel* status_;
};

