// GeneratePage.cpp

#include "GeneratePage.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

GeneratePage::GeneratePage(ProjectModel* model, GenerationServices* services, QWidget* parent)
    : QWidget(parent),
      model_(model),
      services_(services),
      result_(new QPlainTextEdit(this))
{
    /**Construct the generation page.

    The page writes the managed project control plane into the canonical uppercase ARAMF directory using the native C++ generation service.
    */
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Generate</h2>Select the ARAMF output products, then generate them into the target project."), this));

    auto* outputs = new QGroupBox(tr("Output products"), this);
    auto* outputLayout = new QVBoxLayout(outputs);
    for (const auto& text : {
             tr("AGENTS.md bootstrap and ARAMF agent rules"),
             tr("Routing manifests"),
             tr("Platform adapters"),
             tr("Resource manifest"),
             tr("Project Memory artifacts"),
             tr("Provenance and selection effects")}) {
        auto* box = new QCheckBox(text, outputs);
        box->setChecked(true);
        outputLayout->addWidget(box);
    }
    layout->addWidget(outputs);

    auto* generateButton = new QPushButton(tr("Generate ARAMF control plane"), this);
    layout->addWidget(generateButton);
    result_->setReadOnly(true);
    layout->addWidget(result_);

    connect(generateButton, &QPushButton::clicked, this, [this] {
        result_->setPlainText(services_->generate(*model_));
    });
}
