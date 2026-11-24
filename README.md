# ROV HUD

A Qt Widgets prototype with a live video view, a transparent gyro horizon, and a heading tape showing current and desired headings. Attitude and heading are simulated.

## Build and run

Requires CMake 3.16 or later, a C++17 compiler, Qt 5.15 Widgets development files, and a desktop platform with OpenGL support. Webcam capture also requires Qt 5 Multimedia development files and the platform's camera backend. On Ubuntu, these are provided by `qtmultimedia5-dev` and `libqt5multimedia5-plugins`.

```sh
cmake -S . -B build
cmake --build build
./build/rov_hud
```

The app tries the default webcam and falls back to a moving test pattern if the camera is unavailable or fails to deliver its first frame. The status bar identifies the active source. To start with the pattern directly, run `./build/rov_hud --test-pattern`.

## iRAYPLE GigE Vision

Install the Aravis 0.8 development package (version 0.8.25 or newer) and reconfigure CMake. On Ubuntu, the package is `libaravis-dev`. Connect and power the camera, put its IP address and the host Ethernet interface on the same subnet, and close MV Viewer if it has the camera open. If you need the address or device ID, run `arv-tool-0.8` from `aravis-tools-cli`.

```sh
cmake -S . -B build
cmake --build build
./build/rov_hud --gige 192.168.1.20
```

Replace the example address with your camera's IP address or Aravis device ID. The adapter starts continuous acquisition with triggers disabled. It uses the camera's configured resolution, exposure, and frame rate. Select `Mono8`, one of the four 8-bit Bayer formats, `RGB8`, or `BGR8` in the camera settings.

Check that the status bar reads **GigE camera** and that the preview updates. If acquisition fails before the first frame, the status bar reports the reason and the test pattern takes over. To check stall behavior, disconnect the camera after it starts streaming and confirm that the last frame stays visible under `VIDEO STALE`.

## Verify the prototype

- Resize the window until the video has letterbox or pillarbox bars. The gyro horizon should stay centered on the image.
- Press **F2** to move the heading tape between the top of the window and the top of the video image. Use a tall window to see the difference.
- Press **Left** or **Right** to change the desired heading by five degrees. The cyan marker moves along the tape or points to the target from the nearest edge when the target is outside the visible range.
- With the test pattern running, press **Space** to pause frame delivery. After one second, `VIDEO STALE` should appear over the retained frame while the instruments continue updating. Press **Space** again to resume and clear the warning.

See [architecture.md](architecture.md) for the video source and overlay design.
