#include "videosource.h"

#include <QPainter>
#include <QTimer>

VideoFrame VideoSource::latestFrame() const
{
    QMutexLocker lock(&mutex);
    return latest;
}

void VideoSource::publish(QImage image, qint64 sourceTimestampUs,
                          std::chrono::steady_clock::time_point arrival)
{
    QMutexLocker lock(&mutex);
    latest = {std::move(image), latest.sequence + 1, arrival, sourceTimestampUs};
}

namespace
{
class TestPatternSource final : public VideoSource
{
public:
    TestPatternSource()
    {
        timer.setInterval(33);
        connect(&timer, &QTimer::timeout, this, [this] { generateFrame(); });
    }

    void start() override
    {
        generateFrame();
        timer.start();
    }

    void stop() override
    {
        timer.stop();
    }

private:
    void generateFrame()
    {
        QImage image(640, 480, QImage::Format_RGBA8888);
        QPainter painter(&image);
        QLinearGradient water(0, 0, 0, image.height());
        water.setColorAt(0, QColor("#287789"));
        water.setColorAt(1, QColor("#0a2734"));
        painter.fillRect(image.rect(), water);
        painter.setPen(QColor(140, 207, 218, 65));
        for (int x = phase - 48; x < image.width(); x += 48) {
            painter.drawLine(x, 0, x, image.height());
        }
        for (int y = 0; y < image.height(); y += 48) {
            painter.drawLine(0, y, image.width(), y);
        }
        painter.fillRect(QRect(12 + markerPosition, image.height() - 38, 24, 8), QColor("#8bdce4"));
        painter.end();
        phase = (phase + 2) % 48;
        markerPosition = (markerPosition + 5) % (image.width() - 48);
        publish(std::move(image));
    }

    QTimer timer;
    int phase = 0;
    int markerPosition = 0;
};
}

std::unique_ptr<VideoSource> createTestPatternSource()
{
    return std::make_unique<TestPatternSource>();
}

#ifndef ROV_HAS_QT_MULTIMEDIA
std::unique_ptr<VideoSource> createWebcamSource()
{
    return {};
}
#endif
