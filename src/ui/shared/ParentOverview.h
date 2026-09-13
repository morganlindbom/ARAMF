#pragma once

#include <QFrame>
#include <QList>
#include <QString>

struct ParentOverviewCardData
{
    QString id;
    QString number;
    QString title;
    QString description;
};

class ParentOverviewCard final : public QFrame
{
    Q_OBJECT
public:
    ParentOverviewCard(const ParentOverviewCardData& data, QWidget* parent = nullptr);

signals:
    void activated(const QString& id);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QString id_;
};

class ParentOverviewPage : public QWidget
{
    Q_OBJECT
public:
    ParentOverviewPage(const QString& title, const QString& intro,
                       const QList<ParentOverviewCardData>& cards,
                       QWidget* parent = nullptr);

signals:
    void cardActivated(const QString& id);
};
