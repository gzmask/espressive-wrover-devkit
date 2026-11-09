"""Synthetic fixtures only; never opens hardware or reads real credentials."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
import contextlib
import io
import json
from types import SimpleNamespace
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "provision_wifi.py"
spec = importlib.util.spec_from_file_location("provision_wifi", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ProvisionTests(unittest.TestCase):
    def test_frame(self):
        frame = module.make_frame(b"fixture", b"test-only-secret", 0x123456789ABCDEF0)
        self.assertEqual(struct.unpack("<4sQBB", frame[:14]),
                         (b"WRV1", 0x123456789ABCDEF0, 7, 16))
        self.assertEqual(struct.unpack("<I", frame[-4:])[0], zlib.crc32(frame[:-4]))

    def test_constraints(self):
        for ssid, password in [(b"", b"test-only-secret"), (b"s"*33, b"12345678"),
                               (b"fixture", b"short"), (b"fixture", b"p"*64),
                               (b"bad\x1b", b"12345678"), (b"fixture", b"p\x00pppppp")]:
            with self.assertRaises(ValueError):
                module.make_frame(ssid, password, 1)
        self.assertEqual(len(module.make_frame(b"s"*32, b"a"*64, 1)), 114)

    def test_private_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input"
            path.write_bytes(b"fixture\r\ntest-only-secret\r\n")
            path.chmod(0o600)
            self.assertEqual(module.read_credentials(path), (b"fixture", b"test-only-secret"))
            path.chmod(0o644)
            with self.assertRaises(ValueError):
                module.read_credentials(path)
            path.chmod(0o600)
            path.write_bytes(b"fixture\ntest-only-secret\nextra")
            with self.assertRaises(ValueError):
                module.read_credentials(path)

    def test_challenge(self):
        ready = b"I (4) wifi_station: provision_ready v=1 nonce=123456789abcdef0 firmware=0.1.4-wifi-sta country=US"
        self.assertEqual(module.challenge(ready, "0.1.4-wifi-sta"), 0x123456789ABCDEF0)
        self.assertIsNone(module.challenge(b"status only", "0.1.4-wifi-sta"))
        with self.assertRaises(ValueError):
            module.challenge(ready, "different-firmware")

    def test_redaction(self):
        secret = b"test-only-secret"
        for representation in (secret, secret.hex().encode(), secret.hex().upper().encode()):
            clean, matched = module.sanitize(b"prefix " + representation + b"\r\n", secret)
            self.assertTrue(matched)
            self.assertNotIn(representation, clean)
        self.assertEqual(module.sanitize(b"wifi_status={}\r\n", secret),
                         (b"wifi_status={}\r\n", False))


class Clock:
    def __init__(self):
        self.value = 0.0

    def monotonic(self):
        return self.value

    def sleep(self, seconds):
        self.value += seconds


class FakeSerial:
    def __init__(self, clock, chunks, fail=False):
        self.clock, self.chunks, self.fail = clock, list(chunks), fail
        self.writes = []
        self.closed = False
        self.rts = False
        self.dtr = False

    @property
    def in_waiting(self):
        return len(self.chunks[0]) if self.chunks else 1

    def __setattr__(self, name, value):
        if name in ("rts", "dtr") and getattr(self, "read_failed", False):
            raise OSError("synthetic cleanup failure")
        object.__setattr__(self, name, value)

    def open(self):
        pass

    def reset_input_buffer(self):
        pass

    def read(self, count):
        self.clock.value += 1
        if self.fail:
            self.read_failed = True
            raise OSError("synthetic disconnect")
        return self.chunks.pop(0) if self.chunks else b"heartbeat\n"

    def write(self, data):
        self.writes.append(bytes(data))
        return len(data)

    def flush(self):
        pass

    def close(self):
        self.closed = True


class CaptureTests(unittest.TestCase):
    READY = b"wifi_station: provision_ready v=1 nonce=123456789abcdef0 firmware=0.1.4-wifi-sta country=US\n"

    def exercise(self, chunks, seconds=5, fail=False):
        with tempfile.TemporaryDirectory() as directory:
            credential = Path(directory) / "credentials"
            credential.write_bytes(b"fixture\ntest-only-secret\n")
            credential.chmod(0o600)
            log = Path(directory) / "capture.log"
            args = SimpleNamespace(credentials=credential, log=log, port="synthetic-only",
                                   firmware="0.1.4-wifi-sta", seconds=seconds)
            clock = Clock()
            device = FakeSerial(clock, chunks, fail)
            with patch.object(module.serial, "Serial", return_value=device), \
                    patch.object(module.time, "monotonic", clock.monotonic), \
                    patch.object(module.time, "sleep", clock.sleep), contextlib.redirect_stdout(io.StringIO()):
                result = module.capture(args)
            metadata = json.loads(log.with_suffix(".json").read_text())
            self.assertTrue(device.closed)
            self.assertEqual(log.stat().st_mode & 0o777, 0o600)
            self.assertEqual(log.with_suffix(".json").stat().st_mode & 0o777, 0o600)
            return result, metadata, log.read_bytes(), device.writes

    def test_success(self):
        result, meta, log, writes = self.exercise([
            b"ets Jun  8 2016\n", self.READY, b"wifi_station: provision_applied; no HTTP\n"])
        self.assertEqual(result, 0)
        self.assertTrue(meta["completed"] and meta["uart_commands_sent"] and meta["provision_acknowledged"])
        self.assertEqual(len(writes), 1)
        self.assertNotIn(b"test-only-secret", log)
        self.assertEqual(meta["bytes"], len(log))

    def test_wrong_firmware_never_sends(self):
        result, meta, _, writes = self.exercise([self.READY.replace(b"0.1.4-wifi-sta", b"wrong")])
        self.assertEqual(result, 1)
        self.assertFalse(meta["completed"])
        self.assertEqual(writes, [])

    def test_no_challenge_never_sends(self):
        result, meta, _, writes = self.exercise([], seconds=35)
        self.assertEqual(result, 1)
        self.assertEqual(meta["error_type"], "TimeoutError")
        self.assertEqual(writes, [])

    def test_disconnect_still_writes_metadata(self):
        result, meta, _, writes = self.exercise([], fail=True)
        self.assertEqual(result, 1)
        self.assertEqual(meta["error_type"], "OSError")
        self.assertEqual(len(meta["cleanup_errors"]), 2)
        self.assertFalse(meta["completed"])
        self.assertEqual(writes, [])

    def test_reset_never_resends(self):
        result, meta, _, writes = self.exercise([
            b"ets Jun  8 2016\n", self.READY,
            b"wifi_station: provision_applied; no HTTP\n", b"ets Jun  8 2016\n"], seconds=8)
        self.assertEqual(result, 1)
        self.assertFalse(meta["completed"])
        self.assertEqual(len(writes), 1)

    def test_echo_secret_redacted(self):
        result, meta, log, writes = self.exercise([self.READY, b"test-only-secret\n"])
        self.assertEqual(result, 1)
        self.assertFalse(meta["completed"])
        self.assertEqual(len(writes), 1)
        self.assertNotIn(b"test-only-secret", log)
        self.assertIn(b"REDACTED", log)


if __name__ == "__main__":
    unittest.main()
