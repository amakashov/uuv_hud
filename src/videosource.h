#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>

#include <chrono>
#include <memory>

struct VideoFrame
{
    QImage image;
    quint64 sequence = 0;
    std::chrono::steady_clock::time_point arrival;
    qint64 sourceTimestampUs = -1;
};

class VideoSource : public QObject
{
    Q_OBJECT

public:
    virtual void start() = 0;
    virtual void stop() = 0;
    VideoFrame latestFrame() const;

signals:
    void failed(const QString &message);

protected:
    void publish(QImage image, qint64 sourceTimestampUs = -1,
                 std::chrono::steady_clock::time_point arrival = std::chrono::steady_clock::now());

private:
    mutable QMutex mutex;
    VideoFrame latest;
};

std::unique_ptr<VideoSource> createTestPatternSource();
std::unique_ptr<VideoSource> createWebcamSource();
