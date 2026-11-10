"""CMake update-mode guards only: no firmware build or device access."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UpdateModeTests(unittest.TestCase):
    def run_config(self, **overrides):
        config = {"SINGLE_APP_USB_ENABLE": "ON", "PARTITION_TABLE_CUSTOM": "ON",
                  "PARTITION_TABLE_CUSTOM_FILENAME": "partitions.single.csv",
                  "OTA_SERVICE_ENABLE": "OFF", "BOOTLOADER_APP_ROLLBACK_ENABLE": "OFF",
                  "ESP_HTTPS_SERVER_ENABLE": "OFF"}
        config.update(overrides)
        return subprocess.run(["cmake", *[f"-DCONFIG_{k}={v}" for k, v in config.items()],
                               "-P", str(ROOT / "cmake/update_mode.cmake")],
                              capture_output=True, text=True, timeout=10)

    def test_single_usb_allowed(self):
        result = self.run_config()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_network_ota_rejected(self):
        self.assertNotEqual(self.run_config(OTA_SERVICE_ENABLE="ON").returncode, 0)

    def test_rollback_rejected(self):
        self.assertNotEqual(self.run_config(BOOTLOADER_APP_ROLLBACK_ENABLE="ON").returncode, 0)

    def test_https_server_rejected(self):
        self.assertNotEqual(self.run_config(ESP_HTTPS_SERVER_ENABLE="ON").returncode, 0)

    def test_wrong_layout_rejected(self):
        self.assertNotEqual(self.run_config(PARTITION_TABLE_CUSTOM_FILENAME="partitions.ota.csv").returncode, 0)

    def test_stock_layout_rejected(self):
        self.assertNotEqual(self.run_config(PARTITION_TABLE_CUSTOM="OFF").returncode, 0)

    def test_single_table_requires_usb_mode(self):
        self.assertNotEqual(self.run_config(SINGLE_APP_USB_ENABLE="OFF").returncode, 0)

    def test_legacy_dual_slot_retained(self):
        result = self.run_config(SINGLE_APP_USB_ENABLE="OFF",
                                 PARTITION_TABLE_CUSTOM_FILENAME="partitions.ota.csv",
                                 OTA_SERVICE_ENABLE="ON", BOOTLOADER_APP_ROLLBACK_ENABLE="ON",
                                 ESP_HTTPS_SERVER_ENABLE="ON")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_security_migration_rejected(self):
        for option in ("SECURE_BOOT", "SECURE_FLASH_ENC_ENABLED", "BOOTLOADER_APP_ANTI_ROLLBACK"):
            with self.subTest(option=option):
                self.assertNotEqual(self.run_config(**{option: "ON"}).returncode, 0)


if __name__ == "__main__":
    unittest.main()
