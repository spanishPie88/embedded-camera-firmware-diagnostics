# Driver / HAL / App Camera Portfolio

The three directories in this folder demonstrate an end-to-end embedded camera
software stack.

## 1. Linux driver

[`driver-v4l2-sensor`](driver-v4l2-sensor)

A fictional RAW10 I2C sensor driver showing Linux kernel and V4L2 sub-device
engineering: power sequencing, runtime PM, controls, format negotiation,
streaming, media pads, CSI-2 endpoints, and device tree.

## 2. Android camera HAL

[`hal3-camera-pipeline`](hal3-camera-pipeline)

A host-buildable C++17 model of Camera HAL3 request processing: stream
configuration, request validation, worker scheduling, repeating metadata,
buffer ownership, partial/final results, error notification, flush, and a
V4L2-style backend boundary.

## 3. Android application

[`android-camera2-jni-app`](android-camera2-jni-app)

An Android Camera2 application using a preview stream and a YUV analysis
stream. Frames are acquired with bounded latency and passed as direct buffers
to JNI/native C++, which reads the planes using their actual row and pixel
strides.

## Layer boundaries

```text
App responsibilities
  Camera permission, UI/surface lifecycle, capture intent, frame consumption

HAL responsibilities
  Capabilities, stream combinations, requests/results, buffers/fences, errors

Driver responsibilities
  Hardware power/control, bus transactions, formats, timing, stream state
```

Keeping these responsibilities separate makes failures easier to localize:

- no sensor frames or I2C errors -> driver/hardware boundary;
- unsupported combination or stuck buffer -> HAL/pipeline boundary;
- lifecycle leak or analysis backlog -> application boundary.
