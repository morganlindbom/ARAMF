#include "PageSupport.h"

#include "core/ProjectModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
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

}
