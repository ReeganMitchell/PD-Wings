---
name: build-zone-assets
description: "Build all PD-Wings assets for a zone. Use when asked to build assets, convert textures, or export TMX/MODMAP files for ZONE1 or another zone folder."
argument-hint: "ZONE1"
user-invocable: true
disable-model-invocation: false
---

# Build Zone Assets

Use this skill when the user asks to build assets for a zone, convert a zone's PNG textures, or export its Tiled maps and 3D placements.

## Procedure

1. Parse the zone name from the request, for example `ZONE1`.
2. Run the bundled script from the repository root:

   ```bash
   python3 .github/skills/build-zone-assets/scripts/build_zone_assets.py ZONE1
   ```

3. The script:
   - Converts every PNG in `cd/LVL/<ZONE>/TEX/` to keyed TGA files in the same directory.
   - Finds matching TMX maps under `projFiles/maps/`.
   - Exports each map's tile MAP files to `cd/LVL/<ZONE>/MAP/`.
   - Exports object placements to `cd/LVL/<ZONE>/MOD/<MAP_PREFIX>.MODMAP`.
   - Uses `tools/<tmx_stem>_image_names.json` when that mapping file exists.
4. Report each generated texture, MAP, and MODMAP file.

For a preview without changing files, add `--dry-run`. Use `--overwrite` explicitly only when transparent or existing TGA output must be regenerated; the zone runner enables it by default.
