// File: src/ui/workflows/project/academic/information/ProjectAcademicInformationPage.h
#pragma once

#include "../ProjectAcademicPage.h"

class ProjectAcademicInformationPage final : public ProjectAcademicPage
{
public:
    explicit ProjectAcademicInformationPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Construct the dedicated ProjectAcademicInformation workflow page.
    // This route reuses the established controls while owning a stable page class in its navigation-matched directory.
        : ProjectAcademicPage(model, parent, ProjectAcademicPage::Section::Information) {}
};
