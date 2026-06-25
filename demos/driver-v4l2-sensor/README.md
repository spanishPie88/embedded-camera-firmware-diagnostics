# Demo 1 — Linux V4L2 I2C Sensor Driver

A fictional MIPI CSI-2 RAW10 sensor driver demonstrating Linux kernel camera-driver architecture without exposing vendor code.

## Capabilities shown

- I2C/regmap register access and chip identification
- regulator, MCLK, reset GPIO and runtime-PM sequencing
- active-low GPIO descriptor semantics and error rollback
- V4L2 sub-device, media pad and RAW10 media-bus format
- 1080p/720p mode selection and control-range synchronization
- exposure, analogue gain and vertical blanking controls
- cached controls applied during stream start
- ordered stream-on/stream-off and asynchronous sensor registration

The register addresses and values are intentionally fictional. A real integration must replace the mode tables, verify electrical timing and add the target device-tree endpoint.

Core source: [`demo123.c`](demo123.c)
