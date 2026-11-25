---
name: png-to-jo-bin
description: "Convert PNG spritesheets to Jo Engine Saturn BIN files. Use when preparing game character or sprite images as .BIN assets, or when matching a PNG to an existing Jo Engine BIN."
argument-hint: "PNG file or directory"
user-invocable: true
disable-model-invocation: false
---

# Convert PNG Sprites to Jo Engine BIN

Use this skill when a PNG needs to become a Jo Engine `.BIN` sprite image.

## Procedure

1. Confirm the source PNG dimensions and the destination `.BIN` name/location.
   Jo Engine BIN images store a 16-bit little-endian width and height followed
   by one 16-bit Saturn RGB555 pixel per image pixel. Bit 15 marks opaque
   pixels; transparent pixels are zero.
2. Ensure Pillow is installed in the Python environment used to run the tool:

   ```sh
   python3 -m pip install Pillow
   ```

3. Convert a single image, specifying the runtime filename when needed:

   ```sh
   python3 tools/png_to_jo_bin.py "path/to/sprite.png" \
     --output cd/CHAR/REI/IDLE.BIN
   ```

   For all PNGs in a directory, use `--output-dir` instead:

   ```sh
   python3 tools/png_to_jo_bin.py path/to/pngs --output-dir cd/CHAR/REI
   ```

4. Existing files are preserved by default. Add `--overwrite` only when
   replacement is intended. Use `--alpha-threshold 127` to classify alpha
   values 0 through 127 as transparent; the default threshold is 0.
5. Verify the generated BIN header and dimensions, and compare the resulting
   sprite in the game. Jo Engine runtime filenames should follow its ISO 8.3
   naming constraints.

The converter is [png_to_jo_bin.py](../../../tools/png_to_jo_bin.py). Run its
focused tests with:

```sh
python3 -m unittest discover -s tests -p test_png_to_jo_bin.py
```
