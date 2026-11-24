# Tiled to Jo Engine MAP exporter

Export the tile layers in a Tiled TMX map as separate Jo Engine `.MAP` files:

```sh
python3 tools/tiled_to_jo_map.py projFiles/maps/z1a1.tmx \
  --output-dir cd/LVL/ZONE1/MAP \
  --image-names tools/z1a1_image_names.json
```

By default, output names use the first four characters of the TMX filename and
a layer name:

- A layer named `Graphics` exports to `Z1A1G.MAP`.
- A layer named `Collision` exports to `Z1A1C.MAP`.

Other layer names retain the two-digit fallback, such as `Z1A1_03.MAP`.
Empty tile layers are skipped. The exporter prints the source layer name and
non-empty tile count for each output. Use `--prefix` to choose another
one-to-four-character filename prefix.

The exporter reads external TSX tilesets and CSV, XML, or base64 (optionally
zlib/gzip-compressed) tile data. It preserves relative infinite-map chunk
positions, layer grid positions, group offsets, and whole-pixel offsets. The
combined tile origin is shifted to `(0, 0)`, since Jo Engine maps are placed
relative to the level origin. Blank GID 0 cells are omitted. Tilesets must be
image collections with one image per tile; single-image atlas tilesets cannot
be represented by a Jo Engine `.MAP` tile entry.

Jo Engine expects sprite filenames in ISO 8.3 form. The exporter uses each
tileset tile image's basename, uppercases it, and changes its extension to
`.TGA` by default. For converted or shortened runtime filenames, set a string
custom property named `joengineName` on that Tiled tileset tile, or use the
optional JSON image-name mapping (keys are source image basenames). The tile
property takes precedence over the JSON mapping. For example, map
`underground.png` to `UNDERG.TGA`. The referenced sprite images must also be
loaded in the game's texture pack.

Tile custom integer properties named `joengineAttribute` are exported as the
optional fourth MAP field. Z1A1 uses `1` for flat collision, `2` for a
right-rising slope, and `3` for a left-rising slope.

Only orthogonal tile layers are exported. Object/image layers are ignored;
flipped/rotated tiles, fractional offsets, unsupported encodings, and names
that exceed Jo Engine's filename limits are reported as errors.

Point objects in Tiled ObjectLayers are exported as a separate `.MODMAP`
file in the sibling `MOD` directory (or `--mod-output-dir`). The filename is
`<prefix>.MODMAP`, such as `Z1A1.MODMAP`. `House_1` maps to `HOUSE.JOM`;
instances with a numeric or descriptive suffix use the text before `_`.
Set a custom `joengineModel` property on an object to override the JOM
filename. Each line is:

```text
HOUSE.JOM	97	415
```

Object coordinates are normalized against the same infinite-map origin used
for the exported tile MAP files.

Run the exporter tests with:

```sh
python3 -m unittest discover -s tests
```

## Convert transparent PNGs to keyed TGA

Install Pillow into the Python environment you will use to run the converter:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install Pillow
```

Convert every PNG in a directory to a 24-bit TGA with transparent pixels
replaced by green (`#00FF00`):

```sh
image-tools/bin/python tools/png_to_keyed_tga.py cd/LVL/ZONE1/TEX \
  --output-dir cd/LVL/ZONE1/TEX
```

The script preserves the PNG's RGB pixels wherever alpha is greater than zero;
alpha at or below zero becomes the color key. Use `--alpha-threshold 127` to
make pixels with alpha 0 through 127 transparent. TGA is written in 24-bit RGB,
so partial transparency is flattened rather than preserved. The output name is
the input basename plus `.TGA`; existing files are left untouched unless
`--overwrite` is given. Use `--key-color RRGGBB` to choose another key color.
