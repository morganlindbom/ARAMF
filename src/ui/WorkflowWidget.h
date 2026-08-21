#pragma once

#include <QWidget>
#include <QList>

class QListWidget;
class QPushButton;

class WorkflowWidget final : public QWidget {
    Q_OBJECT
public:
    explicit WorkflowWidget(QWidget* parent = nullptr);
    int currentStep() const;
    void setCurrentStep(int index);
    void setStepCount(int count);
    void setStepEnabled(int index, bool enabled, const QString& reason = {});

signals:
    void stepSelected(int index);
    void backRequested();
    void forwardRequested();

private:
    QListWidget* steps_;
    QPushButton* back_;
    QPushButton* forward_;
    QList<int> pageRows_;
};
