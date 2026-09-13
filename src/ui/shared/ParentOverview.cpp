#include "ParentOverview.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

ParentOverviewCard::ParentOverviewCard(const ParentOverviewCardData& data, QWidget* parent)
    : QFrame(parent), id_(data.id)
{
    QString objectId = data.id;
    objectId.replace(QLatin1Char('.'), QLatin1Char('_'));
    setObjectName(QStringLiteral("overviewCard_") + objectId);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setStyleSheet(QStringLiteral(
        "ParentOverviewCard { background: #f8fafc; border: 1px solid #cbd7e2; "
        "border-radius: 4px; }"
        "ParentOverviewCard:hover { background: #f1f6fb; border-color: #8eabc1; }"
        "ParentOverviewCard:focus { border: 2px solid #6b98b8; }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);
    auto* heading = new QLabel(
        QStringLiteral("<b>%1  %2</b>").arg(data.number.toHtmlEscaped(), data.title.toHtmlEscaped()), this);
    heading->setObjectName(QStringLiteral("overviewCardTitle"));
    heading->setWordWrap(true);
    auto* description = new QLabel(data.description, this);
    description->setObjectName(QStringLiteral("overviewCardDescription"));
    description->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(description);
}

void ParentOverviewCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) emit activated(id_);
    QFrame::mousePressEvent(event);
}

void ParentOverviewCard::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        emit activated(id_);
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

ParentOverviewPage::ParentOverviewPage(const QString& title, const QString& intro,
                                       const QList<ParentOverviewCardData>& cards,
                                       QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    auto* heading = new QLabel(QStringLiteral("<h1>%1</h1><p>%2</p>")
                                   .arg(title.toHtmlEscaped(), intro), this);
    heading->setWordWrap(true);
    layout->addWidget(heading);
    for (const auto& data : cards) {
        auto* card = new ParentOverviewCard(data, this);
        layout->addWidget(card);
        connect(card, &ParentOverviewCard::activated, this, &ParentOverviewPage::cardActivated);
    }
    layout->addStretch();
}
