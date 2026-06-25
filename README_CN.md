# 嵌入式 Camera 与 Firmware 诊断 Demo

这是一个面向 Upwork 作品集的公开技术仓库，展示 Embedded Linux、V4L2、
USB/UVC、Camera Pipeline 与固件安全升级能力。所有示例均为自主编写的通用
Demo，不包含公司源码、客户日志、寄存器表或商业产品信息。

## 五个 Demo

### 1. UVC/V4L2 诊断采集

在 Linux Camera 设备上收集系统版本、V4L2 formats/controls、USB topology、
media topology 和相关内核日志，并生成 SHA-256 校验文件。

```bash
./scripts/collect_uvc_diagnostics.sh /dev/video0
```

### 2. 日志脱敏

复制诊断目录，同时遮盖常见邮箱、IPv4、MAC、USB serial、Linux home 和
Windows 用户路径，并输出替换统计。自动脱敏不能替代人工复查。

```bash
python tools/redact_diagnostics.py input-dir output-dir
```

### 3. UVC 带宽估算

估算 YUY2、UYVY、NV12、RGB24、GREY 与 MJPEG 的 active video payload，
并与保守 USB payload budget 比较。

```bash
python tools/uvc_bandwidth.py \
  --width 1920 --height 1080 --fps 30 \
  --format MJPEG --compression-ratio 8 --usb usb2
```

### 4. V4L2 Media Topology 审计

解析 `media-ctl -p` 输出，检查未启用的输出链路和已启用链路两端的
pixel format / resolution 是否一致。

```bash
python tools/v4l2_topology_audit.py examples/media-ctl-rkisp-broken.txt
```

### 5. A/B 固件升级模拟

演示 package hash 校验、inactive slot 分块写入、read-back、trial boot、
确认和回滚，并支持注入写入中途掉电。

```bash
python tools/firmware_ab_simulator.py demo --fail-after-chunk 2
python tools/firmware_ab_simulator.py demo --trial-result fail
```

## 测试

```bash
python -m unittest discover -s tests -v
```

当前测试覆盖：

- UVC payload 计算和 MJPEG 参数校验
- 常见日志字段脱敏
- 正常与断链/格式不匹配的 media topology
- 固件升级成功、写入掉电和 trial boot 失败回滚

## 发布前建议

1. 在个人 Linux Camera 设备或可公开开发板上运行采集脚本。
2. 对输出脱敏，并人工检查后只提交少量 synthetic/public 样例。
3. 截图展示 bandwidth、topology、firmware simulator 和单元测试结果。
4. GitHub 仓库简介可写：

   > Vendor-neutral demos for UVC/V4L2 diagnostics, media topology auditing,
   > USB bandwidth planning, and A/B firmware rollback.

5. Upwork 作品集介绍可直接参考 [`PORTFOLIO.md`](PORTFOLIO.md)。

