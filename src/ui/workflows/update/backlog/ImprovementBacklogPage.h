#pragma once

#include <QSet>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class ProjectModel;

class ImprovementBacklogPage final : public QWidget
{
    Q_OBJECT
public:
    explicit ImprovementBacklogPage(ProjectModel* model, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    void refreshDetails();
    QSet<QString> selectedIds() const;

    QListWidget* items_ = nullptr;
    QComboBox* filter_ = nullptr;
    QLabel* summary_ = nullptr;
    QPlainTextEdit* details_ = nullptr;
    QLineEdit* title_ = nullptr;
    QPlainTextEdit* observation_ = nullptr;
    QLineEdit* expected_ = nullptr;
    QLineEdit* area_ = nullptr;
    QLineEdit* evidence_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* promote_ = nullptr;
    QPushButton* projectSpecific_ = nullptr;
    QPushButton* duplicate_ = nullptr;
    QPushButton* moreEvidence_ = nullptr;
    QPushButton* resolved_ = nullptr;
    QPushButton* reject_ = nullptr;
    QPushButton* delete_ = nullptr;
    QComboBox* priority_ = nullptr;
    QLineEdit* duplicateOf_ = nullptr;
    QComboBox* lifecycle_ = nullptr;
    QSet<QString> selectedIds_;
    ProjectModel* model_ = nullptr;
};
