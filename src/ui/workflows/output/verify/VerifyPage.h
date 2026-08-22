#pragma once

#include "core/Services.h"

#include <QWidget>

class QListWidget;
class QLabel;
class QPushButton;

class VerifyPage final : public QWidget
{
    Q_OBJECT
public:
    VerifyPage(ProjectModel* model, VerificationServices* services, QWidget* parent = nullptr);

private:
    void runVerification();
    void showResult(const VerificationResult& result);
    ProjectModel* model_ = nullptr;
    VerificationServices* services_ = nullptr;
    QLabel* status_ = nullptr;
    QListWidget* checks_ = nullptr;
    QPushButton* verifyButton_ = nullptr;
};
