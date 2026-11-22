#!/usr/bin/env python3
"""Export Tiled TMX tile layers as Jo Engine MAP sprite-placement files."""

from __future__ import annotations

import argparse
import gzip
import json
import re
import sys
import xml.etree.ElementTree as ET
import zlib
from dataclasses import dataclass
from pathlib import Path


GID_MASK = 0x0FFFFFFF
JO_MAX_FILENAME_LENGTH = 12
JO_MAX_BASENAME_LENGTH = 8
JO_MAX_EXTENSION_LENGTH = 3


class ExportError(Exception):
    """An input map cannot be represented by Jo Engine MAP files."""


@dataclass(frozen=True)
class Tileset:
    first_gid: int
    end_gid: int
    source_path: Path
    images: dict[int, str]
    properties: dict[int, dict[str, str]]
    has_atlas_image: bool


@dataclass(frozen=True)
class TileLayer:
    name: str
    entries: list[str]


def _jo_filename(source: str, extension: str, override: str | None) -> str:
    if override:
        filename = override.strip().upper()
    else:
        filename = f"{Path(source).stem}{extension}".upper()

    if "/" in filename or "\\" in filename or filename.count(".") != 1:
        raise ExportError(
            f"invalid Jo Engine filename {filename!r}; use an 8.3 filename"
        )
    basename, file_extension = filename.rsplit(".", 1)
    if (
        not basename
        or len(basename) > JO_MAX_BASENAME_LENGTH
        or not file_extension
        or len(file_extension) > JO_MAX_EXTENSION_LENGTH
        or len(filename) > JO_MAX_FILENAME_LENGTH
        or not re.fullmatch(r"[A-Z0-9_]+", basename)
        or not re.fullmatch(r"[A-Z0-9]+", file_extension)
    ):
        raise ExportError(
            f"{filename!r} is not a valid Jo Engine 8.3 filename; "
            "set a shorter 'joengineName' property on the Tiled tile"
        )
    return filename


def _tile_properties(tile: ET.Element) -> dict[str, str]:
    properties = tile.find("properties")
    if properties is None:
        return {}
    return {
        prop.attrib["name"]: prop.attrib.get("value", prop.text or "")
        for prop in properties.findall("property")
        if "name" in prop.attrib
    }


def _load_tileset(map_path: Path, element: ET.Element) -> Tileset:
    first_gid = int(element.attrib["firstgid"])
    source = element.attrib.get("source")
    if source:
        tileset_path = (map_path.parent / source).resolve()
        try:
            root = ET.parse(tileset_path).getroot()
        except (OSError, ET.ParseError) as exc:
            raise ExportError(f"cannot read tileset {tileset_path}: {exc}") from exc
    else:
        tileset_path = map_path
        root = element

    images: dict[int, str] = {}
    properties: dict[int, dict[str, str]] = {}
    for tile in root.findall("tile"):
        tile_id = int(tile.attrib["id"])
        properties[tile_id] = _tile_properties(tile)
        image = tile.find("image")
        if image is not None and image.attrib.get("source"):
            images[tile_id] = image.attrib["source"]

    atlas_image = root.find("image") is not None
    tile_count = int(root.attrib.get("tilecount", "0"))
    if tile_count <= 0 and images:
        tile_count = max(images) + 1
    return Tileset(
        first_gid=first_gid,
        end_gid=first_gid + tile_count,
        source_path=tileset_path,
        images=images,
        properties=properties,
        has_atlas_image=atlas_image,
    )


def _decode_csv(text: str, expected_count: int, context: str) -> list[int]:
    try:
        values = [int(value.strip()) for value in text.split(",") if value.strip()]
    except ValueError as exc:
        raise ExportError(f"{context}: invalid CSV tile data") from exc
    if len(values) != expected_count:
        raise ExportError(
            f"{context}: expected {expected_count} tile values, found {len(values)}"
        )
    return values


