#include "videosource.h"

#include <QAbstractVideoSurface>
#include <QCamera>
#include <QCameraInfo>
#include <QVideoSurfaceFormat>

#include <functional>

namespace
{
class CameraSurface final : public QAbstractVideoSurface
{
public:
    using FrameHandler = std::function<void(QImage, qint64, std::chrono::steady_clock::time_point)>;

    explicit CameraSurface(FrameHandler handler) : handler(std::move(handler)) {}

    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType handleType) const override
    {
        if (handleType != QAbstractVideoBuffer::NoHandle) {
            return {};
        }
        return {QVideoFrame::Format_NV12, QVideoFrame::Format_NV21,
                QVideoFrame::Format_YUYV, QVideoFrame::Format_UYVY,
                QVideoFrame::Format_YUV420P, QVideoFrame::Format_YV12,
                QVideoFrame::Format_RGB32, QVideoFrame::Format_ARGB32,
                QVideoFrame::Format_RGB24, QVideoFrame::Format_BGR24,
                QVideoFrame::Format_Jpeg};
    }

    bool present(const QVideoFrame &frame) override
    {
        const auto arrival = std::chrono::steady_clock::now();
        QImage image = frame.image();
        if (image.isNull()) {
            // A subsequent frame may still be usable. The HUD monitors freshness.
            return true;
        }
        if (surfaceFormat().scanLineDirection() == QVideoSurfaceFormat::BottomToTop) {
            image = image.mirrored(false, true);
        }
        handler(image.convertToFormat(QImage::Format_RGBA8888), frame.startTime(), arrival);
        return true;
    }

private:
    FrameHandler handler;
};

class WebcamSource final : public VideoSource
{
public:
    explicit WebcamSource(const QCameraInfo &device)
        : surface([this](QImage image, qint64 timestamp, auto arrival) {
              publish(std::move(image), timestamp, arrival);
          }),
          camera(device)
    {
        camera.setViewfinder(&surface);
        connect(&camera, QOverload<QCamera::Error>::of(&QCamera::error), this,
                [this](QCamera::Error error) {
                    if (error != QCamera::NoError) {
                        emit failed(camera.errorString());
                    }
                });
    }

    void start() override { camera.start(); }
    void stop() override { camera.stop(); }

private:
    CameraSurface surface;
    QCamera camera;
};
}

std::unique_ptr<VideoSource> createWebcamSource()
{
    const auto device = QCameraInfo::defaultCamera();
    if (device.isNull()) {
        return {};
    }
    return std::make_unique<WebcamSource>(device);
}
