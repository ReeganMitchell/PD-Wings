import tempfile
import unittest
from pathlib import Path

from PIL import Image

from tools.png_to_keyed_tga import ConversionError, convert_image


class PngToKeyedTgaTests(unittest.TestCase):
    def test_converts_transparent_pixels_to_key_color_in_24_bit_tga(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "input.png"
            output = root / "output.TGA"
            image = Image.new("RGBA", (2, 1))
            image.putdata([(20, 40, 60, 0), (70, 80, 90, 255)])
            image.save(source)

            convert_image(source, output)

            with Image.open(output) as converted:
                self.assertEqual(converted.format, "TGA")
                self.assertEqual(converted.mode, "RGB")
                self.assertEqual(converted.getpixel((0, 0)), (0, 255, 0))
                self.assertEqual(converted.getpixel((1, 0)), (70, 80, 90))

    def test_respects_alpha_threshold(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "input.png"
            output = root / "output.TGA"
            image = Image.new("RGBA", (2, 1))
            image.putdata([(1, 2, 3, 127), (4, 5, 6, 128)])
            image.save(source)

            convert_image(source, output, alpha_threshold=127)

            with Image.open(output) as converted:
                self.assertEqual(converted.getpixel((0, 0)), (0, 255, 0))
                self.assertEqual(converted.getpixel((1, 0)), (4, 5, 6))

    def test_refuses_to_overwrite_unless_requested(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            source = root / "input.png"
            output = root / "output.TGA"
            Image.new("RGBA", (1, 1), (10, 20, 30, 255)).save(source)
            output.write_bytes(b"existing")

            with self.assertRaisesRegex(ConversionError, "refusing to overwrite"):
                convert_image(source, output)


if __name__ == "__main__":
    unittest.main()