def _layer_values(
    layer: ET.Element, map_width: int, map_height: int, context: str
) -> list[tuple[int, int, int]]:
    data = layer.find("data")
    if data is None:
        raise ExportError(f"{context}: tile layer has no data")
    encoding = data.attrib.get("encoding", "")
    compression = data.attrib.get("compression")
    if encoding not in ("", "csv", "base64"):
        raise ExportError(
            f"{context}: unsupported tile data encoding {encoding!r}"
        )
    if compression not in (None, "", "zlib", "gzip"):
        raise ExportError(
            f"{context}: unsupported tile data compression {compression!r}"
        )

    values: list[tuple[int, int, int]] = []
    chunks = data.findall("chunk")
    if chunks:
        for chunk in chunks:
            width = int(chunk.attrib["width"])
            height = int(chunk.attrib["height"])
            x = int(chunk.attrib["x"])
            y = int(chunk.attrib["y"])
            if encoding == "csv":
                gids = _decode_csv(
                    chunk.text or "", width * height, f"{context} chunk ({x}, {y})"
                )
            elif encoding == "base64":
                gids = _decode_base64(
                    chunk.text or "",
                    width * height,
                    compression,
                    f"{context} chunk ({x}, {y})",
                )
            else:
                gids = [
                    int(tile.attrib["gid"])
                    for tile in chunk.findall("tile")
                ]
                if len(gids) != width * height:
                    raise ExportError(
                        f"{context} chunk ({x}, {y}): "
                        f"expected {width * height} tile values, found {len(gids)}"
                    )
            values.extend(
                (x + col, y + row, gids[row * width + col])
                for row in range(height)
                for col in range(width)
            )
        return values

    width = int(layer.attrib.get("width", map_width))
    height = int(layer.attrib.get("height", map_height))
    if encoding == "csv":
        gids = _decode_csv(data.text or "", width * height, context)
    elif encoding == "base64":
        gids = _decode_base64(data.text or "", width * height, compression, context)
    else:
        gids = [int(tile.attrib["gid"]) for tile in data.findall("tile")]
        if len(gids) != width * height:
            raise ExportError(
                f"{context}: expected {width * height} tile values, found {len(gids)}"
            )
    return [
        (col, row, gids[row * width + col])
        for row in range(height)
        for col in range(width)
    ]


def _decode_base64(
    text: str, expected_count: int, compression: str | None, context: str
) -> list[int]:
    import base64

    try:
        raw = base64.b64decode("".join(text.split()), validate=True)
        if compression == "zlib":
            raw = zlib.decompress(raw)
        elif compression == "gzip":
            raw = gzip.decompress(raw)
    except (ValueError, zlib.error, OSError) as exc:
        raise ExportError(f"{context}: invalid base64 tile data") from exc
    expected_bytes = expected_count * 4
    if len(raw) != expected_bytes:
        raise ExportError(
            f"{context}: expected {expected_bytes} bytes of tile data, "
            f"found {len(raw)}"
        )
    return [
        int.from_bytes(raw[offset : offset + 4], "little")
        for offset in range(0, expected_bytes, 4)
    ]


def _find_tileset(gid: int, tilesets: list[Tileset], context: str) -> tuple[Tileset, int]:
    for tileset in reversed(tilesets):
        tile_id = gid - tileset.first_gid
        if tile_id >= 0 and (tileset.end_gid <= tileset.first_gid or gid < tileset.end_gid):
            return tileset, tile_id
    raise ExportError(f"{context}: GID {gid} does not belong to a listed tileset")


