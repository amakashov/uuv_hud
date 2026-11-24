#include <arv.h>

#include "videosource.h"

#include <array>
#include <atomic>
#include <limits>
#include <thread>
#include <utility>

namespace
{
constexpr int bufferCount = 8;
using Clock = std::chrono::steady_clock;

struct GObjectDeleter
{
    void operator()(gpointer object) const { g_object_unref(object); }
};

using CameraPtr = std::unique_ptr<ArvCamera, GObjectDeleter>;
using StreamPtr = std::unique_ptr<ArvStream, GObjectDeleter>;

struct PixelLayout
{
    QImage::Format format = QImage::Format_Invalid;
    int bytesPerPixel = 1;
    // The four entries describe the color at each position in the Bayer tile.
    std::array<int, 4> bayer = {};
};

PixelLayout pixelLayout(ArvPixelFormat format)
{
    switch (format) {
    case ARV_PIXEL_FORMAT_MONO_8: return {QImage::Format_Grayscale8, 1};
    case ARV_PIXEL_FORMAT_RGB_8_PACKED: return {QImage::Format_RGB888, 3};
    case ARV_PIXEL_FORMAT_BGR_8_PACKED: return {QImage::Format_BGR888, 3};
    case ARV_PIXEL_FORMAT_BAYER_RG_8: return {QImage::Format_Invalid, 1, {0, 1, 1, 2}};
    case ARV_PIXEL_FORMAT_BAYER_GR_8: return {QImage::Format_Invalid, 1, {1, 0, 2, 1}};
    case ARV_PIXEL_FORMAT_BAYER_GB_8: return {QImage::Format_Invalid, 1, {1, 2, 0, 1}};
    case ARV_PIXEL_FORMAT_BAYER_BG_8: return {QImage::Format_Invalid, 1, {2, 1, 1, 0}};
    default: return {QImage::Format_Invalid, 0};
    }
}

QImage demosaic(const uchar *data, int width, int height, int stride,
                const std::array<int, 4> &tile)
{
    if (width < 2 || height < 2) {
        return {};
    }
    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return {};
    }
    for (int y = 0; y < height; ++y) {
        const auto *row = data + qsizetype(y) * stride;
        const auto *above = y == 0 ? row + stride : row - stride;
        const auto *below = y == height - 1 ? above : row + stride;
        auto *output = image.scanLine(y);
        const int tileRow = (y % 2) * 2;
        for (int x = 0; x < width; ++x, output += 4) {
            const int left = x == 0 ? 1 : x - 1;
            const int right = x == width - 1 ? left : x + 1;
            const int color = tile[tileRow + x % 2];
            output[color] = row[x];
            const int horizontal = row[left] + row[right];
            const int vertical = above[x] + below[x];
            if (color == 1) {
                const int horizontalColor = tile[tileRow + (1 - x % 2)];
                output[horizontalColor] = (horizontal + 1) / 2;
                output[2 - horizontalColor] = (vertical + 1) / 2;
            } else {
                output[1] = (horizontal + vertical + 2) / 4;
                output[2 - color] = (above[left] + above[right] + below[left] + below[right] + 2) / 4;
            }
            output[3] = 255;
        }
    }
    return image;
}

QImage convertImage(const uchar *data, size_t size, int width, int height, int xPadding,
                    ArvPixelFormat format)
{
    const auto layout = pixelLayout(format);
    const qint64 rowBytes = qint64(width) * layout.bytesPerPixel;
    const qint64 stride = rowBytes + xPadding;
    if (!data || width <= 0 || height <= 0 || xPadding < 0 || !layout.bytesPerPixel ||
        stride > std::numeric_limits<int>::max() ||
        quint64(stride * (height - 1) + rowBytes) > size) {
        return {};
    }

    if (layout.format == QImage::Format_Invalid) {
        return demosaic(data, width, height, int(stride), layout.bayer);
    }
    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return {};
    }
    const bool mono = layout.format == QImage::Format_Grayscale8;
    const int redIndex = layout.format == QImage::Format_BGR888 ? 2 : 0;
    for (int y = 0; y < height; ++y) {
        const auto *pixel = data + qsizetype(y) * stride;
        auto *output = image.scanLine(y);
        for (int x = 0; x < width; ++x, pixel += layout.bytesPerPixel, output += 4) {
            output[0] = pixel[redIndex];
            output[1] = pixel[mono ? 0 : 1];
            output[2] = pixel[mono ? 0 : 2 - redIndex];
            output[3] = 255;
        }
    }
    return image;
}

QImage decodeImage(ArvBuffer *buffer)
{
    size_t size = 0;
    const auto *data = static_cast<const uchar *>(arv_buffer_get_image_data(buffer, &size));
    int xPadding = 0;
    arv_buffer_get_image_padding(buffer, &xPadding, nullptr);
    return convertImage(data, size, arv_buffer_get_image_width(buffer),
                        arv_buffer_get_image_height(buffer), xPadding,
                        arv_buffer_get_image_pixel_format(buffer));
}

