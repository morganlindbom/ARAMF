#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class QButtonGroup;
class QComboBox;
class QGroupBox;
class QLineEdit;
class CapabilityCheckGroup;

class ProjectAcademicPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectAcademicPage(ProjectModel* model, QWidget* parent = nullptr);

private:
    void refresh();
    void persist();
    void updateVisibility();
    static QString comboValue(const QComboBox* combo, const QLineEdit* customEdit);
    static void setComboValue(QComboBox* combo, QLineEdit* customEdit, const QString& value);

    ProjectModel* model_ = nullptr;
    QButtonGroup* modeGroup_ = nullptr;
    QLineEdit* modeCustom_ = nullptr;
    QGroupBox* details_ = nullptr;
    QGroupBox* thesisLevelSection_ = nullptr;
    QComboBox* thesisLevel_ = nullptr;
    QLineEdit* thesisLevelCustom_ = nullptr;
    CapabilityCheckGroup* thesisApproaches_ = nullptr;
    CapabilityCheckGroup* researchMethods_ = nullptr;
    QLineEdit* institution_ = nullptr;
    QLineEdit* programme_ = nullptr;
    QLineEdit* supervisor_ = nullptr;
    QLineEdit* examiner_ = nullptr;
    QComboBox* citationStyle_ = nullptr;
    QLineEdit* citationCustom_ = nullptr;
    QComboBox* academicLanguage_ = nullptr;
    QLineEdit* languageCustom_ = nullptr;
    CapabilityCheckGroup* requirements_ = nullptr;
    CapabilityCheckGroup* deliverables_ = nullptr;
};