def _export_layer(
    layer: ET.Element,
    layer_name: str,
    parent_tile_x: int,
    parent_tile_y: int,
    parent_pixel_x: float,
    parent_pixel_y: float,
    origin_x: int,
    origin_y: int,
    tile_width: int,
    tile_height: int,
    map_width: int,
    map_height: int,
    tilesets: list[Tileset],
    image_extension: str,
    image_names: dict[str, str],
) -> TileLayer:
    context = f"layer {layer_name!r}"
    tile_x = parent_tile_x + int(layer.attrib.get("x", "0"))
    tile_y = parent_tile_y + int(layer.attrib.get("y", "0"))
    pixel_x = parent_pixel_x + float(layer.attrib.get("offsetx", "0"))
    pixel_y = parent_pixel_y + float(layer.attrib.get("offsety", "0"))
    if not pixel_x.is_integer() or not pixel_y.is_integer():
        raise ExportError(f"{context}: pixel offsets must be whole numbers")

    entries: list[str] = []
    for cell_x, cell_y, raw_gid in _layer_values(
        layer, map_width, map_height, context
    ):
        if raw_gid == 0:
            continue
        gid = raw_gid & GID_MASK
        if gid == 0:
            continue
        if gid != raw_gid:
            raise ExportError(
                f"{context}: flipped/rotated tiles are not supported by Jo MAP files"
            )

        tileset, tile_id = _find_tileset(gid, tilesets, context)
        source = tileset.images.get(tile_id)
        if source is None:
            if tileset.has_atlas_image:
                raise ExportError(
                    f"{context}: tileset {tileset.source_path.name} uses a single "
                    "atlas image. Jo MAP files require one image per tile; use "
                    "an image-collection tileset."
                )
            raise ExportError(
                f"{context}: tile ID {tile_id} in {tileset.source_path.name} "
                "has no per-tile image"
            )

        filename = _jo_filename(
            source,
            image_extension,
            tileset.properties.get(tile_id, {}).get("joengineName")
            or image_names.get(Path(source).name.lower()),
        )
        attribute_value = tileset.properties.get(tile_id, {}).get(
            "joengineAttribute"
        )
        attribute = ""
        if attribute_value:
            try:
                parsed_attribute = int(attribute_value)
            except ValueError as exc:
                raise ExportError(
                    f"{context}: joengineAttribute must be an integer from 0 to 255"
                ) from exc
            if not 0 <= parsed_attribute <= 255:
                raise ExportError(
                    f"{context}: joengineAttribute must be an integer from 0 to 255"
                )
            attribute = f"\t{parsed_attribute}"
        x = (cell_x + tile_x - origin_x) * tile_width + int(pixel_x)
        y = (cell_y + tile_y - origin_y) * tile_height + int(pixel_y)
        if not (-32768 <= x <= 32767 and -32768 <= y <= 32767):
            raise ExportError(f"{context}: tile coordinate ({x}, {y}) is out of range")
        entries.append(f"{filename}\t{x}\t{y}{attribute}")
    return TileLayer(layer_name, entries)


def export_map(
    map_path: Path,
    image_extension: str = ".TGA",
    image_names: dict[str, str] | None = None,
) -> list[TileLayer]:
    try:
        root = ET.parse(map_path).getroot()
    except (OSError, ET.ParseError) as exc:
        raise ExportError(f"cannot read map {map_path}: {exc}") from exc
    if root.tag != "map":
        raise ExportError("input file is not a Tiled TMX map")
    if root.attrib.get("orientation") != "orthogonal":
        raise ExportError("only orthogonal Tiled maps are supported")

    tile_width = int(root.attrib["tilewidth"])
    tile_height = int(root.attrib["tileheight"])
    map_width = int(root.attrib["width"])
    map_height = int(root.attrib["height"])
    if tile_width <= 0 or tile_height <= 0:
        raise ExportError("map tile dimensions must be positive")

    tilesets = sorted(
        (_load_tileset(map_path, item) for item in root.findall("tileset")),
        key=lambda tileset: tileset.first_gid,
    )
    if not tilesets:
        raise ExportError("map has no tilesets")
    for index, tileset in enumerate(tilesets[:-1]):
        if tileset.end_gid <= tileset.first_gid:
            tilesets[index] = Tileset(
                first_gid=tileset.first_gid,
                end_gid=tilesets[index + 1].first_gid,
                source_path=tileset.source_path,
                images=tileset.images,
                properties=tileset.properties,
                has_atlas_image=tileset.has_atlas_image,
            )

    layer_specs: list[
        tuple[ET.Element, str, int, int, float, float]
    ] = []

    def visit(
        parent: ET.Element,
        group_name: str,
        parent_tile_x: int,
        parent_tile_y: int,
        parent_pixel_x: float,
        parent_pixel_y: float,
    ) -> None:
        for child in parent:
            if child.tag == "layer":
                name = "/".join(filter(None, (group_name, child.attrib.get("name", "Layer"))))
                layer_specs.append(
                    (
                        child,
                        name,
                        parent_tile_x,
                        parent_tile_y,
                        parent_pixel_x,
                        parent_pixel_y,
                    )
                )
            elif child.tag == "group":
                name = "/".join(filter(None, (group_name, child.attrib.get("name", "Group"))))
                visit(
                    child,
                    name,
                    parent_tile_x + int(child.attrib.get("x", "0")),
                    parent_tile_y + int(child.attrib.get("y", "0")),
                    parent_pixel_x + float(child.attrib.get("offsetx", "0")),
                    parent_pixel_y + float(child.attrib.get("offsety", "0")),
                )

    visit(root, "", 0, 0, 0.0, 0.0)
    if not layer_specs:
        raise ExportError("map has no tile layers")
    layer_specs = [
        spec
        for spec in layer_specs
        if any(
            gid & GID_MASK
            for _, _, gid in _layer_values(
                spec[0], map_width, map_height, f"layer {spec[1]!r}"
            )
        )
    ]
    if not layer_specs:
        raise ExportError("map has no non-empty tile layers")

    origins: list[tuple[int, int]] = []
    for layer, _, parent_x, parent_y, _, _ in layer_specs:
        layer_x = parent_x + int(layer.attrib.get("x", "0"))
        layer_y = parent_y + int(layer.attrib.get("y", "0"))
        data = layer.find("data")
        chunks = data.findall("chunk") if data is not None else []
        if chunks:
            origins.extend(
                (layer_x + int(chunk.attrib["x"]), layer_y + int(chunk.attrib["y"]))
                for chunk in chunks
            )
        else:
            origins.append((layer_x, layer_y))
    origin_x = min(x for x, _ in origins)
    origin_y = min(y for _, y in origins)

    result = [
        _export_layer(
            layer,
            name,
            parent_x,
            parent_y,
            parent_pixel_x,
            parent_pixel_y,
            origin_x,
            origin_y,
            tile_width,
            tile_height,
            map_width,
            map_height,
            tilesets,
            image_extension,
            image_names or {},
        )
        for (
            layer,
            name,
            parent_x,
            parent_y,
            parent_pixel_x,
            parent_pixel_y,
        ) in layer_specs
    ]
    return [layer for layer in result if layer.entries]


