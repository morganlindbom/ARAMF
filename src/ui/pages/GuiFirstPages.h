#pragma once

#include <QWidget>
#include <QList>
class QCheckBox;
class ProjectModel;

class AiStrategyPage final : public QWidget {
public:
    explicit AiStrategyPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);
private:
    ProjectModel* model_ = nullptr;
};

class RulesRoutingPage final : public QWidget {
public:
    explicit RulesRoutingPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);
private:
    ProjectModel* model_ = nullptr;
};

class MemoryPage final : public QWidget {
public:
    explicit MemoryPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);
private:
    ProjectModel* model_ = nullptr;
};

class VerificationPage final : public QWidget {
public:
    explicit VerificationPage(QWidget* parent = nullptr);
    void setModel(ProjectModel* model);
private:
    ProjectModel* model_ = nullptr;
};
