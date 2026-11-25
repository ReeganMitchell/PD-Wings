#!/usr/bin/env python3
"""Convert PNG images to Jo Engine Saturn BIN sprite images."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit(
        "Pillow is required. Install it in the Python environment used to run "
        "this script: python -m pip install Pillow"
    ) from exc


class ConversionError(Exception):
    """An input image cannot be converted as requested."""


def convert_image(
    source: Path,
    destination: Path,
    alpha_threshold: int = 0,
    overwrite: bool = False,
) -> None:
    if not 0 <= alpha_threshold <= 254:
        raise ConversionError("alpha threshold must be between 0 and 254")
    if destination.exists() and not overwrite:
        raise ConversionError(f"refusing to overwrite existing file: {destination}")

    try:
        with Image.open(source) as image:
            if image.format != "PNG":
                raise ConversionError(f"input is not a PNG image: {source}")
            rgba = image.convert("RGBA")
            width, height = rgba.size
            if not 1 <= width <= 0xFFFF or not 1 <= height <= 0xFFFF:
                raise ConversionError(
                    f"image dimensions must be between 1 and 65535 pixels: "
                    f"{width}x{height}"
                )

            output = bytearray(4 + width * height * 2)
            struct.pack_into("<HH", output, 0, width, height)
            pixels = rgba.tobytes()
            for index, (red, green, blue, alpha) in enumerate(
                struct.iter_unpack("4B", pixels)
            ):
                if alpha <= alpha_threshold:
                    pixel = 0
                else:
                    pixel = (
                        0x8000
                        | ((blue >> 3) << 10)
                        | ((green >> 3) << 5)
                        | (red >> 3)
                    )
                struct.pack_into("<H", output, 4 + index * 2, pixel)
            destination.write_bytes(output)
    except OSError as exc:
        raise ConversionError(f"failed to convert {source}: {exc}") from exc


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Convert PNGs to Jo Engine BIN sprites (16-bit Saturn RGB555 with "
            "a 4-byte little-endian width/height header)."
        )
    )
    parser.add_argument("input", type=Path, help="PNG file or directory of PNGs")
    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument(
        "-o",
        "--output",
        type=Path,
        help="destination BIN file (only for a single PNG; defaults beside input)",
    )
    output_group.add_argument(
        "--output-dir",
        type=Path,
        help="destination directory (defaults to the input directory)",
    )
    parser.add_argument(
        "--alpha-threshold",
        type=int,
        default=0,
        metavar="0..254",
        help="treat alpha values at or below this value as transparent (default: 0)",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace existing destination BIN files",
    )
    args = parser.parse_args(argv)

    if not 0 <= args.alpha_threshold <= 254:
        parser.error("--alpha-threshold must be between 0 and 254")
    if not args.input.exists():
        parser.error(f"input does not exist: {args.input}")

    if args.input.is_dir():
        if args.output:
            parser.error("--output can only be used with a single PNG file")
        sources = sorted(
            path
            for path in args.input.iterdir()
            if path.is_file() and path.suffix.lower() == ".png"
        )
        if not sources:
            parser.error(f"no PNG files found in {args.input}")
        output_dir = args.output_dir or args.input
        destinations = [output_dir / f"{source.stem}.BIN" for source in sources]
    elif args.input.is_file() and args.input.suffix.lower() == ".png":
        sources = [args.input]
        if args.output_dir:
            output_dir = args.output_dir
            destinations = [output_dir / f"{args.input.stem}.BIN"]
        else:
            destinations = [args.output or args.input.with_suffix(".BIN")]
    else:
        parser.error("input must be a PNG file or a directory containing PNG files")

    try:
        for source, destination in zip(sources, destinations):
            destination.parent.mkdir(parents=True, exist_ok=True)
            convert_image(
                source,
                destination,
                alpha_threshold=args.alpha_threshold,
                overwrite=args.overwrite,
            )
    except (ConversionError, OSError) as exc:
        print(f"png_to_jo_bin: error: {exc}", file=sys.stderr)
        return 1

    for source, destination in zip(sources, destinations):
        print(f"{source} -> {destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
