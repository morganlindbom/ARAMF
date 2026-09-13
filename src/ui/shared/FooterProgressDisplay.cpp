#include "FooterProgressDisplay.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QTextOption>
#include <QtMath>

namespace
{

QString progressText(const QString& format, int value, int minimum, int maximum)
{
    const int percentage = maximum == minimum
        ? 0
        : qRound(100.0 * (value - minimum) / (maximum - minimum));
    QString text = format;
    text.replace(QStringLiteral("%p"), QString::number(percentage));
    return text;
}

}

FooterProgressDisplay::FooterProgressDisplay(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFocusPolicy(Qt::NoFocus);
}

void FooterProgressDisplay::setRange(int minimum, int maximum)
{
    if (minimum > maximum) qSwap(minimum, maximum);
    minimum_ = minimum;
    maximum_ = maximum;
    value_ = qBound(minimum_, value_, maximum_);
    update();
}

void FooterProgressDisplay::setValue(int value)
{
    const int boundedValue = qBound(minimum_, value, maximum_);
    if (value_ == boundedValue) return;
    value_ = boundedValue;
    update();
}

void FooterProgressDisplay::setFormat(const QString& format)
{
    if (format_ == format) return;
    format_ = format;
    update();
}

void FooterProgressDisplay::setAlignment(Qt::Alignment alignment)
{
    if (alignment_ == alignment) return;
    alignment_ = alignment;
    update();
}

QSize FooterProgressDisplay::sizeHint() const
{
    return QSize(220, 40);
}

QSize FooterProgressDisplay::minimumSizeHint() const
{
    return QSize(120, 40);
}

void FooterProgressDisplay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = qMin<qreal>(6.0, bounds.height() / 2.0);
    const QPalette currentPalette = palette();
    const QColor background = currentPalette.color(QPalette::Button);
    const QColor fill = currentPalette.color(QPalette::Highlight);
    const QColor border = currentPalette.color(QPalette::Mid);
    const QColor backgroundText = currentPalette.color(QPalette::ButtonText);
    const QColor fillText = currentPalette.color(QPalette::HighlightedText);

    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(bounds, radius, radius);

    const qreal fraction = maximum_ == minimum_
        ? 0.0
        : qBound(0.0, (value_ - minimum_) / static_cast<qreal>(maximum_ - minimum_), 1.0);
    const qreal fillWidth = bounds.width() * fraction;
    if (fillWidth > 0.0) {
        painter.save();
        painter.setClipRect(QRectF(bounds.left(), bounds.top(), fillWidth, bounds.height()));
        painter.setBrush(fill);
        painter.drawRoundedRect(bounds, radius, radius);
        painter.restore();
    }

    painter.setPen(border);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bounds, radius, radius);

    const QString text = progressText(format_, value_, minimum_, maximum_);
    QTextOption textOption;
    textOption.setAlignment(alignment_);
    painter.setPen(backgroundText);
    painter.drawText(bounds, text, textOption);
    if (fillWidth > 0.0) {
        painter.save();
        painter.setClipRect(QRectF(bounds.left(), bounds.top(), fillWidth, bounds.height()));
        painter.setPen(fillText);
        painter.drawText(bounds, text, textOption);
        painter.restore();
    }
}
