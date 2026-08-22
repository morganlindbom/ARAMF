#pragma once

#include <QString>

class QCheckBox;
class QGroupBox;
class QLayout;
class QWidget;
class ProjectModel;

namespace AramfUi {

void bindCheckboxes(QWidget* page, ProjectModel* model, const QString& key);
QWidget* pageShell(const QString& title, const QString& intro, QWidget* content);
QGroupBox* group(const QString& title, QLayout* layout, QWidget* parent);
QCheckBox* check(const QString& text, const QString& hint, QWidget* parent);

}
