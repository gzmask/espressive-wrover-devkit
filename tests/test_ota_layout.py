"""Uses the pinned IDF generator; no serial/network/Flash operations."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "gen_esp32part", ROOT / ".local/esp-idf/components/partition_table/gen_esp32part.py")
GEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GEN)


class OtaLayoutTests(unittest.TestCase):
    def setUp(self):
        self.table = GEN.PartitionTable.from_csv((ROOT / "partitions.ota.csv").read_text())

    def test_official_validation(self):
        self.table.verify()
        self.assertEqual(max(p.offset + p.size for p in self.table), 0x400000)

    def test_preserves_data_ranges(self):
        expected = {"nvs": (0x9000, 0x3000), "otadata": (0xc000, 0x2000),
                    "free": (0xe000, 0x2000), "flash": (0x2b0000, 0x10000),
                    "js_code": (0x2c0000, 0x100000), "storage": (0x3c0000, 0x40000)}
        for name, position in expected.items():
            p = self.table.find_by_name(name)
            self.assertEqual((p.offset, p.size), position)

    def test_two_equal_ota_slots_no_factory(self):
        apps = [p for p in self.table if p.type == 0]
        self.assertEqual([(p.subtype, p.offset, p.size) for p in apps],
                         [(0x10, 0x10000, 0x150000), (0x11, 0x160000, 0x150000)])
        self.assertEqual(len(self.table), 8)

    def test_unique_otadata_and_md5_roundtrip(self):
        data = [p for p in self.table if p.type == 1 and p.subtype == 0]
        self.assertEqual(len(data), 1)
        decoded = GEN.PartitionTable.from_binary(self.table.to_binary())
        self.assertEqual([(p.name, p.type, p.subtype, p.offset, p.size) for p in decoded],
                         [(p.name, p.type, p.subtype, p.offset, p.size) for p in self.table])


if __name__ == "__main__":
    unittest.main()
