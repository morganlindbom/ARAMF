// File: src/ui/workflows/project/academic/deliverables/ProjectAcademicDeliverablesPage.h
#pragma once

#include "../ProjectAcademicPage.h"

class ProjectAcademicDeliverablesPage final : public ProjectAcademicPage
{
public:
    explicit ProjectAcademicDeliverablesPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectAcademicDeliverables workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectAcademicPage(model, parent, ProjectAcademicPage::Section::Deliverables) {}
};
