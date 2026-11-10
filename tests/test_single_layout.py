"""Offline single-app layout checks using the pinned IDF generator."""
import unittest
from test_ota_layout import GEN, ROOT


class SingleLayoutTests(unittest.TestCase):
    def setUp(self):
        self.table = GEN.PartitionTable.from_csv((ROOT / "partitions.single.csv").read_text())

    def test_official_validation_and_capacity(self):
        self.table.verify()
        apps = [p for p in self.table if p.type == 0]
        self.assertEqual([(p.name, p.subtype, p.offset, p.size) for p in apps],
                         [("factory", 0, 0x10000, 0x2a0000)])
        self.assertEqual(apps[0].offset + apps[0].size, 0x2b0000)
        self.assertEqual(max(p.offset + p.size for p in self.table), 0x400000)
        self.assertEqual(len(self.table), 7)

    def test_preserves_other_data_ranges(self):
        old = GEN.PartitionTable.from_csv((ROOT / "partitions.ota.csv").read_text())
        for name in ("nvs", "free", "flash", "js_code", "storage"):
            before, after = old.find_by_name(name), self.table.find_by_name(name)
            self.assertEqual((after.type, after.subtype, after.offset, after.size),
                             (before.type, before.subtype, before.offset, before.size))

    def test_reserves_old_control_without_ota_metadata(self):
        reserved = self.table.find_by_name("reserved")
        self.assertEqual((reserved.type, reserved.subtype, reserved.offset, reserved.size),
                         (1, 0x40, 0xc000, 0x2000))
        self.assertFalse(any(p.type == 1 and p.subtype == 0 for p in self.table))
        self.assertFalse(any(p.type == 0 and 0x10 <= p.subtype <= 0x1f for p in self.table))

    def test_md5_binary_roundtrip(self):
        decoded = GEN.PartitionTable.from_binary(self.table.to_binary())
        self.assertEqual([(p.name, p.type, p.subtype, p.offset, p.size) for p in decoded],
                         [(p.name, p.type, p.subtype, p.offset, p.size) for p in self.table])


if __name__ == "__main__":
    unittest.main()
