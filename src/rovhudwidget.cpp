#include "rovhudwidget.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTextureBlitter>
#include <QOpenGLWidget>

class VideoView final : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    explicit VideoView(RovHudWidget *hud) : QOpenGLWidget(hud), hud(hud) {}

    ~VideoView() override
    {
        if (context()) {
            disconnect(context(), nullptr, this, nullptr);
            releaseResources();
        }
    }

    void setImage(const QImage &image)
    {
        pixels = image;
        uploadPending = true;
        update();
    }

protected:
    void initializeGL() override
    {
        initializeOpenGLFunctions();
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &VideoView::releaseResources,
                Qt::DirectConnection);
        blitter.create();
        uploadPending = true;
    }

    void paintGL() override
    {
        glClearColor(0.02f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (pixels.isNull() || !blitter.isCreated()) {
            return;
        }

        if (uploadPending) {
            if (!texture) {
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            } else {
                glBindTexture(GL_TEXTURE_2D, texture);
            }
            if (textureSize != pixels.size()) {
                textureSize = pixels.size();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixels.width(), pixels.height(),
                             0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.constBits());
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixels.width(), pixels.height(),
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels.constBits());
            }
            uploadPending = false;
        }

        const auto transform = QOpenGLTextureBlitter::targetTransform(hud->videoRect(), rect());
        blitter.bind();
        blitter.blit(texture, transform, QOpenGLTextureBlitter::OriginTopLeft);
        blitter.release();
    }

private:
    void releaseResources()
    {
        makeCurrent();
        if (texture) {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
        textureSize = {};
        blitter.destroy();
        doneCurrent();
    }

    RovHudWidget *hud;
    QOpenGLTextureBlitter blitter;
    GLuint texture = 0;
    QSize textureSize;
    QImage pixels;
    bool uploadPending = false;
};

RovHudWidget::RovHudWidget(QWidget *parent)
    : QWidget(parent), video(new VideoView(this))
{
}

void RovHudWidget::setFrame(const VideoFrame &frame)
{
    if (frame.image.isNull()) {
        return;
    }
    if (frameSize != frame.image.size()) {
        frameSize = frame.image.size();
        positionOverlays();
    }
    video->setImage(frame.image);
}

QRect RovHudWidget::videoRect() const
{
    if (frameSize.isEmpty()) {
        return rect();
    }
    const QSize fitted = frameSize.scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted);
}

bool RovHudWidget::addOverlay(const QString &id, QWidget *widget, OverlayPlacement placement)
{
    if (id.isEmpty() || !widget || widget == this || widget == video ||
        widget->isAncestorOf(this) || overlays.value(id).widget) {
        return false;
    }
    for (const auto &overlay : overlays) {
        if (overlay.widget == widget) {
            return false;
        }
    }
    widget->setParent(this);
    overlays.insert(id, {widget, placement});
    positionOverlay(overlays.value(id));
    widget->show();
    widget->raise();
    return true;
}

bool RovHudWidget::setOverlayPlacement(const QString &id, OverlayPlacement placement)
{
    auto entry = overlays.find(id);
    if (entry == overlays.end() || !entry->widget) {
        return false;
    }
    entry->placement = placement;
    positionOverlay(*entry);
    return true;
}

bool RovHudWidget::setOverlayVisible(const QString &id, bool visible)
{
    const auto overlay = overlays.value(id);
    if (!overlay.widget) {
        return false;
    }
    overlay.widget->setVisible(visible);
    return true;
}

bool RovHudWidget::removeOverlay(const QString &id)
{
    const auto overlay = overlays.take(id);
    if (!overlay.widget) {
        return false;
    }
    overlay.widget->hide();
    overlay.widget->deleteLater();
    return true;
}

void RovHudWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    video->setGeometry(rect());
    positionOverlays();
}

void RovHudWidget::positionOverlays()
{
    for (const auto &overlay : overlays) {
        positionOverlay(overlay);
    }
}

void RovHudWidget::positionOverlay(const Overlay &overlay)
{
    auto *widget = overlay.widget.data();
    if (!widget) {
        return;
    }
    const auto [area, anchor, offset] = overlay.placement;
    const QRect bounds = area == OverlayArea::Video ? videoRect() : rect();
    // Keep corner insets available even when an instrument is wider than the video.
    const QSize available = (bounds.size() - QSize(2 * qAbs(offset.x()), 2 * qAbs(offset.y())))
                                .expandedTo(QSize(0, 0));
    const QSize hint = widget->sizeHint();
    const QSize preferred = hint.isValid() ? hint : widget->size();
    const QSize extent = preferred.expandedTo(widget->minimumSizeHint())
                             .boundedTo(widget->maximumSize()).boundedTo(available)
                             .expandedTo(widget->minimumSize());
    int x = bounds.x();
    int y = bounds.y();
    if (anchor.testFlag(Qt::AlignRight)) {
        x += bounds.width() - extent.width();
    } else if (anchor.testFlag(Qt::AlignHCenter)) {
        x += (bounds.width() - extent.width()) / 2;
    }
    if (anchor.testFlag(Qt::AlignBottom)) {
        y += bounds.height() - extent.height();
    } else if (anchor.testFlag(Qt::AlignVCenter)) {
        y += (bounds.height() - extent.height()) / 2;
    }
    widget->setGeometry(QRect(QPoint(x, y) + offset, extent));
}
