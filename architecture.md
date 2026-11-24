# Underwater ROV HUD Architecture

## Application structure

The prototype is a single C++ executable built with CMake and Qt 5.15. It links `Qt5::Widgets` for the interface. Webcam capture uses `Qt5::Multimedia` when available at configure time. `MainWindow` owns the active `VideoSource` and the `RovHudWidget`. The laptop webcam is the first live source. A generated test pattern makes the display usable without a camera.

## Video pipeline

1. **Capture.** `VideoSource` exposes start and stop operations and reports source status and errors. The webcam adapter uses `QCamera` with a custom `QAbstractVideoSurface` to receive `QVideoFrame` objects.
2. **Normalize.** The adapter handles the negotiated pixel format and row stride and publishes an owning `QImage` in `Format_RGBA8888`. Each frame carries a sequence number, a monotonic arrival timestamp, and a source timestamp when available. The adapter copies mapped pixels before unmapping if the image still references the capture buffer.
3. **Hand off.** Blocking capture and decode run off the GUI thread. A single latest-frame slot keeps the preview from building a backlog. The GUI samples it on a timer and schedules a repaint only for a new video frame.
4. **Display.** The private `QOpenGLWidget` uploads pixels on the GUI thread when a new frame arrives. It reuses the texture while the resolution is unchanged and exposes the aspect-ratio-preserving video rectangle for overlay positioning.

Track source status separately from frame freshness. If frames stop arriving after playback begins, retain the last image and show `VIDEO STALE`. Report startup and connection errors through source status.

The optional Aravis adapter uses GigE Vision to acquire frames from a compatible iRAYPLE camera. A worker thread opens the camera, receives images into a bounded buffer pool, and publishes the newest complete image as RGBA. Select this source with `--gige` and a camera IP address or device ID.

Future adapters publish the same decoded-frame format. GStreamer will handle RTSP reception and decoding. ONVIF will select a media profile and resolve its stream URI for the RTSP adapter. Add vendor SDKs for models or features that need them.

## Customizable overlays

`RovHudWidget` is a `QWidget` container with a private `QOpenGLWidget` for video and ordinary child widgets stacked above it for instruments and controls. It takes ownership of added overlay widgets.

Each overlay has a stable ID and a placement made up of a reference rectangle (`Video` or `Viewport`), an anchor, and an offset in logical pixels. For example, a reticle can stay centered on the camera image while a status indicator stays in a corner of the full viewport. The container updates overlay geometry when the viewport, video rectangle, or placement changes.

The initial C++ API supports adding, moving, hiding, and removing overlays. Interactive overlays receive input. Passive overlays opt out of pointer events with `Qt::WA_TransparentForMouseEvents`. Empty areas let pointer events reach the video view. Pilot drag-and-drop and layout persistence can build on the same placement API.

## Telemetry and first milestone

Simulated telemetry provides heading, pitch, and roll directly to the heading tape and gyro horizon. Video and telemetry update independently.

Verify the prototype with the laptop camera and test pattern. Resize the window and confirm that the gyro horizon stays centered on the video. Press **F2** to move the heading tape between video and viewport anchors through the C++ API. Press **Space** in test-pattern mode to stop frame delivery and verify that the last image remains visible with the stale indicator. Resume delivery to verify that the indicator clears.
