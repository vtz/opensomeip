#!/usr/bin/env python3
################################################################################
# Copyright (c) 2025 Vinicius Tadeu Zein
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
################################################################################

"""Compute static BSS/data footprint from static_config.h capacity defines.

Reads compile-time capacity constants from include/platform/static/static_config.h
(or a user-supplied path) and estimates total static memory usage for the
no-heap static allocation backend.

Supports -D KEY=VALUE overrides to simulate custom configurations without
rebuilding the header.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

# Estimated object sizes (bytes) used when sizeof() is not available at analysis time.
MESSAGE_OBJECT_SIZE = 360
SESSION_OBJECT_SIZE = 32
RECEIVE_QUEUE_ENTRY_SIZE = 12
BUFFER_SLOT_META_SIZE = 24
FREE_STACK_ENTRY_SIZE = 2
ETL_CONTAINER_OVERHEAD = 2000

DEFAULT_CONFIG_REL = Path("include/platform/static/static_config.h")

# Defaults mirror static_config.h when the header or a define is missing.
DEFAULT_DEFINES: dict[str, int] = {
    "SOMEIP_MESSAGE_POOL_SIZE": 16,
    "SOMEIP_MAX_SESSIONS": 64,
    "SOMEIP_MAX_RECEIVE_QUEUE": 32,
    "SOMEIP_BYTE_POOL_SMALL_COUNT": 32,
    "SOMEIP_BYTE_POOL_MEDIUM_COUNT": 16,
    "SOMEIP_BYTE_POOL_LARGE_COUNT": 4,
    "SOMEIP_BYTE_POOL_SMALL_SIZE": 256,
    "SOMEIP_BYTE_POOL_MEDIUM_SIZE": 1500,
    "SOMEIP_BYTE_POOL_LARGE_SIZE": 65536,
    "SOMEIP_PIMPL_EVENTPUB_SIZE": 512,
    "SOMEIP_PIMPL_EVENTSUB_SIZE": 512,
    "SOMEIP_PIMPL_RPCCLIENT_SIZE": 512,
    "SOMEIP_PIMPL_RPCSERVER_SIZE": 512,
    "SOMEIP_PIMPL_SDCLIENT_SIZE": 512,
    "SOMEIP_PIMPL_SDSERVER_SIZE": 512,
}

PIMPL_DEFINE_KEYS = (
    "SOMEIP_PIMPL_EVENTPUB_SIZE",
    "SOMEIP_PIMPL_EVENTSUB_SIZE",
    "SOMEIP_PIMPL_RPCCLIENT_SIZE",
    "SOMEIP_PIMPL_RPCSERVER_SIZE",
    "SOMEIP_PIMPL_SDCLIENT_SIZE",
    "SOMEIP_PIMPL_SDSERVER_SIZE",
)

TUNING_HINTS: dict[str, str] = {
    "Message object pool": "SOMEIP_MESSAGE_POOL_SIZE",
    "Buffer pool tier 0 (small)": "SOMEIP_BYTE_POOL_SMALL_COUNT, SOMEIP_BYTE_POOL_SMALL_SIZE",
    "Buffer pool tier 1 (medium)": "SOMEIP_BYTE_POOL_MEDIUM_COUNT, SOMEIP_BYTE_POOL_MEDIUM_SIZE",
    "Buffer pool tier 2 (large)": (
        "SOMEIP_BYTE_POOL_LARGE_COUNT, SOMEIP_BYTE_POOL_LARGE_SIZE, SOMEIP_MAX_TCP_PAYLOAD_SIZE"
    ),
    "Buffer pool metadata (slots)": (
        "SOMEIP_BYTE_POOL_SMALL_COUNT, SOMEIP_BYTE_POOL_MEDIUM_COUNT, SOMEIP_BYTE_POOL_LARGE_COUNT"
    ),
    "Buffer pool free-stacks": (
        "SOMEIP_BYTE_POOL_SMALL_COUNT, SOMEIP_BYTE_POOL_MEDIUM_COUNT, SOMEIP_BYTE_POOL_LARGE_COUNT"
    ),
    "Session pool": "SOMEIP_MAX_SESSIONS",
    "Receive queues (2x transport)": "SOMEIP_MAX_RECEIVE_QUEUE",
    "Pimpl storage (6 classes)": "SOMEIP_PIMPL_*_SIZE",
    "ETL container overhead": (
        "SOMEIP_DEFAULT_VECTOR_CAPACITY, SOMEIP_DEFAULT_MAP_CAPACITY, "
        "SOMEIP_DEFAULT_QUEUE_CAPACITY, SOMEIP_DEFAULT_STRING_CAPACITY"
    ),
}


class Component:
    """One row in the static memory budget table."""

    __slots__ = ("count", "name", "note", "total", "unit_label", "unit_size")

    def __init__(
        self,
        name: str,
        count: int | None,
        unit_size: int | None,
        total: int,
        unit_label: str = "",
        note: str = "",
    ) -> None:
        self.name = name
        self.count = count
        self.unit_size = unit_size
        self.total = total
        self.unit_label = unit_label or (
            format_unit_size(unit_size) if unit_size is not None else ""
        )
        self.note = note


def format_unit_size(size: int | None) -> str:
    if size is None:
        return ""
    if size >= 1024:
        return f"{size:,} B"
    return f"{size} B"


def format_count(count: int | None) -> str:
    if count is None:
        return "est."
    return f"{count:,}"


def format_total(total: int) -> str:
    return f"{total:,} B"


def parse_numeric_value(raw: str) -> int:
    """Parse a C preprocessor numeric literal."""
    token = raw.strip()
    if "//" in token:
        token = token.split("//", 1)[0].strip()
    if "/*" in token:
        token = token.split("/*", 1)[0].strip()
    token = token.rstrip("uUlL")
    if token.startswith(("0x", "0X")):
        return int(token, 16)
    return int(token, 10)


def parse_define_overrides(overrides: list[str]) -> dict[str, int]:
    parsed: dict[str, int] = {}
    for item in overrides:
        if "=" not in item:
            raise ValueError(f"Invalid -D override (expected KEY=VALUE): {item!r}")
        key, value = item.split("=", 1)
        key = key.strip()
        if not key:
            raise ValueError(f"Invalid -D override (empty key): {item!r}")
        parsed[key] = parse_numeric_value(value)
    return parsed


def parse_static_config(text: str) -> dict[str, int]:
    """Extract numeric #define values from a static_config.h header."""
    defines: dict[str, int] = {}

    ifndef_block = re.compile(
        r"#ifndef\s+(\w+)\s*\n\s*#define\s+\1\s+([^\n]+)",
        re.MULTILINE,
    )
    for match in ifndef_block.finditer(text):
        defines[match.group(1)] = parse_numeric_value(match.group(2))

    define_line = re.compile(r"^\s*#define\s+(\w+)\s+([^\n]+)", re.MULTILINE)
    for match in define_line.finditer(text):
        key = match.group(1)
        if key in defines:
            continue
        value = match.group(2).strip()
        if not value or value.startswith("("):
            continue
        try:
            defines[key] = parse_numeric_value(value)
        except ValueError:
            continue

    return defines


