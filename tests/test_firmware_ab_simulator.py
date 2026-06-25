import hashlib
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from firmware_ab_simulator import Device, PowerLoss


class FirmwareABTests(unittest.TestCase):
    def test_successful_trial_is_confirmed(self):
        device = Device()
        image = b"version-two"
        device.install(image, "2.0.0", hashlib.sha256(image).hexdigest())
        selected = device.select_boot_slot()
        self.assertEqual(selected, "B")
        device.confirm(selected)
        self.assertEqual(device.meta.active, "B")
        self.assertIsNone(device.meta.pending)

    def test_power_loss_keeps_known_good_active(self):
        device = Device()
        image = b"version-two-long-enough-for-several-chunks"
        with self.assertRaises(PowerLoss):
            device.install(
                image,
                "2.0.0",
                hashlib.sha256(image).hexdigest(),
                chunk_size=8,
                fail_after_chunk=2,
            )
        self.assertEqual(device.select_boot_slot(), "A")
        self.assertFalse(device.slots["B"].valid)

    def test_failed_trial_rolls_back(self):
        device = Device()
        image = b"version-two"
        device.install(image, "2.0.0", hashlib.sha256(image).hexdigest())
        selected = device.select_boot_slot()
        device.report_boot_failure(selected)
        self.assertEqual(device.select_boot_slot(), "A")


if __name__ == "__main__":
    unittest.main()
