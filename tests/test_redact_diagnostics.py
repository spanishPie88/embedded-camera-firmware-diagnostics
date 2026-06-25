import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from redact_diagnostics import redact_text


class RedactionTests(unittest.TestCase):
    def test_common_identifiers_are_masked(self):
        source = (
            "user@example.com 192.168.1.20 aa:bb:cc:dd:ee:ff "
            "/home/alice/log C:\\Users\\alice\\capture\n"
            "iSerial 3 SECRET123\n"
        )
        redacted, counts = redact_text(source)
        self.assertNotIn("user@example.com", redacted)
        self.assertNotIn("192.168.1.20", redacted)
        self.assertNotIn("SECRET123", redacted)
        self.assertGreaterEqual(sum(counts.values()), 6)


if __name__ == "__main__":
    unittest.main()
