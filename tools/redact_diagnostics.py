#!/usr/bin/env python3
"""Copy a diagnostic directory while masking common sensitive identifiers."""

from __future__ import annotations

import argparse
import json
import re
import shutil
from pathlib import Path


PATTERNS = {
    "email": re.compile(r"\b[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}\b", re.I),
    "ipv4": re.compile(
        r"\b(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}"
        r"(?:25[0-5]|2[0-4]\d|1?\d?\d)\b"
    ),
    "mac": re.compile(r"\b(?:[0-9A-F]{2}:){5}[0-9A-F]{2}\b", re.I),
    "linux_home": re.compile(r"/home/[^/\s]+"),
    "windows_user": re.compile(r"[A-Z]:\\Users\\[^\\\s]+", re.I),
    "usb_serial_line": re.compile(
        r"(?im)^(\s*(?:iSerial|ID_SERIAL_SHORT|SerialNumber)\s*[:=]?\s*).+$"
    ),
}


def redact_text(text: str) -> tuple[str, dict[str, int]]:
    counts: dict[str, int] = {}
    for name, pattern in PATTERNS.items():
        if name == "usb_serial_line":
            text, count = pattern.subn(r"\1<REDACTED>", text)
        else:
            text, count = pattern.subn(f"<REDACTED_{name.upper()}>", text)
        if count:
            counts[name] = count
    return text, counts


def redact_directory(source: Path, destination: Path) -> dict:
    if not source.is_dir():
        raise ValueError(f"source is not a directory: {source}")
    if destination.exists():
        raise ValueError(f"destination already exists: {destination}")

    report = {"source": str(source), "destination": str(destination), "files": {}}
    destination.mkdir(parents=True)

    for src in sorted(source.rglob("*")):
        relative = src.relative_to(source)
        dst = destination / relative
        if src.is_dir():
            dst.mkdir(parents=True, exist_ok=True)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        try:
            text = src.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            shutil.copy2(src, dst)
            report["files"][str(relative)] = {"binary_copied": True}
            continue
        redacted, counts = redact_text(text)
        dst.write_text(redacted, encoding="utf-8")
        report["files"][str(relative)] = {"replacements": counts}

    report_path = destination / "redaction-report.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        report = redact_directory(args.source, args.destination)
    except ValueError as exc:
        raise SystemExit(f"error: {exc}") from exc
    changed = sum(
        sum(item.get("replacements", {}).values())
        for item in report["files"].values()
    )
    print(f"Redacted copy: {args.destination}")
    print(f"Total replacements: {changed}")
    print("Manual review is still required before sharing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
