#include "PageSupport.h"

#include "core/ProjectModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QVBoxLayout>

namespace AramfUi {

void bindCheckboxes(QWidget* page, ProjectModel* model, const QString& key)
{
    if (!model) return;

    const auto checks = page->findChildren<QCheckBox*>();
    const auto refresh = [page, model, key] {
        const auto values = model->optionValues(key);
        for (auto* box : page->findChildren<QCheckBox*>()) {
            const QSignalBlocker blocker(box);
            box->setChecked(values.contains(box->text()));
        }
    };

    for (auto* box : checks) {
        QObject::connect(box, &QCheckBox::toggled, page,
                         [page, model, key] {
                             QStringList values;
                             for (auto* item : page->findChildren<QCheckBox*>()) {
                                 if (item->isChecked()) values << item->text();
                             }
                             model->setOptionValues(key, values);
                         });
    }

    QObject::connect(model, &ProjectModel::optionChanged, page,
                     [key, refresh](const QString& changed) {
                         if (changed == key) refresh();
                     });
    refresh();
}

QWidget* pageShell(const QString& title, const QString& intro, QWidget* content)
{
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 20, 24, 20);
    outer->setSpacing(14);
    outer->addWidget(new QLabel(
        QStringLiteral("<h1>%1</h1><p>%2</p>").arg(title, intro), page));

    outer->addWidget(content, 1);
    return page;
}

QGroupBox* group(const QString& title, QLayout* layout, QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    box->setLayout(layout);
    return box;
}

QCheckBox* check(const QString& text, const QString& hint, QWidget* parent)
{
    auto* result = new QCheckBox(text, parent);
    result->setToolTip(hint);
    return result;
}

void normalizeWorkflowPage(QWidget* page)
{
    if (!page) return;

    // The page host owns the available width.  Clear inherited minimum-width
    // pressure and let controls consume only the width the host supplies.
    page->setMinimumWidth(0);
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    for (auto* form : page->findChildren<QFormLayout*>()) {
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    }
    for (auto* widget : page->findChildren<QWidget*>()) {
        widget->setMinimumWidth(0);
        if (auto* label = qobject_cast<QLabel*>(widget)) {
            label->setWordWrap(true);
            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        } else if (qobject_cast<QLineEdit*>(widget)
                   || qobject_cast<QComboBox*>(widget)
                   || qobject_cast<QTextEdit*>(widget)
                   || qobject_cast<QPlainTextEdit*>(widget)
                   || qobject_cast<QListWidget*>(widget)) {
            widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        } else if (qobject_cast<QGroupBox*>(widget)) {
            widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        } else if (qobject_cast<QCheckBox*>(widget)
                   || qobject_cast<QRadioButton*>(widget)) {
            widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        } else if (qobject_cast<QPushButton*>(widget)) {
            widget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        }
    }
    page->updateGeometry();
}

}
