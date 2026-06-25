# Embedded Camera & Firmware Diagnostics Lab

Public, vendor-neutral demos for embedded Linux camera, USB/UVC, V4L2 media
pipelines, and safe firmware-upgrade engineering.

This repository is designed as a technical portfolio: every demo is small
enough to review quickly, but represents a real task that appears in camera,
driver, BSP, and firmware projects.

## Demo catalog

| Demo | Engineering question | Main skills |
|---|---|---|
| [1. Diagnostic collector](#1-uvcv4l2-diagnostic-collector) | What evidence should be collected before changing a driver? | Linux, V4L2, USB, shell |
| [2. Diagnostic redactor](#2-diagnostic-redactor) | How can logs be shared without leaking common identifiers? | Python, privacy, support workflow |
| [3. UVC bandwidth planner](#3-uvc-bandwidth-planner) | Can a requested video mode fit the selected USB link? | UVC, formats, bandwidth analysis |
| [4. V4L2 topology auditor](#4-v4l2-media-topology-auditor) | Are required media links enabled and formats consistent? | V4L2, media controller, ISP pipeline |
| [5. A/B firmware simulator](#5-ab-firmware-upgrade-simulator) | What happens if power is lost or a trial image fails? | Bootloader, flash, integrity, rollback |

All Python tools use only the standard library.

## Quick start

```bash
python -m unittest discover -s tests -v

python tools/uvc_bandwidth.py \
  --width 3840 --height 2160 --fps 30 --format YUY2 --usb usb3

python tools/v4l2_topology_audit.py \
  examples/media-ctl-rkisp-good.txt

python tools/firmware_ab_simulator.py demo
```

On a Linux camera target:

```bash
chmod +x scripts/collect_uvc_diagnostics.sh
./scripts/collect_uvc_diagnostics.sh /dev/video0

python tools/redact_diagnostics.py \
  uvc-diagnostics-YYYYMMDD-HHMMSS \
  uvc-diagnostics-redacted
```

## 1. UVC/V4L2 diagnostic collector

[`scripts/collect_uvc_diagnostics.sh`](scripts/collect_uvc_diagnostics.sh)
collects a read-only evidence bundle:

- kernel and operating-system version;
- V4L2 capabilities, controls, formats, sizes, and frame intervals;
- USB topology and descriptors when available;
- media-controller topology;
- relevant kernel messages;
- SHA-256 checksums for bundle integrity.

It intentionally does not change controls, flash firmware, or start a long
stream. Run it once on a known-good setup and once on the failing setup before
comparing driver behavior.

## 2. Diagnostic redactor

[`tools/redact_diagnostics.py`](tools/redact_diagnostics.py) copies a diagnostic
directory while masking common IPv4 addresses, MAC addresses, email addresses,
USB serial fields, Linux home paths, and Windows user paths.

Redaction is a review aid, not a security guarantee. The generated
`redaction-report.json` lists how many replacements were made in each file so
an engineer can perform a final manual review.

## 3. UVC bandwidth planner

[`tools/uvc_bandwidth.py`](tools/uvc_bandwidth.py) estimates active-video
payload for YUY2, UYVY, NV12, RGB24, GREY, and MJPEG modes, then compares that
payload with a deliberately conservative USB payload budget.

Example:

```bash
python tools/uvc_bandwidth.py \
  --width 1920 --height 1080 --fps 30 \
  --format MJPEG --compression-ratio 8 --usb usb2
```

The result is an engineering estimate, not a USB scheduler. Real feasibility
still depends on endpoint descriptors, transfer type, host controller,
microframe allocation, UVC headers, other devices, and implementation quality.

## 4. V4L2 media topology auditor

[`tools/v4l2_topology_audit.py`](tools/v4l2_topology_audit.py) parses the useful
subset of `media-ctl -p` output and reports:

- entities with no enabled outgoing link;
- enabled links with conflicting source/sink formats;
- immutable links;
- a concise entity/link summary;
- machine-readable JSON output for CI or BSP smoke tests.

Two synthetic RKISP-style examples are included:

- [`examples/media-ctl-rkisp-good.txt`](examples/media-ctl-rkisp-good.txt)
- [`examples/media-ctl-rkisp-broken.txt`](examples/media-ctl-rkisp-broken.txt)

The parser is intentionally tolerant because media-ctl output varies across
kernel and driver versions. Unknown lines are preserved as ignored input
rather than treated as proof of a healthy pipeline.

## 5. A/B firmware upgrade simulator

[`tools/firmware_ab_simulator.py`](tools/firmware_ab_simulator.py) models:

1. package integrity validation;
2. chunked writes to the inactive slot;
3. read-back verification;
4. pending-slot selection;
5. trial boot;
6. confirmation or rollback.

It can inject power loss after a selected write chunk:

```bash
python tools/firmware_ab_simulator.py demo --fail-after-chunk 2
```

The simulator is not production bootloader code. Its purpose is to make
upgrade-state transitions and failure-handling rules explicit before they are
implemented against a real SPI NOR/NAND or eMMC backend.

## Client-style deliverable

Raw logs are rarely enough. Use
[`templates/diagnostic-report.md`](templates/diagnostic-report.md) to summarize
the symptom, evidence, earliest proven failure, recommended fix, risks, and
verification plan.

## Repository layout

```text
.
├── examples/
├── scripts/
│   └── collect_uvc_diagnostics.sh
├── templates/
│   └── diagnostic-report.md
├── tests/
└── tools/
    ├── firmware_ab_simulator.py
    ├── redact_diagnostics.py
    ├── uvc_bandwidth.py
    └── v4l2_topology_audit.py
```

## Confidentiality

The repository contains no customer source code, firmware image, schematic,
register table, product identifier, or captured commercial-device log. Review
and anonymize every artifact before publishing it.

## License

Add an MIT or Apache-2.0 license only after confirming that all files are your
own work and may be published.