def resolve_defines(
    config_path: Path,
    overrides: dict[str, int],
    require_file: bool,
) -> tuple[dict[str, int], Path | None]:
    values = dict(DEFAULT_DEFINES)

    source: Path | None = None
    if config_path.is_file():
        source = config_path
        values.update(parse_static_config(config_path.read_text(encoding="utf-8")))
    elif require_file:
        raise FileNotFoundError(f"Config header not found: {config_path}")

    values.update(overrides)
    return values, source


def get_int(defines: dict[str, int], key: str) -> int:
    if key not in defines:
        raise KeyError(f"Missing required define: {key}")
    return defines[key]


def compute_components(defines: dict[str, int]) -> list[Component]:
    message_pool_size = get_int(defines, "SOMEIP_MESSAGE_POOL_SIZE")
    small_count = get_int(defines, "SOMEIP_BYTE_POOL_SMALL_COUNT")
    medium_count = get_int(defines, "SOMEIP_BYTE_POOL_MEDIUM_COUNT")
    large_count = get_int(defines, "SOMEIP_BYTE_POOL_LARGE_COUNT")
    small_size = get_int(defines, "SOMEIP_BYTE_POOL_SMALL_SIZE")
    medium_size = get_int(defines, "SOMEIP_BYTE_POOL_MEDIUM_SIZE")
    large_size = get_int(defines, "SOMEIP_BYTE_POOL_LARGE_SIZE")
    max_sessions = get_int(defines, "SOMEIP_MAX_SESSIONS")
    max_receive_queue = get_int(defines, "SOMEIP_MAX_RECEIVE_QUEUE")

    total_slots = small_count + medium_count + large_count
    pimpl_sizes = [get_int(defines, key) for key in PIMPL_DEFINE_KEYS]
    pimpl_total = sum(pimpl_sizes)
    if pimpl_sizes and len(set(pimpl_sizes)) == 1:
        pimpl_unit_label = f"{pimpl_sizes[0]:,} B"
    elif pimpl_sizes:
        pimpl_unit_label = f"{pimpl_total // len(pimpl_sizes):,} B avg"
    else:
        pimpl_unit_label = "0 B"

    components = [
        Component(
            name="Message object pool",
            count=message_pool_size,
            unit_size=MESSAGE_OBJECT_SIZE,
            total=message_pool_size * MESSAGE_OBJECT_SIZE,
            unit_label=f"~{MESSAGE_OBJECT_SIZE} B",
            note="sizeof(Message) incl. payload handle, ref_count_",
        ),
        Component(
            name="Buffer pool tier 0 (small)",
            count=small_count,
            unit_size=small_size,
            total=small_count * small_size,
        ),
        Component(
            name="Buffer pool tier 1 (medium)",
            count=medium_count,
            unit_size=medium_size,
            total=medium_count * medium_size,
        ),
        Component(
            name="Buffer pool tier 2 (large)",
            count=large_count,
            unit_size=large_size,
            total=large_count * large_size,
        ),
        Component(
            name="Buffer pool metadata (slots)",
            count=total_slots,
            unit_size=BUFFER_SLOT_META_SIZE,
            total=total_slots * BUFFER_SLOT_META_SIZE,
        ),
        Component(
            name="Buffer pool free-stacks",
            count=total_slots,
            unit_size=FREE_STACK_ENTRY_SIZE,
            total=total_slots * FREE_STACK_ENTRY_SIZE,
        ),
        Component(
            name="Session pool",
            count=max_sessions,
            unit_size=SESSION_OBJECT_SIZE,
            total=max_sessions * SESSION_OBJECT_SIZE,
            unit_label=f"{SESSION_OBJECT_SIZE} B",
        ),
        Component(
            name="Receive queues (2x transport)",
            count=2 * max_receive_queue,
            unit_size=RECEIVE_QUEUE_ENTRY_SIZE,
            total=2 * max_receive_queue * RECEIVE_QUEUE_ENTRY_SIZE,
            unit_label=f"~{RECEIVE_QUEUE_ENTRY_SIZE} B",
        ),
        Component(
            name="Pimpl storage (6 classes)",
            count=len(pimpl_sizes),
            unit_size=pimpl_sizes[0] if pimpl_sizes else 0,
            total=pimpl_total,
            unit_label=pimpl_unit_label,
        ),
        Component(
            name="ETL container overhead",
            count=None,
            unit_size=None,
            total=ETL_CONTAINER_OVERHEAD,
            unit_label="est.",
        ),
    ]
    return components


