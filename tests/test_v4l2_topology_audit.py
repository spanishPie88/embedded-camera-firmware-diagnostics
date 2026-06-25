import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from v4l2_topology_audit import audit


class TopologyTests(unittest.TestCase):
    def test_good_pipeline_has_no_warning(self):
        text = (ROOT / "examples" / "media-ctl-rkisp-good.txt").read_text()
        result = audit(text)
        self.assertEqual(result["enabled_link_count"], 3)
        self.assertEqual(result["warnings"], [])

    def test_broken_pipeline_is_reported(self):
        text = (ROOT / "examples" / "media-ctl-rkisp-broken.txt").read_text()
        result = audit(text)
        self.assertTrue(
            any("no enabled outgoing link" in warning for warning in result["warnings"])
        )
        self.assertTrue(
            any("format mismatch" in warning for warning in result["warnings"])
        )


if __name__ == "__main__":
    unittest.main()
