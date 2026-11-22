#include "headingwidget.h"

#include <QPainter>

#include <cmath>

namespace
{
double wrapHeading(double degrees)
{
    const double wrapped = std::fmod(degrees, 360.0);
    return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
}

QString headingText(double degrees)
{
    return QStringLiteral("%1°").arg(qRound(degrees) % 360, 3, 10, QLatin1Char('0'));
}

QString tickText(int degrees)
{
    switch (static_cast<int>(wrapHeading(degrees))) {
    case 0: return QStringLiteral("N");
    case 90: return QStringLiteral("E");
    case 180: return QStringLiteral("S");
    case 270: return QStringLiteral("W");
    default: return headingText(wrapHeading(degrees));
    }
}
}

HeadingWidget::HeadingWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAccessibleName(tr("Heading tape"));
}

QSize HeadingWidget::sizeHint() const
{
    return {520, 102};
}

void HeadingWidget::setHeading(double degrees)
{
    if (std::isfinite(degrees)) {
        heading = wrapHeading(degrees);
        update();
    }
}

void HeadingWidget::setDesiredHeading(double degrees)
{
    if (std::isfinite(degrees)) {
        desiredHeading = wrapHeading(degrees);
        update();
    }
}

void HeadingWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal scale = qMin(width() / 520.0, height() / 102.0);
    painter.translate(width() / 2.0, height() / 2.0);
    painter.scale(scale, scale);

    const QColor background(9, 24, 33, 225);
    const QColor text("#eef8fb");
    const QColor target("#67e5ff");
    const QRectF tape(-250, -24, 500, 51);
    painter.setPen(QPen(QColor("#526e7d"), 1));
    painter.setBrush(background);
    painter.drawRoundedRect(tape, 7, 7);

    QFont font = painter.font();
    font.setPixelSize(13);
    painter.setFont(font);
    constexpr double halfRange = 60.0;
    const double pixelsPerDegree = tape.width() / (2 * halfRange);
    painter.save();
    painter.setClipRect(tape.adjusted(5, 1, -5, -1));
    painter.setPen(text);
    const int firstTick = static_cast<int>(std::ceil((heading - halfRange) / 5.0)) * 5;
    for (int tick = firstTick; tick <= heading + halfRange; tick += 5) {
        const double x = (tick - heading) * pixelsPerDegree;
        const bool major = tick % 15 == 0;
        painter.drawLine(QPointF(x, 15), QPointF(x, major ? 3 : 9));
        if (major) {
            painter.drawText(QRectF(x - 25, -20, 50, 20), Qt::AlignCenter, tickText(tick));
        }
    }
    painter.restore();

    const double error = std::remainder(desiredHeading - heading, 360.0);
    const double targetX = error * pixelsPerDegree;
    painter.setPen(Qt::NoPen);
    painter.setBrush(target);
    if (std::abs(error) <= halfRange) {
        painter.drawPolygon(QPolygonF({
            QPointF(targetX - 8, -31), QPointF(targetX + 8, -31),
            QPointF(targetX, -21)
        }));
    } else {
        const double edge = error < 0 ? -250 : 250;
        const double inward = error < 0 ? 12 : -12;
        painter.drawPolygon(QPolygonF({
            QPointF(edge, -29), QPointF(edge + inward, -36), QPointF(edge + inward, -22)
        }));
    }

    painter.setBrush(text);
    painter.drawPolygon(QPolygonF({QPointF(0, 17), QPointF(-6, 26), QPointF(6, 26)}));
    font.setPixelSize(17);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(text);
    painter.setBrush(background);
    const QRectF readout(-41, 26, 82, 24);
    painter.drawRoundedRect(readout, 4, 4);
    painter.drawText(readout, Qt::AlignCenter, headingText(heading));

    font.setPixelSize(14);
    painter.setFont(font);
    painter.setPen(target);
    painter.drawText(QRectF(80, 28, 170, 21), Qt::AlignRight | Qt::AlignVCenter,
                     tr("DES %1").arg(headingText(desiredHeading)));
}
