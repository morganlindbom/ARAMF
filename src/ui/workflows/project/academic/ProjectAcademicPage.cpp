#include "ProjectAcademicPage.h"

#include "core/EnvironmentCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
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

ProjectAcademicPage::ProjectAcademicPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Academic</h2>Configure academic, research and thesis requirements for this project."), this));

    auto* modeSection = new QGroupBox(tr("Academic Mode"), this);
    auto* modeLayout = new QVBoxLayout(modeSection);
    modeGroup_ = new QButtonGroup(this);
    for (const auto& option : EnvironmentCatalog::academicModes()) {
        auto* button = new QRadioButton(option.first, modeSection);
        button->setProperty("academicId", option.second);
        modeGroup_->addButton(button);
        modeLayout->addWidget(button);
        if (option.second == QStringLiteral("custom")) {
            modeCustom_ = new QLineEdit(modeSection);
            modeCustom_->setPlaceholderText(tr("Custom academic mode"));
            modeCustom_->setVisible(false);
            modeLayout->addWidget(modeCustom_);
        }
    }
    layout->addWidget(modeSection);

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

    auto* information = new QGroupBox(tr("Academic Information"), details_);
    auto* informationLayout = new QFormLayout(information);
    institution_ = new QLineEdit(information);
    programme_ = new QLineEdit(information);
    supervisor_ = new QLineEdit(information);
    examiner_ = new QLineEdit(information);
    informationLayout->addRow(tr("Institution"), institution_);
    informationLayout->addRow(tr("Programme / Course"), programme_);
    informationLayout->addRow(tr("Supervisor"), supervisor_);
    informationLayout->addRow(tr("Examiner"), examiner_);
    detailsLayout->addWidget(information);

    auto* standards = new QGroupBox(tr("Academic Standards"), details_);
    auto* standardsLayout = new QFormLayout(standards);
    citationStyle_ = new QComboBox(standards);
    addComboOptions(citationStyle_, EnvironmentCatalog::citationStyles());
    citationCustom_ = new QLineEdit(standards);
    citationCustom_->setPlaceholderText(tr("Custom citation style"));
    citationCustom_->setVisible(false);
    auto* citationLayout = new QVBoxLayout;
    citationLayout->addWidget(citationStyle_);
    citationLayout->addWidget(citationCustom_);
    standardsLayout->addRow(tr("Citation Style"), citationLayout);

    academicLanguage_ = new QComboBox(standards);
    addComboOptions(academicLanguage_, EnvironmentCatalog::academicLanguages());
    languageCustom_ = new QLineEdit(standards);
    languageCustom_->setPlaceholderText(tr("Custom academic language"));
    languageCustom_->setVisible(false);
    auto* languageLayout = new QVBoxLayout;
    languageLayout->addWidget(academicLanguage_);
    languageLayout->addWidget(languageCustom_);
    standardsLayout->addRow(tr("Academic Language"), languageLayout);
    detailsLayout->addWidget(standards);

    requirements_ = new CapabilityCheckGroup(
        tr("Academic Requirements"), EnvironmentCatalog::academicRequirements(), 3, details_);
    deliverables_ = new CapabilityCheckGroup(
        tr("Academic Deliverables"), EnvironmentCatalog::academicDeliverables(), 3, details_);
    detailsLayout->addWidget(requirements_);
    detailsLayout->addWidget(deliverables_);
    layout->addWidget(details_);
    layout->addStretch();

    connect(modeGroup_, &QButtonGroup::idClicked, this, [this] { persist(); updateVisibility(); });
    connect(modeCustom_, &QLineEdit::textChanged, this, [this] { persist(); });
    connect(thesisLevel_, &QComboBox::currentIndexChanged, this, [this] { persist(); updateVisibility(); });
    connect(thesisLevelCustom_, &QLineEdit::textChanged, this, [this] { persist(); });
    connect(thesisApproaches_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(researchMethods_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(requirements_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(deliverables_, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    for (auto* field : {institution_, programme_, supervisor_, examiner_, citationCustom_, languageCustom_}) {
        connect(field, &QLineEdit::textChanged, this, [this] { persist(); });
    }
    connect(citationStyle_, &QComboBox::currentIndexChanged, this, [this] { persist(); });
    connect(academicLanguage_, &QComboBox::currentIndexChanged, this, [this] { persist(); });
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
    if (!model_ || !modeGroup_) return;
    AcademicConfiguration value;
    if (auto* button = modeGroup_->checkedButton()) {
        value.academicMode = button->property("academicId").toString();
        if (value.academicMode == QStringLiteral("custom") && !modeCustom_->text().trimmed().isEmpty()) {
            value.academicMode = QStringLiteral("custom:%1").arg(modeCustom_->text().trimmed());
        }
    }
    value.thesisLevel = comboValue(thesisLevel_, thesisLevelCustom_);
    value.thesisApproaches = thesisApproaches_->selectedIds();
    value.researchMethods = researchMethods_->selectedIds();
    value.institution = institution_->text();
    value.programmeOrCourse = programme_->text();
    value.supervisor = supervisor_->text();
    value.examiner = examiner_->text();
    value.citationStyle = comboValue(citationStyle_, citationCustom_);
    value.academicLanguage = comboValue(academicLanguage_, languageCustom_);
    value.academicRequirements = requirements_->selectedIds();
    value.academicDeliverables = deliverables_->selectedIds();
    model_->setAcademicConfiguration(value);
}

void ProjectAcademicPage::updateVisibility()
{
    const auto value = model_->academicConfiguration();
    const bool enabled = value.academicMode != QStringLiteral("disabled");
    details_->setVisible(enabled);
    thesisLevelSection_->setVisible(value.academicMode == QStringLiteral("thesis"));
}

void ProjectAcademicPage::refresh()
{
    const auto value = model_->academicConfiguration();
    for (auto* button : modeGroup_->buttons()) {
        const QSignalBlocker blocker(button);
        const QString id = button->property("academicId").toString();
        const bool custom = value.academicMode.startsWith(QStringLiteral("custom:"));
        button->setChecked((custom && id == QStringLiteral("custom")) || id == value.academicMode);
    }
    if (modeCustom_) {
        const QSignalBlocker blocker(modeCustom_);
        modeCustom_->setText(value.academicMode.startsWith(QStringLiteral("custom:"))
                                 ? value.academicMode.mid(7) : QString());
        modeCustom_->setVisible(value.academicMode.startsWith(QStringLiteral("custom:")));
    }
    setComboValue(thesisLevel_, thesisLevelCustom_, value.thesisLevel);
    thesisApproaches_->setSelectedIds(value.thesisApproaches);
    researchMethods_->setSelectedIds(value.researchMethods);
    institution_->setText(value.institution);
    programme_->setText(value.programmeOrCourse);
    supervisor_->setText(value.supervisor);
    examiner_->setText(value.examiner);
    setComboValue(citationStyle_, citationCustom_, value.citationStyle);
    setComboValue(academicLanguage_, languageCustom_, value.academicLanguage);
    requirements_->setSelectedIds(value.academicRequirements);
    deliverables_->setSelectedIds(value.academicDeliverables);
    updateVisibility();
}
