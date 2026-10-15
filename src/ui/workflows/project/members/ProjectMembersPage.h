// File: src/ui/workflows/project/members/ProjectMembersPage.h
#pragma once

#include "core/ProjectModel.h"
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

class ProjectMembersPage final : public QWidget
{
public:
    explicit ProjectMembersPage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Edit project members independently from Academic settings; contacts are optional.
        : QWidget(parent), model_(model), table_(new QTableWidget(this))
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel(tr("<h2>Project members</h2>Add roles and responsibilities. Contact details are optional. Never enter credentials."), this));
        table_->setObjectName("projectMembersTable");
        table_->setColumnCount(6);
        table_->setHorizontalHeaderLabels({tr("Name"), tr("Project role"), tr("Responsibilities"), tr("Institution / course / organization"), tr("Email (optional)"), tr("GitHub username (optional)")});
        table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        layout->addWidget(table_);
        auto* buttons = new QHBoxLayout;
        auto* add = new QPushButton(tr("Add member"), this);
        auto* remove = new QPushButton(tr("Remove selected members"), this);
        add->setObjectName("addProjectMember"); remove->setObjectName("removeProjectMember");
        buttons->addWidget(add); buttons->addWidget(remove); buttons->addStretch(); layout->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, [this] {
            auto members = model_->projectMembers(); members.append(ProjectMember{}); model_->setProjectMembers(members);
            table_->setCurrentCell(table_->rowCount() - 1, 0); table_->editItem(table_->item(table_->rowCount() - 1, 0));
        });
        connect(remove, &QPushButton::clicked, this, [this] {
            auto members = model_->projectMembers();
            for (int row = table_->rowCount() - 1; row >= 0; --row)
                if (table_->selectionModel()->isRowSelected(row, QModelIndex())) members.removeAt(row);
            model_->setProjectMembers(members);
        });
        connect(table_, &QTableWidget::cellChanged, this, [this] {
            QList<ProjectMember> members;
            for (int row = 0; row < table_->rowCount(); ++row) {
                const auto text = [&](int column) { return table_->item(row, column) ? table_->item(row, column)->text() : QString(); };
                members.append({text(0), text(1), text(2), text(3), text(4), text(5)});
            }
            updating_ = true; model_->setProjectMembers(members); updating_ = false;
        });
        connect(model_, &ProjectModel::modelChanged, this, [this] { if (!updating_) refresh(); });
        refresh();
    }
private:
    void refresh()
    // Brief: Restore member rows without creating feedback edits or fabricating participants.
    {
        const QSignalBlocker blocker(table_);
        const auto members = model_->projectMembers();
        table_->setRowCount(members.size());
        for (int row = 0; row < members.size(); ++row) {
            const auto& member = members[row];
            const QStringList values{member.name, member.role, member.responsibilities, member.affiliation, member.email, member.githubUsername};
            for (int column = 0; column < values.size(); ++column) table_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
    }
    ProjectModel* model_;
    QTableWidget* table_;
    bool updating_ = false;
};
