#!/usr/bin/env python3
"""Audit a useful subset of `media-ctl -p` output."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path


ENTITY_RE = re.compile(r"^- entity \d+:\s+(.+?)\s+\(\d+ pad")
PAD_RE = re.compile(r"^\s*pad(\d+):\s+(Source|Sink)")
FORMAT_RE = re.compile(r"\[fmt:([A-Z0-9_]+)/(\d+)x(\d+)")
LINK_RE = re.compile(
    r'^\s*->\s*"([^"]+)":(\d+)\s+\[([^\]]+)\]|'
    r'^\s*<-\s*"([^"]+)":(\d+)\s+\[([^\]]+)\]'
)


@dataclass
class Pad:
    entity: str
    index: int
    direction: str
    pixel_format: str | None = None
    width: int | None = None
    height: int | None = None


@dataclass
class Link:
    source_entity: str
    source_pad: int
    sink_entity: str
    sink_pad: int
    enabled: bool
    immutable: bool


def parse_topology(text: str) -> tuple[dict[tuple[str, int], Pad], list[Link]]:
    pads: dict[tuple[str, int], Pad] = {}
    links: list[Link] = []
    entity: str | None = None
    pad: Pad | None = None
    seen: set[tuple[str, int, str, int]] = set()

    for line in text.splitlines():
        entity_match = ENTITY_RE.match(line)
        if entity_match:
            entity = entity_match.group(1)
            pad = None
            continue
        pad_match = PAD_RE.match(line)
        if pad_match and entity:
            pad = Pad(entity, int(pad_match.group(1)), pad_match.group(2))
            pads[(entity, pad.index)] = pad
            format_match = FORMAT_RE.search(line)
            if format_match:
                pad.pixel_format = format_match.group(1)
                pad.width = int(format_match.group(2))
                pad.height = int(format_match.group(3))
            continue
        format_match = FORMAT_RE.search(line)
        if format_match and pad:
            pad.pixel_format = format_match.group(1)
            pad.width = int(format_match.group(2))
            pad.height = int(format_match.group(3))
            continue
        link_match = LINK_RE.match(line)
        if not link_match or not entity or not pad:
            continue

        outgoing = link_match.group(1) is not None
        if outgoing:
            target_entity = link_match.group(1)
            target_pad = int(link_match.group(2))
            flags = link_match.group(3)
            source_entity, source_pad = entity, pad.index
            sink_entity, sink_pad = target_entity, target_pad
        else:
            target_entity = link_match.group(4)
            target_pad = int(link_match.group(5))
            flags = link_match.group(6)
            source_entity, source_pad = target_entity, target_pad
            sink_entity, sink_pad = entity, pad.index

        key = (source_entity, source_pad, sink_entity, sink_pad)
        if key in seen:
            continue
        seen.add(key)
        links.append(
            Link(
                source_entity,
                source_pad,
                sink_entity,
                sink_pad,
                "ENABLED" in flags,
                "IMMUTABLE" in flags,
            )
        )
    return pads, links


def audit(text: str) -> dict:
    pads, links = parse_topology(text)
    warnings: list[str] = []
    enabled_links = [link for link in links if link.enabled]

    source_entities = {pad.entity for pad in pads.values() if pad.direction == "Source"}
    for name in sorted(source_entities):
        if not any(link.source_entity == name and link.enabled for link in links):
            warnings.append(f'entity "{name}" has no enabled outgoing link')

    for link in enabled_links:
        source = pads.get((link.source_entity, link.source_pad))
        sink = pads.get((link.sink_entity, link.sink_pad))
        if not source or not sink:
            continue
        source_fmt = (source.pixel_format, source.width, source.height)
        sink_fmt = (sink.pixel_format, sink.width, sink.height)
        if all(source_fmt) and all(sink_fmt) and source_fmt != sink_fmt:
            warnings.append(
                f'format mismatch: "{link.source_entity}":{link.source_pad} '
                f"{source.pixel_format}/{source.width}x{source.height} -> "
                f'"{link.sink_entity}":{link.sink_pad} '
                f"{sink.pixel_format}/{sink.width}x{sink.height}"
            )

    if not pads:
        warnings.append("no entities/pads were parsed; input format may be unsupported")
    if pads and not enabled_links:
        warnings.append("no enabled media links were found")

    return {
        "entity_count": len({pad.entity for pad in pads.values()}),
        "pad_count": len(pads),
        "link_count": len(links),
        "enabled_link_count": len(enabled_links),
        "immutable_link_count": sum(link.immutable for link in links),
        "warnings": warnings,
        "pads": [asdict(pad) for pad in pads.values()],
        "links": [asdict(link) for link in links],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return exit code 1 when warnings are found.",
    )
    args = parser.parse_args()
    result = audit(args.input.read_text(encoding="utf-8"))
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(
            f"Entities: {result['entity_count']}  Pads: {result['pad_count']}  "
            f"Links: {result['link_count']}  Enabled: {result['enabled_link_count']}"
        )
        if result["warnings"]:
            print("Warnings:")
            for warning in result["warnings"]:
                print(f"  - {warning}")
        else:
            print("No static topology warnings found.")
    return 1 if args.strict and result["warnings"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
