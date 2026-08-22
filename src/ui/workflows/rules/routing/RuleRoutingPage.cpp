#include "RuleRoutingPage.h"

#include "core/ProjectModel.h"
#include "core/RuleCatalog.h"
#include "ui/shared/CapabilityCheckGroup.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

RuleRoutingPage::RuleRoutingPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>How should rules be routed?</h2>Define when and where ARAMF should load project rules."), this));
    auto* form = new QFormLayout;
    loading_ = new QComboBox(this);
    loading_->addItem(tr("Load only relevant rules"), QStringLiteral("relevant"));
    loading_->addItem(tr("Always load all active rules"), QStringLiteral("always"));
    loading_->addItem(tr("Load only explicitly requested rules"), QStringLiteral("explicit"));
    loading_->addItem(tr("Metadata first, content when relevant"), QStringLiteral("metadata-first"));
    conflict_ = new QComboBox(this);
    for (const auto& item : {qMakePair(tr("Prefer explicit user instruction"), QStringLiteral("prefer-user-instruction")), qMakePair(tr("Prefer project Source of Truth"), QStringLiteral("prefer-source-of-truth")), qMakePair(tr("Prefer durable project decision"), QStringLiteral("prefer-durable-decision")), qMakePair(tr("Prefer project-specific rule"), QStringLiteral("prefer-project-rule")), qMakePair(tr("Require manual resolution"), QStringLiteral("manual-resolution"))}) conflict_->addItem(item.first, item.second);
    form->addRow(tr("Default Rule Loading Strategy"), loading_);
    form->addRow(tr("When rules conflict"), conflict_);
    layout->addLayout(form);
    workScopes_ = new CapabilityCheckGroup(tr("Apply rules when AI works on"), RuleCatalog::workScopes(), 3, this);
    projectScopes_ = new CapabilityCheckGroup(tr("Apply rules to"), RuleCatalog::projectScopes(), 3, this);
    contextPolicies_ = new CapabilityCheckGroup(tr("Context / Token Efficiency"), RuleCatalog::contextPolicies(), 3, this);
    layout->addWidget(workScopes_); layout->addWidget(projectScopes_); layout->addWidget(contextPolicies_); layout->addStretch();
    connect(loading_, &QComboBox::currentIndexChanged, this, [this] { persist(); });
    connect(conflict_, &QComboBox::currentIndexChanged, this, [this] { persist(); });
    for (auto* group : {workScopes_, projectScopes_, contextPolicies_}) connect(group, &CapabilityCheckGroup::selectionChanged, this, [this] { persist(); });
    connect(model_, &ProjectModel::modelChanged, this, &RuleRoutingPage::refresh);
    refresh();
}

void RuleRoutingPage::persist()
{
    auto value = model_->ruleConfiguration();
    value.loadingStrategy = loading_->currentData().toString(); value.conflictPolicy = conflict_->currentData().toString();
    value.workScopes = workScopes_->selectedIds(); value.projectScopes = projectScopes_->selectedIds(); value.contextPolicies = contextPolicies_->selectedIds();
    model_->setRuleConfiguration(value);
}

void RuleRoutingPage::refresh()
{
    const auto value = model_->ruleConfiguration();
    loading_->setCurrentIndex(loading_->findData(value.loadingStrategy)); conflict_->setCurrentIndex(conflict_->findData(value.conflictPolicy));
    workScopes_->setSelectedIds(value.workScopes); projectScopes_->setSelectedIds(value.projectScopes); contextPolicies_->setSelectedIds(value.contextPolicies);
}
