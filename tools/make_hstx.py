#!/usr/bin/env python3
"""Small dependency-free replacement for 3hstool's theme writer."""

from pathlib import Path
import re
import struct
import sys

from PIL import Image


COLOR_IDS = {
    "background_colour": 0x1001,
    "text_colour": 0x1002,
    "button_background_colour": 0x1003,
    "button_border_colour": 0x1004,
    "battery_good_colour": 0x1005,
    "battery_bad_colour": 0x1006,
    "toggle_enabled_colour": 0x1007,
    "toggle_disabled_colour": 0x1008,
    "toggle_slider_colour": 0x1009,
    "progress_bar_foreground_colour": 0x1010,
    "progress_bar_background_colour": 0x1011,
    "scrollbar_colour": 0x1012,
    "led_success": 0x1013,
    "led_failure": 0x1014,
    "smdh_icon_border_colour": 0x1015,
    "checkbox_border_colour": 0x1016,
    "checkbox_check_colour": 0x1017,
    "graph_line_colour": 0x1018,
    "warning_colour": 0x1019,
    "x_colour": 0x101A,
    "battery_charging_colour": 0x101B,
}

IMAGE_IDS = {
    "more_image": 0x2001,
    "battery_image": 0x2002,
    "search_image": 0x2003,
    "settings_image": 0x2004,
    "spinner_image": 0x2005,
    "random_image": 0x2006,
    "background_top_image": 0x2007,
    "background_bottom_image": 0x2008,
    "battery_charging_image": 0x2009,
}


def read_config(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def target_version(repo: Path) -> int:
    text = (repo / "include" / "update.hh").read_text(encoding="utf-8")
    nums = {
        key: int(re.search(rf"#define VERSION_{key}\s+(\d+)", text).group(1))
        for key in ("MAJOR", "MINOR", "PATCH")
    }
    return (nums["MAJOR"] << 20) | (nums["MINOR"] << 10) | nums["PATCH"]


def append_string(blob: bytearray, value: str) -> int:
    offset = len(blob)
    blob.extend(value.encode("utf-8") + b"\0")
    return offset


def make_theme(config_path: Path, output_path: Path) -> None:
    values = read_config(config_path)
    repo = config_path.parents[1]
    blob = bytearray(b"\0")
    descriptors: list[bytes] = []

    name_offset = append_string(blob, values.get("name", "Unknown"))
    author_offset = append_string(blob, values.get("author", "Unknown"))

    for key, value in values.items():
        if key in COLOR_IDS:
            rgba = int(value.removeprefix("#").removeprefix("0x"), 16)
            descriptors.append(struct.pack(">II", COLOR_IDS[key], rgba) + bytes(8))
        elif key in IMAGE_IDS:
            image_path = (config_path.parent / value).resolve()
            image = Image.open(image_path).convert("RGBA")
            width, height = image.size
            if width > 400 or height > 360:
                raise ValueError(f"image too large: {image_path}")
            offset = len(blob)
            blob.extend(image.tobytes())
            descriptors.append(
                struct.pack(">IIHHI", IMAGE_IDS[key], offset, width, height, 0)
            )

    header = struct.pack(
        ">4sIIIIII5I",
        b"HSTX",
        0,
        target_version(repo),
        len(descriptors),
        len(blob),
        name_offset,
        author_offset,
        0, 0, 0, 0, 0,
    )
    output_path.write_bytes(header + b"".join(descriptors) + blob)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: make_hstx.py CONFIG OUTPUT")
    make_theme(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
