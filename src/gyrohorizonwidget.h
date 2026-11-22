#pragma once

#include <QWidget>

class GyroHorizonWidget final : public QWidget
{
public:
    explicit GyroHorizonWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    void setAttitude(double pitchDegrees, double rollDegrees);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double pitch = 0.0;
    double roll = 0.0;
};
