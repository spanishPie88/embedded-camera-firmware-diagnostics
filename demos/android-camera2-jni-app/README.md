# Demo 3 — Android Camera2 + JNI Frame Analyzer

An Android application-layer demo above Camera HAL3. It creates preview and YUV analysis streams, applies bounded-latency frame consumption and passes plane buffers to native C++.

## Capabilities shown

- Camera2 device discovery, open/configure/close lifecycle
- generation token protecting against stale asynchronous callbacks
- TextureView preview plus `YUV_420_888` ImageReader stream
- bounded `maxImages` and `acquireLatestImage()` back pressure
- guaranteed `Image.close()` on all paths
- direct ByteBuffer JNI boundary without copying complete frames
- row-stride and pixel-stride aware Y/U/V sampling
- pause/surface destruction cleanup and background HandlerThread ownership

Core sources:

- [`CameraController.kt`](CameraController.kt)
- [`frame_analyzer.cpp`](frame_analyzer.cpp)

A production app should additionally handle orientation, camera quirks, thermal policy, disconnect recovery and UI permission rationale.
