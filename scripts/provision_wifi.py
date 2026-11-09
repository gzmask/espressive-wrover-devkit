#!/usr/bin/env python3
"""Local, one-shot UART provisioning and private capture.
Python is used specifically for pyserial; no credentials enter CLI args,
firmware images, sdkconfig, or printed results. This is not encrypted USB.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import stat
import struct
import time
import zlib

import serial


def make_frame(ssid, password, nonce):
    if not 1 <= len(ssid) <= 32 or any(c < 32 or c == 127 for c in ssid):
        raise ValueError("Invalid SSID bytes")
    if not (8 <= len(password) <= 63 and all(32 <= c <= 126 for c in password)
            or len(password) == 64 and re.fullmatch(rb"[0-9a-fA-F]{64}", password)):
        raise ValueError("Invalid PSK/passphrase format")
    frame = bytearray(struct.pack("<4sQBB", b"WRV1", nonce, len(ssid), len(password)))
    frame.extend(ssid)
    frame.extend(password)
    frame.extend(struct.pack("<I", zlib.crc32(frame)))
    return frame


def read_credentials(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    try:
        info = os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_uid != os.getuid() or info.st_mode & 0o077:
            raise ValueError("Credential file must be owner-only and owned by current user")
        raw = os.read(descriptor, 257)
    finally:
        os.close(descriptor)
    if len(raw) > 256:
        raise ValueError("Credential file too large")
    lines = raw.split(b"\n")
    if lines and lines[-1] == b"":
        lines.pop()
    lines = [line[:-1] if line.endswith(b"\r") else line for line in lines]
    if len(lines) != 2:
        raise ValueError("Expected exactly SSID and password lines")
    temporary = make_frame(lines[0], lines[1], 0)
    temporary[:] = b"\0" * len(temporary)
    return tuple(lines)


def challenge(line, expected_firmware):
    match = re.search(rb"provision_ready v=1 nonce=([0-9a-f]{16}) firmware=([\w.-]+) country=US", line)
    if not match:
        return None
    if match[2].decode("ascii") != expected_firmware:
        raise ValueError("Unexpected firmware identity")
    return int(match[1], 16)


def sanitize(line, password):
    patterns = (password, password.hex().encode("ascii"), password.hex().upper().encode("ascii"))
    if any(pattern in line for pattern in patterns):
        return b"[REDACTED credential-bearing UART line; capture aborted]\n", True
    return line, False


def capture(args):
    ssid, password = read_credentials(args.credentials)
    log_path = Path(args.log)
    metadata_path = log_path.with_suffix(".json")
    if metadata_path.exists():
        raise ValueError("Refusing to overwrite metadata")
    if not log_path.parent.is_dir() or log_path.parent.stat().st_mode & 0o077:
        raise ValueError("Capture directory must already exist and be owner-only")
    descriptor = os.open(log_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2, exclusive=True)
    port.dtr = False
    port.rts = False
    port.port = args.port
    metadata = {"completed": False, "port": args.port, "baud": 115200,
                "normal_rts_reset": False, "uart_commands_sent": False,
                "provision_acknowledged": False, "country": "US",
                "firmware": args.firmware, "credential_values_logged": False,
                "cleanup_errors": []}
    start = time.monotonic()
    buffered = bytearray()
    sent_at = None
    error = None
    with os.fdopen(descriptor, "wb") as output:
        try:
            port.open()
            port.reset_input_buffer()
            port.rts = True
            time.sleep(0.1)
            port.rts = False
            metadata["normal_rts_reset"] = True
            metadata["started_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
            start = time.monotonic()
            boot_count = 0
            while time.monotonic() - start < args.seconds:
                data = port.read(min(port.in_waiting or 1, 1024))
                buffered.extend(data)
                if len(buffered) > 8192:
                    raise RuntimeError("Oversized unterminated UART data")
                while b"\n" in buffered:
                    line, _, remainder = buffered.partition(b"\n")
                    buffered = bytearray(remainder)
                    line = bytes(line) + b"\n"
                    clean, matched = sanitize(line, password)
                    output.write(clean)
                    output.flush()
                    if matched:
                        raise RuntimeError("Sensitive UART output redacted")
                    if b"ets Jun  8 2016" in line:
                        boot_count += 1
                        if boot_count > 1:
                            raise RuntimeError("Unexpected reset; never resend credentials automatically")
                    token = challenge(line, args.firmware)
                    if token is not None:
                        if sent_at is not None:
                            raise RuntimeError("Repeated provisioning challenge")
                        frame = make_frame(ssid, password, token)
                        try:
                            metadata["uart_commands_sent"] = True
                            if port.write(frame) != len(frame):
                                raise RuntimeError("Partial credential frame write")
                            port.flush()
                        finally:
                            frame[:] = b"\0" * len(frame)
                        sent_at = time.monotonic()
                    if b"wifi_station: provision_applied;" in line:
                        if sent_at is None:
                            raise RuntimeError("Unexpected provisioning acknowledgement")
                        metadata["provision_acknowledged"] = True
                    if b"station terminal error;" in line:
                        raise RuntimeError("Device entered terminal error")
                if sent_at is None and time.monotonic() - start >= 30:
                    raise TimeoutError("No expected firmware challenge; nothing sent")
                if sent_at is not None and not metadata["provision_acknowledged"] and time.monotonic() - sent_at >= 30:
                    raise TimeoutError("No provisioning acknowledgement")
            if sent_at is None or not metadata["provision_acknowledged"]:
                raise RuntimeError("Provisioning incomplete")
            metadata["completed"] = True
        except Exception as failure:
            # Never print exception values or source lines containing file data.
            error = type(failure).__name__
            metadata["error_type"] = error
        finally:
            metadata["duration_seconds"] = time.monotonic() - start
            # A disconnect must not mask the primary failure or skip metadata.
            for attribute in ("rts", "dtr"):
                try:
                    setattr(port, attribute, False)
                except Exception as failure:
                    metadata["cleanup_errors"].append(type(failure).__name__)
            try:
                port.close()
            except Exception as failure:
                metadata["cleanup_errors"].append(type(failure).__name__)
            buffered[:] = b"\0" * len(buffered)
    metadata["bytes"] = log_path.stat().st_size
    descriptor = os.open(metadata_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "w") as output:
        json.dump(metadata, output, indent=2)
        output.write("\n")
    print(json.dumps(metadata))
    return 0 if metadata["completed"] and error is None else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--credentials", required=True, type=Path)
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--port")
    parser.add_argument("--log", type=Path)
    parser.add_argument("--seconds", type=float, default=690)
    parser.add_argument("--firmware", default="0.1.4-wifi-sta")
    args = parser.parse_args()
    if args.validate_only:
        read_credentials(args.credentials)
        print(json.dumps({"format_valid": True, "country": "US", "values_redacted": True}))
        return 0
    if not args.port or not args.log or not 1 <= args.seconds <= 900:
        parser.error("Specify port, private log path and duration 1..900 seconds")
    return capture(args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError) as failure:
        print(json.dumps({"completed": False, "error_type": type(failure).__name__,
                          "credential_values_logged": False}))
        raise SystemExit(1)
