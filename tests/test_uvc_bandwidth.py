import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from uvc_bandwidth import calculate


class BandwidthTests(unittest.TestCase):
    def test_1080p30_yuy2_payload(self):
        result = calculate(1920, 1080, 30, "YUY2", "usb2")
        self.assertAlmostEqual(result.raw_mbps, 995.328)
        self.assertGreater(result.utilization_percent, 100)

    def test_mjpeg_uses_compression_ratio(self):
        result = calculate(
            1920,
            1080,
            30,
            "MJPEG",
            "usb2",
            compression_ratio=8,
        )
        self.assertAlmostEqual(result.estimated_video_mbps, 124.416)
        self.assertLess(result.utilization_percent, 100)

    def test_mjpeg_requires_ratio(self):
        with self.assertRaises(ValueError):
            calculate(1280, 720, 30, "MJPEG", "usb2")


if __name__ == "__main__":
    unittest.main()
