#!/usr/bin/env python3
"""Build PNG, MAP, and MODMAP assets for a PD-Wings zone."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


def find_repository_root(start: Path) -> Path:
    for path in (start, *start.parents):
        if (path / "tools" / "tiled_to_jo_map.py").is_file():
            return path
    raise RuntimeError("could not find the PD-Wings repository root")


def infer_map_prefix(zone: str) -> str:
    match = re.search(r"(\d+)$", zone, re.IGNORECASE)
    if match:
        zone_label = re.match(r"[A-Z]+", zone, re.IGNORECASE)
        if zone_label:
            return f"{zone_label.group(0)[0]}{match.group(1)}".upper()
    return re.sub(r"[^A-Za-z0-9]", "", zone).upper()[:4]


def run(command: list[str], dry_run: bool) -> None:
    print(" ".join(str(part) for part in command))
    if not dry_run:
        subprocess.run(command, check=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("zone", help="zone directory name, for example ZONE1")
    parser.add_argument(
        "--map-prefix",
        default=None,
        help="TMX filename prefix; inferred from the zone name by default",
    )
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    try:
        root = find_repository_root(Path.cwd().resolve())
    except RuntimeError as exc:
        print(f"build-zone-assets: error: {exc}", file=sys.stderr)
        return 1

    zone = args.zone.strip().upper()
    zone_dir = root / "cd" / "LVL" / zone
    tex_dir = zone_dir / "TEX"
    map_dir = zone_dir / "MAP"
    mod_dir = zone_dir / "MOD"

    if not tex_dir.is_dir():
        print(f"build-zone-assets: error: texture directory not found: {tex_dir}",
              file=sys.stderr)
        return 1

    image_python = root / "image-tools" / "bin" / "python"
    python = str(image_python) if image_python.is_file() else sys.executable

    png_script = root / "tools" / "png_to_keyed_tga.py"
    if not png_script.is_file():
        print(f"build-zone-assets: error: script not found: {png_script}",
              file=sys.stderr)
        return 1
    run(
        [
            python,
            str(png_script),
            str(tex_dir),
            "--output-dir",
            str(tex_dir),
            "--overwrite",
        ],
        args.dry_run,
    )

    tiled_script = root / "tools" / "tiled_to_jo_map.py"
    maps_dir = root / "projFiles" / "maps"
    prefix = (args.map_prefix or infer_map_prefix(zone)).lower()
    maps = sorted(
        path for path in maps_dir.glob("*.tmx")
        if path.stem.lower().startswith(prefix)
    )
    if not maps:
        print(
            f"build-zone-assets: error: no TMX map matching {prefix!r} "
            f"was found in {maps_dir}",
            file=sys.stderr,
        )
        return 1

    for map_path in maps:
        command = [
            python,
            str(tiled_script),
            str(map_path),
            "--output-dir",
            str(map_dir),
            "--mod-output-dir",
            str(mod_dir),
        ]
        mapping = root / "tools" / f"{map_path.stem}_image_names.json"
        if mapping.is_file():
            command.extend(["--image-names", str(mapping)])
        run(command, args.dry_run)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
