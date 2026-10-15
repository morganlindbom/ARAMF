// File: src/ui/workflows/project/frameworks/dataml/ProjectFrameworkDataMlPage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"

class ProjectFrameworkDataMlPage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkDataMlPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct this categorized frameworks workflow child.
    // Stable IDs are isolated to this group so edits preserve selections from every sibling group.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkDataMl(), {QStringLiteral("pytorch"), QStringLiteral("tensorflow"), QStringLiteral("scikit-learn"), QStringLiteral("keras"), QStringLiteral("xgboost"), QStringLiteral("lightgbm"), QStringLiteral("hugging-face-transformers"), QStringLiteral("numpy"), QStringLiteral("pandas"), QStringLiteral("scipy"), QStringLiteral("matplotlib"), QStringLiteral("opencv"), QStringLiteral("onnx"), QStringLiteral("onnx-runtime"), QStringLiteral("tensorflow-lite"), QStringLiteral("cuda"), QStringLiteral("cudnn")}) {}
};
