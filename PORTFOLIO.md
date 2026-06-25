# Portfolio Presentation Notes

## Suggested Upwork portfolio title

**Embedded Camera Stack: Linux Driver, Android HAL3, Camera2/JNI App**

## Short description

I created a vendor-neutral camera portfolio covering three engineering layers:
a Linux V4L2 I2C sensor driver, a Camera HAL3-style request and buffer
pipeline, and an Android Camera2 application with JNI/native YUV analysis.

The Driver demo covers runtime power management, active-low reset handling,
V4L2 controls, media-bus formats, mode selection and streaming. The HAL demo
covers stream configuration, capture requests, repeating metadata, buffer
ownership, partial/final results, cancellation, errors and flush. The App demo
covers Camera2 lifecycle, preview plus YUV analysis, ImageReader back pressure,
direct buffers, row/pixel strides and native C++.

All examples are synthetic and vendor-neutral. They contain no customer source,
commercial sensor register table or proprietary device log.

## Skills demonstrated

- Linux I2C image-sensor driver and runtime PM
- V4L2 sub-device, media pads, controls and MIPI CSI-2 formats
- Android Camera HAL3 request/result/buffer lifecycle
- Camera2, ImageReader, JNI/NDK and native C++
- UVC bandwidth and media-topology analysis
- Firmware integrity, power-loss handling and rollback

## Screenshots to publish

1. Driver source showing power, controls, format and stream operations.
2. HAL architecture and capture-request result output.
3. Android Camera2 preview with native Y/U/V statistics.
4. Good/broken media-topology comparison.
5. Firmware simulator rollback event log.

Do not publish screenshots containing real customer identifiers, USB serial
numbers, internal paths, register tables or confidential device names.