def find_largest(components: list[Component]) -> Component:
    return max(components, key=lambda item: item.total)


def format_table(
    components: list[Component],
    total_bytes: int,
    largest: Component,
    config_path: Path,
    source: Path | None,
) -> str:
    title = "OpenSOMEIP Static Memory Budget"
    width = 64
    lines = [
        f"{'=' * 16} {title} {'=' * 16}",
        f"{'Component':<32}{'Count':>8}  {'Unit Size':>10}  {'Total':>12}",
        "-" * width,
    ]

    for component in components:
        count_str = format_count(component.count)
        unit_str = component.unit_label
        total_str = format_total(component.total)
        lines.append(f"{component.name:<32}{count_str:>8}  {unit_str:>10}  {total_str:>12}")
        if component.note:
            lines.append(f"  ({component.note})")

    lines.append("-" * width)
    if total_bytes >= 1024:
        total_display = f"~{total_bytes / 1024:.0f} KB"
    else:
        total_display = format_total(total_bytes)
    lines.append(f"{'TOTAL STATIC FOOTPRINT':<32}{'':>8}  {'':>10}  {total_display:>12}")
    lines.append("=" * width)

    percent = (largest.total / total_bytes * 100.0) if total_bytes else 0.0
    lines.append(f"Largest contributor: {largest.name} ({percent:.1f}%)")
    hint = TUNING_HINTS.get(largest.name, "Review static_config.h capacity defines")
    lines.append(f"Tune via: {hint}")

    if source is not None:
        lines.append(f"Config: {source}")
    elif config_path.is_file():
        lines.append(f"Config: {config_path}")
    else:
        lines.append(f"Config: {config_path} (not found; using built-in defaults)")

    return "\n".join(lines)


