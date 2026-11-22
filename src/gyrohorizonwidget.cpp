#include "gyrohorizonwidget.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

GyroHorizonWidget::GyroHorizonWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAccessibleName(tr("Artificial horizon"));
}

QSize GyroHorizonWidget::sizeHint() const
{
    return {240, 240};
}

void GyroHorizonWidget::setAttitude(double pitchDegrees, double rollDegrees)
{
    if (!std::isfinite(pitchDegrees) || !std::isfinite(rollDegrees)) {
        return;
    }
    pitch = std::clamp(pitchDegrees, -90.0, 90.0);
    roll = std::remainder(rollDegrees, 360.0);
    update();
}

void GyroHorizonWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(width() / 2.0, height() / 2.0);
    const double scale = std::min(width(), height()) / 240.0;
    painter.scale(scale, scale);

    QPainterPath aperture;
    aperture.addEllipse(QPointF(), 96, 96);
    painter.save();
    painter.setClipPath(aperture);
    painter.rotate(-roll);
    constexpr double pixelsPerDegree = 3.0;
    const double horizonY = pitch * pixelsPerDegree;
    QPainterPath pitchLines;
    pitchLines.moveTo(-100, horizonY);
    pitchLines.lineTo(100, horizonY);

    QFont font = painter.font();
    font.setPixelSize(11);
    const QFontMetricsF metrics(font);
    QPainterPath pitchLabels;
    for (int degree = -90; degree <= 90; degree += 5) {
        if (degree == 0) {
            continue;
        }
        const bool labeled = degree % 10 == 0;
        const int halfWidth = labeled ? 23 : 12;
        const double y = horizonY - degree * pixelsPerDegree;
        pitchLines.moveTo(-halfWidth, y);
        pitchLines.lineTo(halfWidth, y);
        if (labeled) {
            const QString label = QString::number(std::abs(degree));
            const double baseline = y + (metrics.ascent() - metrics.descent()) / 2.0;
            pitchLabels.addText(-28 - metrics.horizontalAdvance(label), baseline, font, label);
            pitchLabels.addText(28, baseline, font, label);
        }
    }
    const QColor outline("#10202a");
    painter.strokePath(pitchLines, QPen(outline, 4));
    painter.strokePath(pitchLines, QPen(Qt::white, 1.5));
    painter.strokePath(pitchLabels, QPen(outline, 2));
    painter.fillPath(pitchLabels, Qt::white);
    painter.restore();

    QPainterPath rollTicks;
    for (int angle : {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60}) {
        QTransform rotation;
        rotation.rotate(angle);
        const int innerRadius = angle % 30 == 0 ? 99 : 103;
        rollTicks.moveTo(rotation.map(QPointF(0, -111)));
        rollTicks.lineTo(rotation.map(QPointF(0, -innerRadius)));
    }
    painter.strokePath(rollTicks, QPen(outline, 4));
    painter.strokePath(rollTicks, QPen(Qt::white, 1.5));

    const QColor referenceColor("#ffd36a");
    painter.save();
    painter.rotate(-roll);
    painter.setPen(QPen(outline, 2));
    painter.setBrush(referenceColor);
    painter.drawPolygon(QPolygonF({QPointF(0, -96), QPointF(-6, -84), QPointF(6, -84)}));
    painter.restore();

    const QList<QPolygonF> wings = {
        QPolygonF({QPointF(-46, 0), QPointF(-22, 0), QPointF(-22, 8)}),
        QPolygonF({QPointF(46, 0), QPointF(22, 0), QPointF(22, 8)})
    };
    for (const auto &wing : wings) {
        painter.setPen(QPen(outline, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(wing);
        painter.setPen(QPen(referenceColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(wing);
    }
    painter.setPen(QPen(outline, 2));
    painter.setBrush(referenceColor);
    painter.drawEllipse(QPointF(), 4, 4);
}
