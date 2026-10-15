// File: src/ui/workflows/project/academic/documentation/ProjectAcademicDocumentationPage.h
#pragma once

#include "../ProjectAcademicPage.h"

class ProjectAcademicDocumentationPage final : public ProjectAcademicPage
{
public:
    explicit ProjectAcademicDocumentationPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectAcademicDocumentation workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectAcademicPage(model, parent, ProjectAcademicPage::Section::Documentation) {}
};