def build_json_report(
    components: list[Component],
    total_bytes: int,
    largest: Component,
    defines: dict[str, int],
    config_path: Path,
    source: Path | None,
) -> dict[str, object]:
    percent = (largest.total / total_bytes * 100.0) if total_bytes else 0.0
    return {
        "config_path": str(config_path),
        "config_source": str(source) if source is not None else None,
        "defines": defines,
        "components": [
            {
                "name": component.name,
                "count": component.count,
                "unit_size": component.unit_size,
                "unit_label": component.unit_label,
                "total_bytes": component.total,
                "note": component.note,
            }
            for component in components
        ],
        "total_bytes": total_bytes,
        "largest_contributor": {
            "name": largest.name,
            "total_bytes": largest.total,
            "percent": round(percent, 1),
        },
        "tuning_hint": TUNING_HINTS.get(
            largest.name,
            "Review static_config.h capacity defines",
        ),
    }


def build_arg_parser() -> argparse.ArgumentParser:
    project_root = Path(__file__).resolve().parent.parent
    default_config = project_root / DEFAULT_CONFIG_REL

    parser = argparse.ArgumentParser(
        description=(
            "Compute static BSS/data footprint for the OpenSOMEIP static "
            "allocation backend from static_config.h capacity defines."
        ),
    )
    parser.add_argument(
        "config",
        nargs="?",
        default=str(default_config),
        help=(f"Path to static_config.h (default: {DEFAULT_CONFIG_REL.as_posix()})"),
    )
    parser.add_argument(
        "-D",
        "--define",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Override a capacity define (repeatable, e.g. -D SOMEIP_MESSAGE_POOL_SIZE=32)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON instead of a formatted table",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    project_root = Path(__file__).resolve().parent.parent
    default_config = project_root / DEFAULT_CONFIG_REL

    config_path = Path(args.config)
    if not config_path.is_absolute():
        config_path = project_root / config_path

    require_file = config_path.resolve() != default_config.resolve()

    try:
        overrides = parse_define_overrides(args.define)
        defines, source = resolve_defines(config_path, overrides, require_file)
        components = compute_components(defines)
        total_bytes = sum(component.total for component in components)
        largest = find_largest(components)
    except (FileNotFoundError, ValueError, KeyError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.json:
        report = build_json_report(
            components,
            total_bytes,
            largest,
            defines,
            config_path,
            source,
        )
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(
            format_table(
                components,
                total_bytes,
                largest,
                config_path,
                source,
            )
        )

    return 0


if __name__ == "__main__":
    sys.exit(main())
