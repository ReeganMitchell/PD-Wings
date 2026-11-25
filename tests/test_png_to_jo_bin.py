from contextlib import redirect_stdout
from io import StringIO
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from tools.png_to_jo_bin import ConversionError, convert_image, main


class PngToJoBinTests(unittest.TestCase):
    def test_writes_little_endian_dimensions_and_saturn_rgb555_pixels(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "input.png"
            output = root / "output.BIN"
            image = Image.new("RGBA", (2, 1))
            image.putdata([(200, 30, 60, 0), (255, 241, 232, 255)])
            image.save(source)

            convert_image(source, output)

            self.assertEqual(output.read_bytes(), b"\x02\x00\x01\x00\x00\x00\xdf\xf7")

    def test_respects_alpha_threshold(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "input.png"
            output = root / "output.BIN"
            image = Image.new("RGBA", (2, 1))
            image.putdata([(255, 0, 0, 127), (0, 255, 0, 128)])
            image.save(source)

            convert_image(source, output, alpha_threshold=127)

            self.assertEqual(output.read_bytes(), b"\x02\x00\x01\x00\x00\x00\xe0\x83")

    def test_refuses_to_overwrite_unless_requested(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "input.png"
            output = root / "output.BIN"
            Image.new("RGBA", (1, 1), (10, 20, 30, 255)).save(source)
            output.write_bytes(b"existing")

            with self.assertRaisesRegex(ConversionError, "refusing to overwrite"):
                convert_image(source, output)

    def test_converts_pngs_in_directory_to_output_directory(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source_dir = root / "sources"
            output_dir = root / "converted"
            source_dir.mkdir()
            Image.new("RGBA", (1, 1), (255, 0, 0, 255)).save(source_dir / "one.png")
            Image.new("RGBA", (1, 1), (0, 255, 0, 255)).save(source_dir / "two.PNG")

            with redirect_stdout(StringIO()):
                result = main(
                    [str(source_dir), "--output-dir", str(output_dir)]
                )

            self.assertEqual(result, 0)
            self.assertEqual(
                sorted(path.name for path in output_dir.iterdir()),
                ["one.BIN", "two.BIN"],
            )


if __name__ == "__main__":
    unittest.main()