class GigECameraSource final : public VideoSource
{
public:
    explicit GigECameraSource(QString cameraId) : cameraId(std::move(cameraId)) {}
    ~GigECameraSource() override { stop(); }

    void start() override
    {
        if (!worker.joinable()) {
            stopRequested = false;
            worker = std::thread([this] { capture(); });
        }
    }

    void stop() override
    {
        stopRequested = true;
        if (worker.joinable()) {
            worker.join();
        }
    }

private:
    void reportError(const QString &step, GError *error = nullptr)
    {
        const QString message = error
            ? QStringLiteral("%1: %2").arg(step, QString::fromUtf8(error->message))
            : step;
        g_clear_error(&error);
        if (!stopRequested) {
            emit failed(message);
        }
    }

    void capture()
    {
        GError *error = nullptr;
        CameraPtr camera(arv_camera_new(cameraId.toUtf8().constData(), &error));
        if (!camera || error) {
            reportError(tr("Could not open GigE camera %1").arg(cameraId), error);
            return;
        }
        if (!arv_camera_is_gv_device(camera.get())) {
            reportError(tr("Selected camera is not a GigE Vision device"));
            return;
        }
        arv_camera_set_acquisition_mode(camera.get(), ARV_ACQUISITION_MODE_CONTINUOUS, &error);
        if (!error && arv_camera_is_feature_available(camera.get(), "TriggerMode", &error) && !error) {
            arv_camera_clear_triggers(camera.get(), &error);
        }
        if (error) {
            reportError(tr("Could not configure continuous acquisition"), error);
            return;
        }

        const auto format = arv_camera_get_pixel_format(camera.get(), &error);
        if (error) {
            reportError(tr("Could not read camera pixel format"), error);
            return;
        }
        if (!pixelLayout(format).bytesPerPixel) {
            reportError(tr("Unsupported pixel format (0x%1). Select Mono8, Bayer8, RGB8 or BGR8 on the camera.")
                            .arg(format, 0, 16));
            return;
        }

        const auto payload = arv_camera_get_payload(camera.get(), &error);
        if (!payload || error) {
            reportError(tr("Could not determine camera payload size"), error);
            return;
        }

        StreamPtr stream(arv_camera_create_stream(camera.get(), nullptr, nullptr, &error));
        if (!stream || error) {
            reportError(tr("Could not create GigE video stream"), error);
            return;
        }
        for (int i = 0; i < bufferCount; ++i) {
            arv_stream_push_buffer(stream.get(), arv_buffer_new_allocate(payload));
        }
        if (stopRequested) {
            return;
        }
        arv_camera_start_acquisition(camera.get(), &error);
        if (error) {
            reportError(tr("Could not start GigE acquisition"), error);
            return;
        }
        const QString failure = receiveFrames(stream.get());
        arv_camera_stop_acquisition(camera.get(), nullptr);
        if (!failure.isEmpty()) {
            reportError(failure);
        }
    }

    QString receiveFrames(ArvStream *stream)
    {
        const auto firstFrameDeadline = Clock::now() + std::chrono::seconds(5);
        bool receivedFrame = false;
        while (!stopRequested) {
            auto *buffer = arv_stream_timeout_pop_buffer(stream, 100000);
            ArvBuffer *newest = nullptr;
            // Keep the freshest complete image and recycle the rest of the batch.
            for (int i = 0; buffer && i < bufferCount; ++i) {
                const auto payloadType = arv_buffer_get_payload_type(buffer);
                if (arv_buffer_get_status(buffer) == ARV_BUFFER_STATUS_SUCCESS &&
                    (payloadType == ARV_BUFFER_PAYLOAD_TYPE_IMAGE ||
                     payloadType == ARV_BUFFER_PAYLOAD_TYPE_EXTENDED_CHUNK_DATA)) {
                    std::swap(buffer, newest);
                }
                if (buffer) {
                    arv_stream_push_buffer(stream, buffer);
                }
                buffer = i + 1 < bufferCount ? arv_stream_try_pop_buffer(stream) : nullptr;
            }
            if (newest) {
                const auto arrival = Clock::now();
                auto image = decodeImage(newest);
                const auto timestamp = arv_buffer_get_timestamp(newest);
                arv_stream_push_buffer(stream, newest);
                if (!image.isNull()) {
                    receivedFrame = true;
                    publish(std::move(image), timestamp ? qint64(timestamp / 1000) : -1, arrival);
                }
            }
            if (!receivedFrame && Clock::now() >= firstFrameDeadline) {
                return tr("No usable GigE frame received within five seconds");
            }
        }
        return {};
    }

    QString cameraId;
    std::atomic<bool> stopRequested{false};
    std::thread worker;
};
}

std::unique_ptr<VideoSource> createGigECameraSource(const QString &cameraId)
{
    return std::make_unique<GigECameraSource>(cameraId);
}
