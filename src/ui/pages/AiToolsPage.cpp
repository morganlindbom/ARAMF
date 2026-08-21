#include "AiToolsPage.h"
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

AiToolsPage::AiToolsPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model), primaryAgent_(new QComboBox(this)), additionalAgent_(new QCheckBox(tr("ChatGPT"), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("<h2>AI</h2>Configure how AI participates in this project. These settings drive generated AGENTS.md, rules, routing and memory."), this));
    auto* agent = new QGroupBox(tr("AI Agent"), this); auto* agentLayout = new QVBoxLayout(agent);
    primaryAgent_->addItem(tr("Codex"), QStringLiteral("openai-codex")); primaryAgent_->addItem(tr("ChatGPT"), QStringLiteral("chatgpt"));
    agentLayout->addWidget(new QLabel(tr("Primary coding agent"), agent)); agentLayout->addWidget(primaryAgent_); agentLayout->addWidget(new QLabel(tr("Additional agents"), agent)); agentLayout->addWidget(additionalAgent_); layout->addWidget(agent);
    auto* modes = new QGroupBox(tr("AI Working Mode"), this); auto* modeLayout = new QVBoxLayout(modes);
    for (const auto& label : {tr("Planning"), tr("Coding"), tr("Review"), tr("Testing"), tr("Documentation")}) { auto* check = new QCheckBox(label, modes); modeLayout->addWidget(check); connect(check, &QCheckBox::toggled, this, [this, modes](bool) { QStringList values; for (auto* item : modes->findChildren<QCheckBox*>()) if (item->isChecked()) values << item->text(); model_->setOptionValues(QStringLiteral("ai-working-mode"), values); }); }
    layout->addWidget(modes);
    auto* configuration = new QGroupBox(tr("Agent Configuration"), this); auto* configurationLayout = new QVBoxLayout(configuration);
    for (const auto& label : {tr("AGENTS.md"), tr("Memory"), tr("Rules"), tr("Routing")}) { auto* check = new QCheckBox(label, configuration); check->setChecked(true); configurationLayout->addWidget(check); }
    layout->addWidget(configuration); layout->addStretch();
    connect(primaryAgent_, &QComboBox::currentIndexChanged, this, [this] { model_->setAiPlatforms({primaryAgent_->currentData().toString()}); });
    connect(additionalAgent_, &QCheckBox::toggled, this, [this](bool enabled) { QStringList values{primaryAgent_->currentData().toString()}; if (enabled) values << QStringLiteral("chatgpt"); model_->setAiPlatforms(values); });
    connect(model_, &ProjectModel::aiPlatformsChanged, this, &AiToolsPage::refreshFromModel); refreshFromModel();
}

void AiToolsPage::refreshFromModel()
{
    const auto platforms = model_->aiPlatforms(); const QSignalBlocker a(primaryAgent_); const QSignalBlocker b(additionalAgent_);
    const int index = primaryAgent_->findData(platforms.isEmpty() ? QStringLiteral("openai-codex") : platforms.first()); primaryAgent_->setCurrentIndex(index < 0 ? 0 : index); additionalAgent_->setChecked(platforms.size() > 1 && platforms.contains(QStringLiteral("chatgpt")));
}
