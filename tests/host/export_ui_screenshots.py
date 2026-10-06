#!/usr/bin/env python3
"""Export LVGL test snapshots as lossless PNGs using only the Python standard library."""

import argparse
from pathlib import Path
import struct
import zlib


SNAPSHOTS = (
    "ui_home_reference", "ui_quota_details", "ui_cost", "ui_network_online",
    "ui_settings_idle", "ui_waiting", "ui_95", "ui_100", "ui_zero",
    "ui_quota_details_missing_weekly", "ui_quota_details_extremes",
    "ui_quota_details_unsynced", "ui_cost_waiting", "ui_cost_increase",
    "ui_cost_invalid", "ui_cost_zero", "ui_network_offline",
    "ui_network_unconfigured", "ui_settings_confirm", "ui_settings_waiting",
    "ui_settings_connecting", "ui_settings_api_error", "ui_settings_stopping",
    "ui_settings_failed", "ui_settings_first_boot", "ui_settings_startup_error",
)


def chunk(kind, payload):
    checksum = zlib.crc32(kind + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def export(source, destination):
    with source.open("rb") as stream:
        if stream.readline() != b"P6\n":
            raise ValueError(f"Unsupported PPM header: {source}")
        width, height = map(int, stream.readline().split())
        if (width, height) != (466, 466) or stream.readline() != b"255\n":
            raise ValueError(f"Expected a 466x466 RGB888 LVGL snapshot: {source}")
        pixels = stream.read()
    stride = width * 3
    if len(pixels) != stride * height:
        raise ValueError(f"Invalid pixel data length: {source}")
    rows = b"".join(b"\0" + pixels[y * stride:(y + 1) * stride] for y in range(height))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    destination.write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    )


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=root / "build" / "host")
    parser.add_argument("--output", type=Path, default=root / "docs" / "screenshots")
    args = parser.parse_args()
    missing = [name for name in SNAPSHOTS if not (args.source / f"{name}.ppm").is_file()]
    if missing:
        parser.error("Run the host UI test first; missing snapshots: " + ", ".join(missing))
    args.output.mkdir(parents=True, exist_ok=True)
    for name in SNAPSHOTS:
        export(args.source / f"{name}.ppm", args.output / f"{name}.png")
    print(f"Exported {len(SNAPSHOTS)} LVGL snapshots (466x466) to {args.output}")


if __name__ == "__main__":
    main()
