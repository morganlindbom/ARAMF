// File: src/ui/workflows/project/academic/research/ProjectAcademicResearchPage.h
#pragma once

#include "../ProjectAcademicPage.h"

class ProjectAcademicResearchPage final : public ProjectAcademicPage
{
public:
    explicit ProjectAcademicResearchPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectAcademicResearch workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectAcademicPage(model, parent, ProjectAcademicPage::Section::Research) {}
};
