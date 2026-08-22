#include "GeneratePage.h"

#include "ui/workflows/project/setup/ProjectSetupPage.h"

#include <algorithm>
#include <QDir>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

GeneratePage::GeneratePage(ProjectModel* model, ProjectSetupPage* setupPage,
                           GenerationServices* services, QWidget* parent)
    : QWidget(parent), model_(model), setupPage_(setupPage), services_(services)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Generate</h2>Select the ARAMF output products, then generate them into the target project."),
        this));

    auto* outputs = new QGroupBox(tr("Output products"), this);
    auto* outputLayout = new QVBoxLayout(outputs);
    const QList<QPair<QString, QString>> products{
        {tr("AGENTS.md bootstrap and ARAMF agent rules"), QStringLiteral("agent-rules")},
        {tr("Routing manifests"), QStringLiteral("routing")},
        {tr("Platform and environment metadata"), QStringLiteral("platforms")},
        {tr("Resource manifest"), QStringLiteral("resources")},
        {tr("Project Memory artifacts"), QStringLiteral("memory")},
        {tr("Provenance and selection effects"), QStringLiteral("provenance")}
    };
    for (const auto& product : products) {
        auto* box = new QCheckBox(product.first, outputs);
        box->setProperty("generationProduct", product.second);
        const auto current = model_->generationOptions();
        const bool checked = product.second == QStringLiteral("agent-rules") ? current.generateAgentRules
            : product.second == QStringLiteral("routing") ? current.generateRouting
            : product.second == QStringLiteral("platforms") ? current.generatePlatforms
            : product.second == QStringLiteral("resources") ? current.generateResources
            : product.second == QStringLiteral("memory") ? current.generateMemory
            : current.generateProvenance;
        box->setChecked(checked);
        outputProducts_.append(box);
        outputLayout->addWidget(box);
        connect(box, &QCheckBox::toggled, this, &GeneratePage::updateSelectAllText);
        connect(box, &QCheckBox::toggled, this, [this] { syncOptionsToModel(); });
    }
    layout->addWidget(outputs);

    selectAll_ = new QPushButton(tr("Clear All"), this);
    selectAll_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    layout->addWidget(selectAll_);
    connect(selectAll_, &QPushButton::clicked, this, [this] {
        const bool select = selectAll_->text() == tr("Select All");
        for (auto* product : outputProducts_) product->setChecked(select);
        updateSelectAllText();
    });

    auto* generateButton = new QPushButton(tr("Save & Generate"), this);
    layout->addWidget(generateButton);

    result_ = new QPlainTextEdit(this);
    result_->setReadOnly(true);
    layout->addWidget(result_);

    connect(generateButton, &QPushButton::clicked, this, [this] {
        const GenerationOptions options = selectedOptions();
        const bool hasProjectPath = !model_->projectPath().trimmed().isEmpty()
            && QDir::cleanPath(model_->projectPath().trimmed()) != QStringLiteral(".");
        const bool hasProduct = options.generateAgentRules || options.generateRouting
            || options.generatePlatforms || options.generateResources
            || options.generateMemory || options.generateProvenance;
        if (!hasProjectPath) {
            result_->setPlainText(tr("Save: NOT RUN\nGenerate: NOT RUN\n\nSave and Generate stopped: choose a Project Path first."));
            return;
        }
        if (!hasProduct) {
            result_->setPlainText(tr("Save: NOT RUN\nGenerate: NOT RUN\n\nSelect at least one output product."));
            return;
        }

        QString saveError;
        if (!setupPage_->saveForGeneration(&saveError)) {
            result_->setPlainText(tr("Save: FAIL\nGenerate: NOT RUN\n\nReason:\n%1").arg(saveError));
            return;
        }
        const auto result = services_->generate(*model_, options);
        showResult(result);
    });
    connect(model_, &ProjectModel::modelChanged, this, [this] {
        const auto options = model_->generationOptions();
        const QList<bool> values{options.generateAgentRules, options.generateRouting,
                                 options.generatePlatforms, options.generateResources,
                                 options.generateMemory, options.generateProvenance};
        for (int i = 0; i < outputProducts_.size(); ++i) {
            QSignalBlocker blocker(outputProducts_.at(i));
            outputProducts_.at(i)->setChecked(values.at(i));
        }
        updateSelectAllText();
    });
    updateSelectAllText();
}

GenerationOptions GeneratePage::selectedOptions() const
{
    GenerationOptions options;
    options.generateAgentRules = outputProducts_.at(0)->isChecked();
    options.generateRouting = outputProducts_.at(1)->isChecked();
    options.generatePlatforms = outputProducts_.at(2)->isChecked();
    options.generateResources = outputProducts_.at(3)->isChecked();
    options.generateMemory = outputProducts_.at(4)->isChecked();
    options.generateProvenance = outputProducts_.at(5)->isChecked();
    return options;
}

void GeneratePage::syncOptionsToModel() const
{
    model_->setGenerationOptions(selectedOptions());
}

void GeneratePage::updateSelectAllText()
{
    const bool allSelected = !outputProducts_.isEmpty()
                             && std::all_of(outputProducts_.cbegin(), outputProducts_.cend(), [](auto* product) {
                                    return product->isChecked();
                                });
    selectAll_->setText(allSelected ? tr("Clear All") : tr("Select All"));
}

void GeneratePage::showResult(const GenerationResult& result)
{
    QString text;
    if (result.success) {
        text += tr("Save: PASS\nGenerate: PASS\n\n");
    } else {
        text += tr("Save: PASS\nGenerate: FAIL\n\n");
    }
    if (!result.error.isEmpty()) text += tr("Error:\n%1\n\n").arg(result.error);
    text += tr("Project configuration:\n%1\n\nProject ID:\n%2\n\nSaved configuration:\n%3\n\nGenerated project:\n%4\n\n")
                .arg(model_->projectName(), model_->projectId(),
                     model_->projectFilePath(), model_->projectPath());
    text += tr("Generated products:\n");
    if (result.generatedFiles.isEmpty()) text += tr("- None\n");
    for (const auto& file : result.generatedFiles) text += QStringLiteral("- %1\n").arg(file);
    text += tr("\nSkipped products:\n");
    if (result.skippedProducts.isEmpty()) text += tr("- None\n");
    for (const auto& product : result.skippedProducts) text += QStringLiteral("- %1\n").arg(product);
    if (!result.warnings.isEmpty()) {
        text += tr("\nWarnings:\n");
        for (const auto& warning : result.warnings) text += QStringLiteral("- %1\n").arg(warning);
    }
    text += tr("\nMemory validation: %1\n")
                .arg(result.generatedFiles.contains(QStringLiteral("ARAMF/memory/memory-consistency-validation.json"))
                         ? tr("PASS")
                         : tr("NOT RUN"));
    text += tr("Verification: Required\n");
    result_->setPlainText(text);
}
