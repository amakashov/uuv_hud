#pragma once

#include <QWidget>

class HeadingWidget final : public QWidget
{
public:
    explicit HeadingWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    void setHeading(double degrees);
    void setDesiredHeading(double degrees);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double heading = 0.0;
    double desiredHeading = 0.0;
};
