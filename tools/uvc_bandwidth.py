#!/usr/bin/env python3
"""Estimate active-video bandwidth for common UVC formats."""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass


BITS_PER_PIXEL = {
    "GREY": 8,
    "NV12": 12,
    "YUY2": 16,
    "UYVY": 16,
    "RGB24": 24,
}

# Conservative usable payload assumptions for an initial feasibility check.
# These are not endpoint guarantees and intentionally leave protocol margin.
USB_PAYLOAD_MBPS = {
    "usb2": 320.0,
    "usb3": 3200.0,
}


@dataclass(frozen=True)
class BandwidthResult:
    width: int
    height: int
    fps: float
    pixel_format: str
    usb: str
    transfer: str
    raw_mbps: float
    estimated_video_mbps: float
    budget_mbps: float
    utilization_percent: float
    headroom_mbps: float
    assessment: str
    note: str


def calculate(
    width: int,
    height: int,
    fps: float,
    pixel_format: str,
    usb: str,
    transfer: str = "isochronous",
    compression_ratio: float | None = None,
    budget_fraction: float = 0.8,
) -> BandwidthResult:
    if width <= 0 or height <= 0 or fps <= 0:
        raise ValueError("width, height, and fps must be positive")
    if not 0 < budget_fraction <= 1:
        raise ValueError("budget_fraction must be in the range (0, 1]")

    fmt = pixel_format.upper()
    usb_key = usb.lower()
    transfer_key = transfer.lower()
    if usb_key not in USB_PAYLOAD_MBPS:
        raise ValueError(f"unsupported USB link: {usb}")
    if transfer_key not in {"isochronous", "bulk"}:
        raise ValueError("transfer must be isochronous or bulk")

    if fmt == "MJPEG":
        if compression_ratio is None or compression_ratio <= 1:
            raise ValueError("MJPEG requires --compression-ratio greater than 1")
        raw_mbps = width * height * fps * 16 / 1_000_000
        video_mbps = raw_mbps / compression_ratio
        note = (
            "MJPEG size is content-dependent; the compression ratio is an "
            "assumption and peak frames may be larger."
        )
    elif fmt in BITS_PER_PIXEL:
        raw_mbps = width * height * fps * BITS_PER_PIXEL[fmt] / 1_000_000
        video_mbps = raw_mbps
        note = (
            "Estimate covers active image payload only; allow additional margin "
            "for UVC headers, timing, retries, and shared-bus traffic."
        )
    else:
        raise ValueError(f"unsupported pixel format: {pixel_format}")

    budget = USB_PAYLOAD_MBPS[usb_key] * budget_fraction
    utilization = video_mbps / budget * 100
    headroom = budget - video_mbps
    if utilization <= 70:
        assessment = "comfortable estimate"
    elif utilization <= 90:
        assessment = "tight; validate descriptors and sustained streaming"
    elif utilization <= 100:
        assessment = "high risk; little margin for overhead or contention"
    else:
        assessment = "does not fit the selected conservative budget"

    return BandwidthResult(
        width=width,
        height=height,
        fps=fps,
        pixel_format=fmt,
        usb=usb_key,
        transfer=transfer_key,
        raw_mbps=round(raw_mbps, 3),
        estimated_video_mbps=round(video_mbps, 3),
        budget_mbps=round(budget, 3),
        utilization_percent=round(utilization, 2),
        headroom_mbps=round(headroom, 3),
        assessment=assessment,
        note=note,
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--fps", type=float, required=True)
    parser.add_argument(
        "--format",
        dest="pixel_format",
        required=True,
        choices=sorted([*BITS_PER_PIXEL, "MJPEG"]),
    )
    parser.add_argument("--usb", required=True, choices=sorted(USB_PAYLOAD_MBPS))
    parser.add_argument(
        "--transfer",
        default="isochronous",
        choices=["isochronous", "bulk"],
    )
    parser.add_argument(
        "--compression-ratio",
        type=float,
        help="Required for MJPEG; for example, 8 means 8:1.",
    )
    parser.add_argument(
        "--budget-fraction",
        type=float,
        default=0.8,
        help="Fraction of the conservative payload assumption to budget (default: 0.8).",
    )
    parser.add_argument("--json", action="store_true")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        result = calculate(
            width=args.width,
            height=args.height,
            fps=args.fps,
            pixel_format=args.pixel_format,
            usb=args.usb,
            transfer=args.transfer,
            compression_ratio=args.compression_ratio,
            budget_fraction=args.budget_fraction,
        )
    except ValueError as exc:
        raise SystemExit(f"error: {exc}") from exc

    if args.json:
        print(json.dumps(asdict(result), indent=2))
    else:
        print(f"Mode:       {result.width}x{result.height} {result.pixel_format} @ {result.fps:g} fps")
        print(f"Transport:  {result.usb.upper()} / {result.transfer}")
        print(f"Raw payload:{result.raw_mbps:10.3f} Mb/s")
        print(f"Estimate:   {result.estimated_video_mbps:10.3f} Mb/s")
        print(f"Budget:     {result.budget_mbps:10.3f} Mb/s")
        print(f"Utilization:{result.utilization_percent:9.2f}%")
        print(f"Headroom:   {result.headroom_mbps:10.3f} Mb/s")
        print(f"Assessment: {result.assessment}")
        print(f"Note:       {result.note}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
