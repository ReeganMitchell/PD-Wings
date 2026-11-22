#!/usr/bin/env python3
"""Convert PNG images to 24-bit TGA with transparent pixels replaced by a key."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit(
        "Pillow is required. Install it in the Python environment used to run "
        "this script: python -m pip install Pillow"
    ) from exc


DEFAULT_KEY_COLOR = (0, 255, 0)


class ConversionError(Exception):
    """An input image cannot be converted as requested."""


def parse_color(value: str) -> tuple[int, int, int]:
    match = re.fullmatch(r"#?([0-9A-Fa-f]{6})", value)
    if not match:
        raise argparse.ArgumentTypeError("color must be a hex RGB value like 00FF00")
    rgb = match.group(1)
    return tuple(int(rgb[index : index + 2], 16) for index in (0, 2, 4))


def convert_image(
    source: Path,
    destination: Path,
    key_color: tuple[int, int, int] = DEFAULT_KEY_COLOR,
    alpha_threshold: int = 0,
    overwrite: bool = False,
) -> None:
    if destination.exists() and not overwrite:
        raise ConversionError(f"refusing to overwrite existing file: {destination}")

    try:
        with Image.open(source) as image:
            rgba = image.convert("RGBA")
            output = Image.new("RGB", rgba.size)
            output.putdata(
                [
                    key_color if alpha <= alpha_threshold else (red, green, blue)
                    for red, green, blue, alpha in rgba.getdata()
                ]
            )
            output.save(destination, format="TGA")
    except OSError as exc:
        raise ConversionError(f"failed to convert {source}: {exc}") from exc


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Convert PNGs to 24-bit TGA, replacing transparent pixels with "
            "a solid color key (default: green)."
        )
    )
    parser.add_argument("input", type=Path, help="PNG file or directory of PNGs")
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=None,
        help="destination directory (defaults to input file's directory)",
    )
    parser.add_argument(
        "--key-color",
        type=parse_color,
        default=DEFAULT_KEY_COLOR,
        metavar="RRGGBB",
        help="RGB color used for transparent pixels (default: 00FF00)",
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
        help="replace existing destination TGA files",
    )
    args = parser.parse_args(argv)

    if not 0 <= args.alpha_threshold <= 254:
        parser.error("--alpha-threshold must be between 0 and 254")
    if not args.input.exists():
        parser.error(f"input does not exist: {args.input}")

    if args.input.is_dir():
        sources = sorted(
            path
            for path in args.input.iterdir()
            if path.is_file() and path.suffix.lower() == ".png"
        )
        if not sources:
            parser.error(f"no PNG files found in {args.input}")
        output_dir = args.output_dir or args.input
    elif args.input.is_file() and args.input.suffix.lower() == ".png":
        sources = [args.input]
        output_dir = args.output_dir or args.input.parent
    else:
        parser.error("input must be a PNG file or a directory containing PNG files")

    try:
        output_dir.mkdir(parents=True, exist_ok=True)
        converted: list[tuple[Path, Path]] = []
        for source in sources:
            destination = output_dir / f"{source.stem}.TGA"
            convert_image(
                source,
                destination,
                key_color=args.key_color,
                alpha_threshold=args.alpha_threshold,
                overwrite=args.overwrite,
            )
            converted.append((source, destination))
    except (ConversionError, OSError) as exc:
        print(f"png_to_keyed_tga: error: {exc}", file=sys.stderr)
        return 1

    for source, destination in converted:
        print(f"{source} -> {destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