def _output_prefix(value: str) -> str:
    prefix = re.sub(r"[^A-Za-z0-9_]", "", value).upper()
    if not prefix or len(prefix) > 4:
        raise ExportError("output prefix must be 1 to 4 letters, digits, or underscores")
    return prefix


def _output_filename(prefix: str, layer_name: str, index: int) -> str:
    leaf_name = layer_name.rsplit("/", 1)[-1].strip().lower()
    suffix = {"graphics": "G", "collision": "C"}.get(leaf_name)
    if suffix is not None:
        return f"{prefix}{suffix}.MAP"
    return f"{prefix}_{index:02}.MAP"


def _read_image_names(path: Path | None) -> dict[str, str]:
    if path is None:
        return {}
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ExportError(f"cannot read image-name mapping {path}: {exc}") from exc
    if not isinstance(value, dict) or any(
        not isinstance(key, str) or not isinstance(name, str)
        for key, name in value.items()
    ):
        raise ExportError("image-name mapping must be a JSON object of strings")
    return {key.lower(): name for key, name in value.items()}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Export Tiled TMX tile layers as Jo Engine MAP files."
    )
    parser.add_argument("map", type=Path, help="source .tmx map")
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=None,
        help="destination directory (defaults to the map's directory)",
    )
    parser.add_argument(
        "--prefix",
        default=None,
        help="up to four-character MAP filename prefix (defaults to map filename)",
    )
    parser.add_argument(
        "--image-extension",
        default=".TGA",
        help="runtime sprite extension to use (default: .TGA)",
    )
    parser.add_argument(
        "--image-names",
        type=Path,
        default=None,
        help="JSON mapping from tileset image basenames to runtime filenames",
    )
    args = parser.parse_args(argv)

    extension = args.image_extension.upper()
    if extension.startswith("."):
        extension = extension[1:]
    if not re.fullmatch(r"[A-Z0-9]{1,3}", extension):
        parser.error("--image-extension must be 1 to 3 letters/digits")

    try:
        prefix = _output_prefix(args.prefix or args.map.stem[:4])
        layers = export_map(
            args.map,
            f".{extension}",
            _read_image_names(args.image_names),
        )
        output_dir = args.output_dir or args.map.parent
        output_dir.mkdir(parents=True, exist_ok=True)
        for index, layer in enumerate(layers, start=1):
            output_path = output_dir / _output_filename(prefix, layer.name, index)
            output_path.write_text(
                "".join(f"{entry}\n" for entry in layer.entries),
                encoding="ascii",
                newline="\n",
            )
            print(f"{output_path}: {layer.name} ({len(layer.entries)} tiles)")
    except (ExportError, OSError, ValueError, KeyError) as exc:
        print(f"tiled_to_jo_map: error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
