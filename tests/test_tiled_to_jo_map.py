import tempfile
import unittest
from pathlib import Path

from tools.tiled_to_jo_map import ExportError, _output_filename, export_map


class TiledToJoMapTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.tileset = self.root / "tiles.tsx"
        self.map_path = self.root / "level.tmx"
        self.tileset.write_text(
            """<tileset name="tiles" tilewidth="32" tileheight="32" tilecount="2" columns="0">
  <tile id="0"><image source="one.png"/></tile>
  <tile id="1">
    <properties><property name="joengineName" value="TWO.TGA"/></properties>
    <image source="two.png"/>
  </tile>
</tileset>""",
            encoding="utf-8",
        )

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_outputs_graphics_and_collision_names_from_layer_names(self):
        self.assertEqual(_output_filename("Z1A1", "Graphics", 1), "Z1A1G.MAP")
        self.assertEqual(_output_filename("Z1A1", "Group/Collision", 2), "Z1A1C.MAP")
        self.assertEqual(_output_filename("Z1A1", "Decorations", 3), "Z1A1_03.MAP")

    def test_exports_each_nonempty_layer_with_tileset_filenames_and_offsets(self):
        self.map_path.write_text(
            """<map orientation="orthogonal" width="2" height="2" tilewidth="32" tileheight="32" infinite="1">
  <tileset firstgid="1" source="tiles.tsx"/>
  <group name="Art" offsetx="32" offsety="0">
    <layer name="Back" x="1">
      <data encoding="csv"><chunk x="-1" y="2" width="2" height="2">
0,1,
2,0
      </chunk></data>
    </layer>
    <layer name="Empty" width="1" height="1">
      <data encoding="csv">0</data>
    </layer>
  </group>
  <layer name="Front" width="1" height="1">
    <data encoding="csv">2</data>
  </layer>
</map>""",
            encoding="utf-8",
        )

        layers = export_map(self.map_path)

        self.assertEqual(len(layers), 2)
        self.assertEqual(layers[0].name, "Art/Back")
        self.assertEqual(
            layers[0].entries,
            ["ONE.TGA\t64\t64", "TWO.TGA\t32\t96"],
        )
        self.assertEqual(layers[1].name, "Front")
        self.assertEqual(layers[1].entries, ["TWO.TGA\t0\t0"])

    def test_rejects_tileset_atlas(self):
        self.tileset.write_text(
            """<tileset name="atlas" tilewidth="32" tileheight="32" tilecount="1" columns="1">
  <image source="atlas.png" width="32" height="32"/>
</tileset>""",
            encoding="utf-8",
        )
        self.map_path.write_text(
            """<map orientation="orthogonal" width="1" height="1" tilewidth="32" tileheight="32" infinite="0">
  <tileset firstgid="1" source="tiles.tsx"/>
  <layer name="Art" width="1" height="1"><data encoding="csv">1</data></layer>
</map>""",
            encoding="utf-8",
        )

        with self.assertRaisesRegex(ExportError, "image-collection tileset"):
            export_map(self.map_path)

    def test_rejects_unmapped_long_runtime_filename(self):
        self.tileset.write_text(
            """<tileset name="tiles" tilewidth="32" tileheight="32" tilecount="1" columns="0">
  <tile id="0"><image source="underground.png"/></tile>
</tileset>""",
            encoding="utf-8",
        )
        self.map_path.write_text(
            """<map orientation="orthogonal" width="1" height="1" tilewidth="32" tileheight="32" infinite="0">
  <tileset firstgid="1" source="tiles.tsx"/>
  <layer name="Art" width="1" height="1"><data encoding="csv">1</data></layer>
</map>""",
            encoding="utf-8",
        )

        with self.assertRaisesRegex(ExportError, "joengineName"):
            export_map(self.map_path)

    def test_accepts_runtime_filename_mapping(self):
        self.tileset.write_text(
            """<tileset name="tiles" tilewidth="32" tileheight="32" tilecount="1" columns="0">
  <tile id="0"><image source="underground.png"/></tile>
</tileset>""",
            encoding="utf-8",
        )
        self.map_path.write_text(
            """<map orientation="orthogonal" width="1" height="1" tilewidth="32" tileheight="32" infinite="0">
  <tileset firstgid="1" source="tiles.tsx"/>
  <layer name="Art" width="1" height="1"><data encoding="csv">1</data></layer>
</map>""",
            encoding="utf-8",
        )

        layers = export_map(
            self.map_path,
            image_names={"underground.png": "underg.tga"},
        )

        self.assertEqual(layers[0].entries, ["UNDERG.TGA\t0\t0"])

    def test_exports_joengine_tile_attributes(self):
        self.tileset.write_text(
            """<tileset name="tiles" tilewidth="32" tileheight="32" tilecount="1" columns="0">
  <tile id="0">
    <properties><property name="joengineAttribute" value="2"/></properties>
    <image source="slope.png"/>
  </tile>
</tileset>""",
            encoding="utf-8",
        )
        self.map_path.write_text(
            """<map orientation="orthogonal" width="1" height="1" tilewidth="32" tileheight="32" infinite="0">
  <tileset firstgid="1" source="tiles.tsx"/>
  <layer name="Collision" width="1" height="1"><data encoding="csv">1</data></layer>
</map>""",
            encoding="utf-8",
        )

        layers = export_map(self.map_path)

        self.assertEqual(layers[0].entries, ["SLOPE.TGA\t0\t0\t2"])


if __name__ == "__main__":
    unittest.main()
