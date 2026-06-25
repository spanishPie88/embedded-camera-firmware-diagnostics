# 嵌入式 Camera 与 Firmware Demo

公开作品集，展示 Embedded Linux、V4L2、Android Camera HAL/App、USB/UVC 和固件能力，不包含公司源码或商业 Sensor 寄存器表。

## Driver / HAL / App 三层 Demo

1. [`V4L2 I2C Sensor Driver`](demos/driver-v4l2-sensor)：I2C/regmap、runtime PM、V4L2 sub-device、RAW10、controls、mode 与 stream 生命周期。
2. [`Camera HAL3 Pipeline`](demos/hal3-camera-pipeline)：stream 校验、capture request、buffer ownership、partial/final result、error 与 flush。
3. [`Android Camera2 + JNI`](demos/android-camera2-jni-app)：Camera2 生命周期、preview、YUV ImageReader、背压、DirectByteBuffer、row/pixel stride 和 native C++。

```text
Android Camera2 App
        ↓
Camera HAL3 request / buffer pipeline
        ↓
V4L2 / media-controller / ISP
        ↓
I2C Sensor Driver / MIPI CSI-2
```

仓库还包含 UVC/V4L2 诊断、USB 带宽估算、media topology 审计和 A/B 固件回滚 Demo。Upwork 文案见 [`PORTFOLIO.md`](PORTFOLIO.md)。
