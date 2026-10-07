#include "ProjectAcademicPage.h"

#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

void addComboOptions(QComboBox* combo, const QList<EnvironmentOption>& options)
{
    for (const auto& option : options) {
        combo->addItem(option.first, option.second);
    }
}

}

ProjectAcademicPage::ProjectAcademicPage(ProjectModel* model, QWidget* parent, Section section)
    : QWidget(parent),
      model_(model),
      section_(section)
{
    auto* layout = new QVBoxLayout(this);
    const QString title = section_ == Section::Documentation ? tr("Documentation")
        : section_ == Section::Research ? tr("Thesis & research")
        : section_ == Section::Information ? tr("Academic information")
        : section_ == Section::Standards ? tr("Standards & languages")
        : section_ == Section::Deliverables ? tr("Requirements & deliverables") : tr("Academic");
    auto* heading = new QLabel(QStringLiteral("<h2>%1</h2>").arg(title.toHtmlEscaped()), this);
    layout->addWidget(heading);
    if (section_ != Section::All && section_ != Section::Documentation) {
        activationHint_ = new QLabel(tr("Select an academic document type on page 2.1 to enable these settings."), this);
        activationHint_->setWordWrap(true);
        layout->addWidget(activationHint_);
    }

    modeSection_ = new QGroupBox(tr("Documentation"), this);
    auto* modeLayout = new QVBoxLayout(modeSection_);
    projectTypes_ = new CapabilityCheckGroup(tr("Documentation"), EnvironmentCatalog::academicModes(), 2, modeSection_);
    modeLayout->addWidget(projectTypes_);
    layout->addWidget(modeSection_);

    details_ = new QGroupBox(this);
    details_->setFlat(true);
    auto* detailsLayout = new QVBoxLayout(details_);

    thesisLevelSection_ = new QGroupBox(tr("Thesis Level"), details_);
    auto* thesisLevelLayout = new QVBoxLayout(thesisLevelSection_);
    thesisLevel_ = new QComboBox(thesisLevelSection_);
    addComboOptions(thesisLevel_, EnvironmentCatalog::thesisLevels());
    thesisLevelLayout->addWidget(thesisLevel_);
    thesisLevelCustom_ = new QLineEdit(thesisLevelSection_);
    thesisLevelCustom_->setPlaceholderText(tr("Custom thesis level"));
    thesisLevelCustom_->setVisible(false);
    thesisLevelLayout->addWidget(thesisLevelCustom_);
    detailsLayout->addWidget(thesisLevelSection_);

    thesisApproaches_ = new CapabilityCheckGroup(
        tr("Thesis Type / Approach"), EnvironmentCatalog::thesisApproaches(), 3, details_);
    researchMethods_ = new CapabilityCheckGroup(
        tr("Research Method"), EnvironmentCatalog::researchMethods(), 3, details_);
    detailsLayout->addWidget(thesisApproaches_);
    detailsLayout->addWidget(researchMethods_);

    information_ = new QGroupBox(tr("Academic Information"), details_);
    auto* informationLayout = new QFormLayout(information_);
    institution_ = new QLineEdit(information_);
    programme_ = new QLineEdit(information_);
    supervisor_ = new QLineEdit(information_);
    examiner_ = new QLineEdit(information_);
    informationLayout->addRow(tr("Institution"), institution_);
    informationLayout->addRow(tr("Programme / Course"), programme_);
    informationLayout->addRow(tr("Supervisor"), supervisor_);
    informationLayout->addRow(tr("Examiner"), examiner_);
    detailsLayout->addWidget(information_);

    standards_ = new QGroupBox(tr("Academic Standards"), details_);
    auto* standardsLayout = new QFormLayout(standards_);
    citationStyle_ = new QComboBox(standards_);
    citationStyle_->setObjectName(QStringLiteral("academicCitationStyle"));
    addComboOptions(citationStyle_, EnvironmentCatalog::citationStyles());
    citationCustom_ = new QLineEdit(standards_);
    citationCustom_->setObjectName(QStringLiteral("academicCitationCustom"));
    citationCustom_->setPlaceholderText(tr("Custom citation style"));
    citationCustom_->setVisible(false);
    auto* citationLayout = new QVBoxLayout;
    citationLayout->addWidget(citationStyle_);
    citationLayout->addWidget(citationCustom_);
    standardsLayout->addRow(tr("Citation Style"), citationLayout);

    academicLanguages_ = new CapabilityCheckGroup(tr("Academic Languages"), EnvironmentCatalog::academicLanguages(), 3, standards_);
    academicLanguages_->setObjectName(QStringLiteral("academicLanguages"));
    standardsLayout->addRow(academicLanguages_);
    auto* languageHint = new QLabel(tr("Select one or more languages. Each selected document gets a separate version in each language."), standards_);
    languageHint->setWordWrap(true);
    standardsLayout->addRow(languageHint);
    detailsLayout->addWidget(standards_);

    requirements_ = new CapabilityCheckGroup(
        tr("Academic Requirements"), EnvironmentCatalog::academicRequirements(), 3, details_);
    deliverables_ = new CapabilityCheckGroup(
        tr("Academic Deliverables"), EnvironmentCatalog::academicDeliverables(), 3, details_);
    detailsLayout->addWidget(requirements_);
    detailsLayout->addWidget(deliverables_);
    layout->addWidget(details_);
    layout->addStretch();

    connect(projectTypes_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); updateVisibility(); });
    connect(thesisLevel_, &QComboBox::currentIndexChanged, this, [this] { persist(); updateVisibility(); });
    connect(thesisLevelCustom_, &QLineEdit::textChanged, this, [this] { persist(); });
    connect(thesisApproaches_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(researchMethods_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(requirements_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(deliverables_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    for (auto* field : {institution_, programme_, supervisor_, examiner_, citationCustom_}) {
        connect(field, &QLineEdit::textChanged, this, [this] { persist(); });
    }
    connect(citationStyle_, &QComboBox::currentIndexChanged, this, [this] { persist(); updateVisibility(); });
    connect(academicLanguages_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(model_, &ProjectModel::modelChanged, this, &ProjectAcademicPage::refresh);
    refresh();
}

QString ProjectAcademicPage::comboValue(const QComboBox* combo, const QLineEdit* customEdit)
{
    const QString id = combo->currentData().toString();
    if (id == QStringLiteral("custom") && customEdit && !customEdit->text().trimmed().isEmpty()) {
        return QStringLiteral("custom:%1").arg(customEdit->text().trimmed());
    }
    return id;
}

void ProjectAcademicPage::setComboValue(QComboBox* combo, QLineEdit* customEdit, const QString& value)
{
    QString stableId = value;
    QString customValue;
    if (value.startsWith(QStringLiteral("custom:"))) {
        stableId = QStringLiteral("custom");
        customValue = value.mid(7);
    }
    const QSignalBlocker comboBlocker(combo);
    const QSignalBlocker customBlocker(customEdit);
    const int index = combo->findData(stableId);
    combo->setCurrentIndex(index >= 0 ? index : 0);
    customEdit->setText(customValue);
    customEdit->setVisible(stableId == QStringLiteral("custom"));
}

void ProjectAcademicPage::persist()
{
    if (!model_ || !projectTypes_) return;
    AcademicConfiguration value = model_->academicConfiguration();
    value.projectTypes = projectTypes_->selectedIds();
    value.enabled = !value.projectTypes.isEmpty();
    value.academicMode = value.projectTypes.isEmpty() ? QStringLiteral("disabled") : value.projectTypes.first();
    value.thesisLevel = comboValue(thesisLevel_, thesisLevelCustom_);
    value.thesisApproaches = thesisApproaches_->selectedIds();
    value.researchMethods = researchMethods_->selectedIds();
    value.institution = institution_->text();
    value.programmeOrCourse = programme_->text();
    value.supervisor = supervisor_->text();
    value.examiner = examiner_->text();
    value.citationStyle = comboValue(citationStyle_, citationCustom_);
    value.academicLanguages = academicLanguages_->selectedIds();
    value.academicLanguage = value.academicLanguages.value(0);
    value.academicRequirements = requirements_->selectedIds();
    value.academicDeliverables = deliverables_->selectedIds();
    value.thesisDocumentation.enabled = value.projectTypes.contains(QStringLiteral("thesis-project"));
    value.reportDocumentation.enabled = value.projectTypes.contains(QStringLiteral("report-project"));
    model_->setAcademicConfiguration(value);
}

void ProjectAcademicPage::updateVisibility()
{
    const auto value = model_->academicConfiguration();
    const bool thesis = value.projectTypes.contains(QStringLiteral("thesis-project")) || value.thesisDocumentation.enabled;
    const bool research = value.projectTypes.contains(QStringLiteral("research-project")) || thesis;
    const bool all = section_ == Section::All;
    if (activationHint_) activationHint_->setVisible(!value.enabled && value.projectTypes.isEmpty());
    modeSection_->setVisible(all || section_ == Section::Documentation);
    details_->setVisible((value.enabled || !value.projectTypes.isEmpty()) && section_ != Section::Documentation);
    thesisLevelSection_->setVisible((all || section_ == Section::Research) && thesis);
    thesisApproaches_->setVisible((all || section_ == Section::Research) && thesis);
    researchMethods_->setVisible((all || section_ == Section::Research) && research);
    information_->setVisible(all || section_ == Section::Information);
    standards_->setVisible(all || section_ == Section::Standards);
    citationCustom_->setVisible(citationStyle_->currentData().toString() == QStringLiteral("custom"));
    requirements_->setVisible(all || section_ == Section::Deliverables);
    deliverables_->setVisible(all || section_ == Section::Deliverables);
}

void ProjectAcademicPage::refresh()
{
    const QSignalBlocker institutionBlocker(institution_), programmeBlocker(programme_),
        supervisorBlocker(supervisor_), examinerBlocker(examiner_);
    const auto value = model_->academicConfiguration();
    projectTypes_->setSelectedIds(value.projectTypes);
    setComboValue(thesisLevel_, thesisLevelCustom_, value.thesisLevel);
    thesisApproaches_->setSelectedIds(value.thesisApproaches);
    researchMethods_->setSelectedIds(value.researchMethods);
    institution_->setText(value.institution);
    programme_->setText(value.programmeOrCourse);
    supervisor_->setText(value.supervisor);
    examiner_->setText(value.examiner);
    setComboValue(citationStyle_, citationCustom_, value.citationStyle);
    academicLanguages_->setSelectedIds(value.academicLanguages);
    requirements_->setSelectedIds(value.academicRequirements);
    deliverables_->setSelectedIds(value.academicDeliverables);
    updateVisibility();
}
