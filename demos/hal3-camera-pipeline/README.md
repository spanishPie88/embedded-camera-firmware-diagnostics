# Demo 2 — Camera HAL3 Request Pipeline

A host-buildable C++17 model of Android Camera HAL3 control flow. It is intentionally independent of AOSP/vendor source while preserving the important ownership and lifecycle concepts.

## Capabilities shown

- configureStreams-style stream-combination validation
- monotonic frame-number and repeating-settings handling
- request queue and worker-thread scheduling
- explicit framework/HAL buffer ownership
- partial metadata followed by final result delivery
- request and buffer error return paths
- flush cancellation that does not poison the next capture
- backend abstraction representing V4L2/media-controller/ISP integration
- deterministic close and thread shutdown

Production HAL code additionally needs native handles, acquire/release fences, gralloc usage, Android metadata tags, FMQ/AIDL, hotplug and vendor ISP controls.

Core source: [`hal3_pipeline_demo.cpp`](hal3_pipeline_demo.cpp)
