#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class QComboBox;
class QGroupBox;
class QLineEdit;
class QLabel;
class CapabilityCheckGroup;

class ProjectAcademicPage final : public QWidget
{
    Q_OBJECT

public:
    enum class Section { All, Documentation, Research, Information, Standards, Deliverables };
    explicit ProjectAcademicPage(ProjectModel* model, QWidget* parent = nullptr, Section section = Section::All);

private:
    void refresh();
    void persist();
    void updateVisibility();
    static QString comboValue(const QComboBox* combo, const QLineEdit* customEdit);
    static void setComboValue(QComboBox* combo, QLineEdit* customEdit, const QString& value);

    ProjectModel* model_ = nullptr;
    Section section_ = Section::All;
    QLabel* activationHint_ = nullptr;
    QGroupBox* modeSection_ = nullptr;
    CapabilityCheckGroup* projectTypes_ = nullptr;
    QGroupBox* details_ = nullptr;
    QGroupBox* thesisLevelSection_ = nullptr;
    QComboBox* thesisLevel_ = nullptr;
    QLineEdit* thesisLevelCustom_ = nullptr;
    CapabilityCheckGroup* thesisApproaches_ = nullptr;
    CapabilityCheckGroup* researchMethods_ = nullptr;
    QGroupBox* information_ = nullptr;
    QLineEdit* institution_ = nullptr;
    QLineEdit* programme_ = nullptr;
    QLineEdit* supervisor_ = nullptr;
    QLineEdit* examiner_ = nullptr;
    QGroupBox* standards_ = nullptr;
    QComboBox* citationStyle_ = nullptr;
    QLineEdit* citationCustom_ = nullptr;
    CapabilityCheckGroup* academicLanguages_ = nullptr;
    CapabilityCheckGroup* requirements_ = nullptr;
    CapabilityCheckGroup* deliverables_ = nullptr;
};
