# Embedded Camera & Firmware Diagnostics Lab

Public, vendor-neutral demos for embedded Linux camera, USB/UVC, V4L2 media
pipelines, Android Camera HAL/App integration, and safe firmware upgrades.

## Featured Driver / HAL / App camera stack

| Layer | Demo | Capability shown |
|---|---|---|
| Linux Driver | [`demos/driver-v4l2-sensor`](demos/driver-v4l2-sensor) | I2C/regmap, runtime PM, V4L2 controls, RAW10 format and stream lifecycle |
| Android HAL | [`demos/hal3-camera-pipeline`](demos/hal3-camera-pipeline) | Stream validation, capture requests, buffer ownership, results, flush/errors |
| Android App | [`demos/android-camera2-jni-app`](demos/android-camera2-jni-app) | Camera2 lifecycle, preview + YUV analysis, ImageReader back pressure and JNI/C++ |

```text
Android Camera2 App
       |
       v
Camera HAL3 request and buffer pipeline
       |
       v
V4L2 / media-controller / ISP
       |
       v
I2C image sensor driver + MIPI CSI-2
```

The examples are intentionally vendor-neutral. They demonstrate engineering
structure and boundary handling without publishing customer source code or
pretending that fictional registers support a real sensor.

## Supporting diagnostic demos

| Demo | Engineering question | Main skills |
|---|---|---|
| Diagnostic collector | What evidence should be collected before changing a driver? | Linux, V4L2, USB, shell |
| Diagnostic redactor | How can logs be shared without leaking common identifiers? | Python, privacy, support workflow |
| UVC bandwidth planner | Can a requested video mode fit the selected USB link? | UVC, formats, bandwidth analysis |
| V4L2 topology auditor | Are required media links enabled and formats consistent? | V4L2, media controller, ISP pipeline |
| A/B firmware simulator | What happens if power is lost or a trial image fails? | Bootloader, flash, integrity, rollback |

## Quick examples

```bash
python tools/uvc_bandwidth.py \
  --width 3840 --height 2160 --fps 30 --format YUY2 --usb usb3

python tools/v4l2_topology_audit.py examples/media-ctl-rkisp-good.txt
python tools/firmware_ab_simulator.py demo --fail-after-chunk 2
```

On a Linux camera target:

```bash
chmod +x scripts/collect_uvc_diagnostics.sh
./scripts/collect_uvc_diagnostics.sh /dev/video0
python tools/redact_diagnostics.py input-diagnostics output-redacted
```

## Diagnostic collector

[`scripts/collect_uvc_diagnostics.sh`](scripts/collect_uvc_diagnostics.sh)
collects kernel/OS information, V4L2 capabilities and controls, USB topology,
media-controller topology, relevant kernel messages and SHA-256 checksums.
It is read-only and does not change controls or flash firmware.

## UVC bandwidth planner

[`tools/uvc_bandwidth.py`](tools/uvc_bandwidth.py) estimates active-video
payload for YUY2, UYVY, NV12, RGB24, GREY and MJPEG, then compares it with a
conservative USB payload budget. Real feasibility still depends on endpoint
descriptors, transfer type, host scheduling, UVC headers and shared traffic.

## V4L2 media topology auditor

[`tools/v4l2_topology_audit.py`](tools/v4l2_topology_audit.py) checks parsed
`media-ctl -p` output for missing enabled links and conflicting source/sink
formats. Synthetic RKISP-style good and broken examples are included.

## A/B firmware upgrade simulator

[`tools/firmware_ab_simulator.py`](tools/firmware_ab_simulator.py) models
package validation, inactive-slot writes, read-back verification, pending-slot
selection, trial boot, confirmation, power loss and rollback.

## Client-style deliverable

Use [`templates/diagnostic-report.md`](templates/diagnostic-report.md) to turn
raw evidence into a concise symptom, root-cause, fix and verification report.

## Repository layout

```text
.
├── demos/
│   ├── android-camera2-jni-app/
│   ├── driver-v4l2-sensor/
│   └── hal3-camera-pipeline/
├── examples/
├── scripts/
├── templates/
├── tests/
└── tools/
```

## Confidentiality

The repository contains no customer source code, firmware image, schematic,
register table, product identifier or captured commercial-device log. Review
and anonymize every artifact before publishing it.

## License

Add an MIT or Apache-2.0 license only after confirming that all files are your
own work and may be published.
