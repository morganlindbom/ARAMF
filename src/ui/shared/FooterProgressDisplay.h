#pragma once

#include <QString>
#include <QWidget>

class QPaintEvent;

class FooterProgressDisplay final : public QWidget
{
    Q_OBJECT

public:
    explicit FooterProgressDisplay(QWidget* parent = nullptr);

    int minimum() const { return minimum_; }
    int maximum() const { return maximum_; }
    int value() const { return value_; }
    void setRange(int minimum, int maximum);
    void setValue(int value);

    QString format() const { return format_; }
    void setFormat(const QString& format);

    Qt::Alignment alignment() const { return alignment_; }
    void setAlignment(Qt::Alignment alignment);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int minimum_ = 0;
    int maximum_ = 100;
    int value_ = 0;
    QString format_ = QStringLiteral("%p% of your setup is done");
    Qt::Alignment alignment_ = Qt::AlignCenter;
};
