#!/usr/bin/env python3
"""Model A/B firmware update, trial boot, confirmation, and rollback."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass


class PowerLoss(RuntimeError):
    pass


@dataclass
class Slot:
    name: str
    version: str
    image: bytes
    valid: bool = True

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.image).hexdigest()


@dataclass
class BootMetadata:
    active: str = "A"
    pending: str | None = None
    previous: str | None = None
    trial_remaining: int = 0


class Device:
    def __init__(self) -> None:
        self.slots = {
            "A": Slot("A", "1.0.0", b"known-good-firmware-v1"),
            "B": Slot("B", "empty", b"", valid=False),
        }
        self.meta = BootMetadata()
        self.events: list[str] = []

    def inactive_slot_name(self) -> str:
        return "B" if self.meta.active == "A" else "A"

    def install(
        self,
        image: bytes,
        version: str,
        expected_sha256: str,
        chunk_size: int = 8,
        fail_after_chunk: int | None = None,
    ) -> None:
        actual = hashlib.sha256(image).hexdigest()
        if actual != expected_sha256:
            raise ValueError("package SHA-256 does not match")
        target_name = self.inactive_slot_name()
        target = self.slots[target_name]
        target.image = b""
        target.version = version
        target.valid = False
        self.events.append(f"erase slot {target_name}")

        written = bytearray()
        for index, start in enumerate(range(0, len(image), chunk_size), start=1):
            written.extend(image[start : start + chunk_size])
            target.image = bytes(written)
            self.events.append(f"write slot {target_name} chunk {index}")
            if fail_after_chunk == index:
                self.events.append("power loss")
                raise PowerLoss(f"simulated power loss after chunk {index}")

        if hashlib.sha256(target.image).hexdigest() != expected_sha256:
            raise RuntimeError("read-back verification failed")
        target.valid = True
        self.meta.previous = self.meta.active
        self.meta.pending = target_name
        self.meta.trial_remaining = 1
        self.events.append(f"verified slot {target_name}; marked pending")

    def select_boot_slot(self) -> str:
        if self.meta.pending:
            candidate = self.slots[self.meta.pending]
            if candidate.valid and self.meta.trial_remaining > 0:
                self.meta.trial_remaining -= 1
                self.events.append(f"trial boot slot {candidate.name}")
                return candidate.name
            self.events.append("pending slot invalid or trial exhausted; rollback")
            self.meta.pending = None
        self.events.append(f"boot active slot {self.meta.active}")
        return self.meta.active

    def confirm(self, slot_name: str) -> None:
        if self.meta.pending != slot_name:
            raise ValueError("only the pending slot can be confirmed")
        self.meta.active = slot_name
        self.meta.pending = None
        self.meta.previous = None
        self.meta.trial_remaining = 0
        self.events.append(f"confirmed slot {slot_name}")

    def report_boot_failure(self, slot_name: str) -> None:
        if self.meta.pending == slot_name:
            self.events.append(f"slot {slot_name} failed self-test; rollback")
            self.meta.pending = None
            self.meta.trial_remaining = 0

    def state(self) -> dict:
        return {
            "metadata": asdict(self.meta),
            "slots": {
                name: {
                    "version": slot.version,
                    "valid": slot.valid,
                    "size": len(slot.image),
                    "sha256": slot.sha256,
                }
                for name, slot in self.slots.items()
            },
            "events": self.events,
        }


def run_demo(fail_after_chunk: int | None, trial_result: str) -> Device:
    device = Device()
    image = b"demo-firmware-v2-with-camera-and-uvc-fixes"
    digest = hashlib.sha256(image).hexdigest()
    try:
        device.install(
            image,
            version="2.0.0",
            expected_sha256=digest,
            fail_after_chunk=fail_after_chunk,
        )
    except PowerLoss:
        device.select_boot_slot()
        return device

    selected = device.select_boot_slot()
    if trial_result == "pass":
        device.confirm(selected)
    else:
        device.report_boot_failure(selected)
        device.select_boot_slot()
    return device


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    demo = sub.add_parser("demo")
    demo.add_argument("--fail-after-chunk", type=int)
    demo.add_argument("--trial-result", choices=["pass", "fail"], default="pass")
    args = parser.parse_args()
    device = run_demo(args.fail_after_chunk, args.trial_result)
    print(json.dumps(device.state(), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
