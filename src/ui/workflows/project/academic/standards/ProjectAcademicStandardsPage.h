// File: src/ui/workflows/project/academic/standards/ProjectAcademicStandardsPage.h
#pragma once

#include "../ProjectAcademicPage.h"

class ProjectAcademicStandardsPage final : public ProjectAcademicPage
{
public:
    explicit ProjectAcademicStandardsPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectAcademicStandards workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectAcademicPage(model, parent, ProjectAcademicPage::Section::Standards) {}
};
