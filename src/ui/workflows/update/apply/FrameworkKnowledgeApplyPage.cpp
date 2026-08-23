#include "FrameworkKnowledgeApplyPage.h"

#include "core/FrameworkKnowledge.h"
#include "core/UpdateService.h"
#include "core/UpdateExecutionService.h"
#include "core/CodexExecutionAdapter.h"

#include <QHBoxLayout>
#include <QDir>
#include <QJsonArray>
#include <QJsonValue>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

namespace {
QString jsonList(const QJsonValue& value)
{
    QStringList values;
    for (const auto& item : value.toArray()) values.append(item.toString());
    return values.join(QStringLiteral(", "));
}

QString codexStatusText(const ProjectModel& model)
{
    if (model.aiConfiguration().primaryAgent != QStringLiteral("openai-codex")) return {};
    const auto resolution = CodexExecutionAdapter::resolution();
    return QObject::tr("\nCodex status: %1\nCodex version: %2\nCodex executable: %3")
        .arg(resolution.available ? QObject::tr("Available") : QObject::tr("Not found"),
             resolution.version,
             resolution.path.isEmpty() ? resolution.error : resolution.path);
}
}

FrameworkKnowledgeApplyPage::FrameworkKnowledgeApplyPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), entries_(new QListWidget(this)), analysis_(new QPlainTextEdit(this)), status_(new QLabel(this)),
      analyze_(new QPushButton(tr("Analyze Update"), this)), apply_(new QPushButton(tr("Prepare Update"), this)),
      selectAll_(new QPushButton(tr("Select All"), this)), clearAll_(new QPushButton(tr("Clear All"), this)),
      execute_(new QPushButton(tr("Execute Update"), this)), validate_(new QPushButton(tr("Validate Update"), this)),
      executionInfo_(new QLabel(this)), execution_(new UpdateExecutionService(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>Apply Framework Knowledge</h2>Which approved knowledge should update the project?"), this));
    layout->addWidget(new QLabel(tr("Select approved knowledge to analyze for this project. Relevant knowledge can be adopted; ARAMF determines whether implementation changes are required."), this));
    executionInfo_->setObjectName(QStringLiteral("updateExecutionInfo"));
    executionInfo_->setWordWrap(true);
    layout->addWidget(executionInfo_);
    entries_->setObjectName(QStringLiteral("approvedFrameworkKnowledgeSelection"));
    layout->addWidget(entries_);
    status_->setObjectName(QStringLiteral("updateStatus"));
    layout->addWidget(status_);
    analysis_->setReadOnly(true);
    analysis_->setObjectName(QStringLiteral("updateImpactAnalysis"));
    layout->addWidget(analysis_);
    auto* selectionActions = new QHBoxLayout;
    selectAll_->setObjectName(QStringLiteral("selectAllFrameworkKnowledge"));
    clearAll_->setObjectName(QStringLiteral("clearAllFrameworkKnowledge"));
    selectionActions->addWidget(selectAll_);
    selectionActions->addWidget(clearAll_);
    selectionActions->addStretch();
    layout->addLayout(selectionActions);
    auto* actions = new QHBoxLayout;
    analyze_->setObjectName(QStringLiteral("analyzeFrameworkUpdate"));
    apply_->setObjectName(QStringLiteral("applyFrameworkUpdate"));
    execute_->setObjectName(QStringLiteral("executeFrameworkUpdate"));
    execute_->setEnabled(false);
    validate_->setObjectName(QStringLiteral("validateFrameworkUpdate"));
    validate_->setEnabled(false);
    actions->addWidget(analyze_); actions->addWidget(apply_); actions->addWidget(execute_); actions->addWidget(validate_); actions->addStretch();
    layout->addLayout(actions);
    connect(selectAll_, &QPushButton::clicked, this, [this] {
        for (int row = 0; row < entries_->count(); ++row) {
            auto* item = entries_->item(row);
            if (item->flags() & Qt::ItemIsUserCheckable) item->setCheckState(Qt::Checked);
        }
    });
    connect(clearAll_, &QPushButton::clicked, this, [this] {
        for (int row = 0; row < entries_->count(); ++row) entries_->item(row)->setCheckState(Qt::Unchecked);
    });
    connect(analyze_, &QPushButton::clicked, this, [this] {
        UpdateService service;
        const auto result = service.analyze(model_->projectPath(), *model_, selectedIds());
        status_->setText(result.success ? tr("Analysis: %1").arg(result.status) : tr("Analysis failed: %1").arg(result.error));
        if (result.success) {
            const auto plan = result.plan;
            analysis_->setPlainText(tr("Selected knowledge: %1\nAffected scopes: %2\nAffected systems: %3\nExpected areas: %4\nConflicts: %5\nRecommended validation: %6\nPlan: %7")
                                   .arg(plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray().size())
                                   .arg(jsonList(plan.value(QStringLiteral("affectedScopes"))))
                                   .arg(jsonList(plan.value(QStringLiteral("affectedSystems"))))
                                   .arg(jsonList(plan.value(QStringLiteral("expectedAreas"))))
                                   .arg(jsonList(plan.value(QStringLiteral("conflicts"))))
                                   .arg(plan.value(QStringLiteral("recommendedValidationLevel")).toString(), result.planPath));
            for (int row = 0; row < entries_->count(); ++row) {
                auto* item = entries_->item(row);
                const QString id = item->data(Qt::UserRole).toString();
                for (const auto& selected : plan.value(QStringLiteral("selectedFrameworkKnowledge")).toArray()) {
                    const auto value = selected.toObject();
                    if (value.value(QStringLiteral("id")).toString() == id) {
                        item->setText(QStringLiteral("[%1] %2").arg(value.value(QStringLiteral("classification")).toString(), item->text()));
                        break;
                    }
                }
            }
            apply_->setEnabled(!result.blockedByAuthority && result.status != QStringLiteral("conflict"));
        } else apply_->setEnabled(false);
    });
    connect(apply_, &QPushButton::clicked, this, [this] {
        UpdateService service; QString error;
        if (service.apply(model_->projectPath(), *model_, &error)) {
            status_->setText(tr("READY - selected knowledge and classifications preserved. Adoption and implementation are pending Execute."));
            apply_->setEnabled(false);
            execute_->setEnabled(true);
        } else status_->setText(tr("Apply blocked: %1").arg(error));
    });
    connect(execute_, &QPushButton::clicked, this, [this] {
        QString error;
        if (!execution_->execute(model_->projectPath(), *model_, &error)) {
            status_->setText(tr("Execution not started: %1").arg(error));
        }
    });
    connect(validate_, &QPushButton::clicked, this, [this] {
        QString error;
        if (!execution_->validate(model_->projectPath(), &error)) status_->setText(tr("Validation not started: %1").arg(error));
    });
    connect(execution_, &UpdateExecutionService::stateChanged, this, [this](const QString& state) {
        status_->setText(tr("Execution state: %1").arg(state));
        executionInfo_->setText(tr("Managed project: %1\nControl plane: %2\nConfigured agent: %3")
                                 .arg(model_->projectPath(), QDir(model_->projectPath()).filePath(QStringLiteral("ARAMF_WORKER")), model_->aiConfiguration().primaryAgent)
                                 + codexStatusText(*model_));
        validate_->setEnabled(state == QStringLiteral("AWAITING_VALIDATION"));
        if (state == QStringLiteral("AWAITING_VALIDATION") || state == QStringLiteral("COMPLETED") || state == QStringLiteral("FAILED")) execute_->setEnabled(false);
    });
    connect(model_, &ProjectModel::modelChanged, this, &FrameworkKnowledgeApplyPage::refresh);
    refresh();
}

QStringList FrameworkKnowledgeApplyPage::selectedIds() const
{
    QStringList ids;
    for (int i = 0; i < entries_->count(); ++i) if (entries_->item(i)->checkState() == Qt::Checked) ids.append(entries_->item(i)->data(Qt::UserRole).toString());
    return ids;
}

void FrameworkKnowledgeApplyPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

void FrameworkKnowledgeApplyPage::refresh()
{
    entries_->clear();
    UpdateService service;
    QString error;
    const auto allEntries = service.approvedKnowledgeForProject(model_->projectPath(), *model_, &error);
    const auto applicableEntries = service.applicableApprovedKnowledge(model_->projectPath(), *model_, &error);
    const auto applicableIds = [&applicableEntries] {
        QStringList ids;
        for (const auto& entry : applicableEntries) ids.append(entry.id);
        return ids;
    }();
    for (const auto& entry : allEntries) {
        const bool applicable = applicableIds.contains(entry.id);
        auto* item = new QListWidgetItem(QStringLiteral("[Needs analysis] %1 - %2 (%3)").arg(entry.title, entry.id, entry.origin), entries_);
        item->setData(Qt::UserRole, entry.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        if (!applicable) item->setToolTip(tr("Applicability will be determined during analysis; selection is still allowed."));
    }
    const int nonApplicable = qMax(0, allEntries.size() - applicableEntries.size());
    status_->setText(tr("Approved and applicable: %1. Approved but not applicable: %2.").arg(applicableEntries.size()).arg(nonApplicable));
    analysis_->clear();
    apply_->setEnabled(false);
    execute_->setEnabled(false);
    validate_->setEnabled(false);
    executionInfo_->setText(tr("Managed project: %1\nControl plane: %2\nConfigured agent: %3")
                             .arg(model_->projectPath(), QDir(model_->projectPath()).filePath(QStringLiteral("ARAMF_WORKER")), model_->aiConfiguration().primaryAgent)
                             + codexStatusText(*model_));
}
