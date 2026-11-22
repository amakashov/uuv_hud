#pragma once

#include "videosource.h"

#include <QHash>
#include <QPointer>
#include <QWidget>

class VideoView;

class RovHudWidget final : public QWidget
{
public:
    enum class OverlayArea { Video, Viewport };

    struct OverlayPlacement
    {
        OverlayArea area;
        Qt::Alignment anchor;
        QPoint offset;
    };

    explicit RovHudWidget(QWidget *parent = nullptr);
    void setFrame(const VideoFrame &frame);

    bool addOverlay(const QString &id, QWidget *widget, OverlayPlacement placement);
    bool setOverlayPlacement(const QString &id, OverlayPlacement placement);
    bool setOverlayVisible(const QString &id, bool visible);
    bool removeOverlay(const QString &id);
    QRect videoRect() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Overlay
    {
        QPointer<QWidget> widget;
        OverlayPlacement placement;
    };

    void positionOverlay(const Overlay &overlay);
    void positionOverlays();

    VideoView *video;
    QHash<QString, Overlay> overlays;
    QSize frameSize;
};
