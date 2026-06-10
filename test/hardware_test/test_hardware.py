#!/usr/bin/env python3
"""
LLP Hardware Test — automated test runner for Arduino Nano.

Usage:
  python3 test/hardware_test/test_hardware.py                  # use defaults
  python3 test/hardware_test/test_hardware.py --port /dev/ttyUSB1
  python3 test/hardware_test/test_hardware.py --baud 9600 --no-upload

The script:
  1. Builds the firmware (skip with --no-upload)
  2. Uploads to the board (skip with --no-upload)
  3. Runs ~20 test cases against the device
  4. Reports pass/fail per test and exits with code 0/1

Requirements:
  - PlatformIO  (pip install platformio)
  - pyserial    (pip install pyserial)
  - Arduino Nano connected via USB
"""

import argparse
import os
import subprocess
import sys
import time

# ---------------------------------------------------------------------------
# Paths — everything relative to the project root
# ---------------------------------------------------------------------------
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
SKETCH_DIR = os.path.join(PROJECT_ROOT, "test", "hardware_test")
SKETCH_ENV = "nano_hardware_test"

DEFAULT_PORT = "/dev/ttyUSB0"
DEFAULT_BAUD = 115200


# =========================================================================
# CRC16-CCITT  (matches llp_crc16_update)
# =========================================================================
def _crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
        crc &= 0xFFFF
    return crc


# =========================================================================
# LLP frame builder  (matches llp_build_frame)
# =========================================================================
def _stuff(b):
    return bytes([b, 0x00]) if b == 0xAA else bytes([b])


def build_llp_frame(payload: bytes) -> bytes:
    """Build a complete LLP wire frame from a raw payload (layer chain)."""
    crc_input = (
        bytes([0xAA, 0x55, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF])
        + payload
    )
    crc = _crc16(crc_input)

    frame = bytes([0xAA, 0x55])
    frame += _stuff(len(payload) & 0xFF)
    frame += _stuff((len(payload) >> 8) & 0xFF)
    for b in payload:
        frame += _stuff(b)
    frame += _stuff(crc & 0xFF)
    frame += _stuff((crc >> 8) & 0xFF)
    return frame


# =========================================================================
# Test harness
# =========================================================================
class Tester:
    def __init__(self, port: str, baud: int):
        self.port = port
        self.baud = baud
        self.ser = None
        self.passed = 0
        self.failed = 0

    # ---- helpers -------------------------------------------------------

    def _read_ok(self, timeout: float = 0.6) -> str:
        """Read raw data until we find an 'OK' or 'ERR' line, or timeout."""
        self.ser.timeout = timeout
        buf = b""
        deadline = time.time() + timeout * 3
        while time.time() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                buf += chunk
                # search for "OK " or "ERR " (space-separated tokens)
                text = buf.decode(errors="replace")
                for line in text.splitlines():
                    if line.startswith("OK ") or line.startswith("ERR "):
                        return line.strip()
            else:
                # no data this cycle — small sleep to avoid busy-wait
                time.sleep(0.05)
        return ""

    def run(self, name: str, payload_hex: str, expect_ok: bool = True):
        """Send an LLP frame and expect OK (or ERR)."""
        payload = bytes.fromhex(payload_hex)
        frame = build_llp_frame(payload)
        self.ser.write(frame)
        got = self._read_ok()
        ok = (got.startswith("OK")) if expect_ok else (got.startswith("ERR"))
        if ok:
            self.passed += 1
            print(f"  [PASS] {name}: {got}")
        else:
            self.failed += 1
            expect_str = "OK" if expect_ok else "ERR"
            print(f"  [FAIL] {name}: expected {expect_str}, got: {got or '(no response)'}")
        return ok

    def run_frame(self, name: str, raw_bytes: bytes, expect_ok: bool = True):
        """Send raw bytes and expect OK/ERR response."""
        self.ser.write(raw_bytes)
        got = self._read_ok()
        ok = (got.startswith("OK")) if expect_ok else (got.startswith("ERR"))
        if ok:
            self.passed += 1
            print(f"  [PASS] {name}: {got}")
        else:
            self.failed += 1
            expect_str = "OK" if expect_ok else "ERR"
            print(f"  [FAIL] {name}: expected {expect_str}, got: {got or '(no response)'}")
        return ok

    # ---- test groups ---------------------------------------------------

    def test_valid_payloads(self):
        print("\n[Valid payloads]")
        print("-" * 50)
        cases = [
            ("empty",             "00"),
            ("single_byte_0x42",  "0042"),
            ("hello_world",       "0048656C6C6F"),
            ("hex_cafebabe",      "00CAFEBABE"),
            ("zeros_4",           "0000000000"),
            ("byte_0xAA_stuffed", "00AA"),
            ("byte_0x55",         "0055"),
            ("byte_0xFF",         "00FF"),
            ("byte_0x7F",         "007F"),
            ("byte_0x80",         "0080"),
            ("byte_0xFE",         "00FE"),
            ("aa55_magic_seq",    "00AA55"),
            ("triple_aa",         "00AAAAAA"),
            ("mixed_aa",          "0001AA02AA03"),
            ("ascii_test",        "0054657374"),
            ("payload_16_bytes",  "00000102030405060708090A0B0C0D0E0F"),
            ("payload_32_bytes",  "00" + "".join(f"{i:02X}" for i in range(32))),
        ]
        for name, hex_payload in cases:
            self.run(name, hex_payload)

    def test_errors(self):
        print("\n[Error detection]")
        print("-" * 50)
        # Corrupted CRC — flip last byte
        bad = bytearray(build_llp_frame(bytes.fromhex("0042")))
        bad[-1] ^= 0xFF
        self.run_frame("corrupted_crc", bytes(bad), expect_ok=False)

        # Invalid escape inside frame
        self.run_frame("invalid_escape_0x01",
                       bytes.fromhex("AA55020000AA0197A6"), expect_ok=False)
        self.run_frame("invalid_escape_0xFF",
                       bytes.fromhex("AA55020000AAFF97A6"), expect_ok=False)
        self.run_frame("raw_aa_unescaped",
                       bytes.fromhex("AA55020000AA97A6"), expect_ok=False)

    def test_noise_recovery(self):
        print("\n[Noise recovery]")
        print("-" * 50)
        # Garbage followed by valid frame
        self.ser.write(b"\xFF\xFE\xDE\xAD")
        time.sleep(0.2)
        self.run("noise_then_valid", "00CAFE")

    def test_sequential_frames(self):
        print("\n[Sequential frames]")
        print("-" * 50)
        all_ok = True
        for val in [0x01, 0x02, 0x03, 0xAA, 0x55]:
            if not self.run(f"frame_{val:02X}", f"00{val:02X}"):
                all_ok = False
        if all_ok:
            self.passed += 1
            print("  [PASS] all_sequential")
        else:
            self.failed += 1
            print("  [FAIL] all_sequential — one or more frames failed")

    def test_zero_copy_api(self):
        print("\n[Zero-copy API]")
        print("-" * 50)
        self.run("llp_get_final_payload_ptr(0042)", "0042")
        self.run("llp_get_final_payload_ptr(0048656C6C6F)", "0048656C6C6F")

    # ---- main runner ---------------------------------------------------

    def run_all(self):
        header = "=" * 56
        print()
        print(header)
        print("LLP v3.1.0 — Hardware Test Suite")
        print(f"Port: {self.port} @ {self.baud} baud")
        print(header)

        import serial as _serial
        self.ser = _serial.Serial()
        self.ser.port = self.port
        self.ser.baudrate = self.baud
        self.ser.timeout = 3
        self.ser.setDTR(False)
        self.ser.setRTS(False)
        self.ser.open()

        # Flush boot messages / heartbeats
        time.sleep(2)
        self.ser.reset_input_buffer()
        time.sleep(0.5)
        self.ser.reset_input_buffer()

        # Verify device is alive
        self.ser.timeout = 5
        alive = False
        for _ in range(10):
            line = self.ser.readline().decode(errors="replace").strip()
            if "[BOOT]" in line or "[HB]" in line:
                alive = True
                break
        if not alive:
            print("\n  WARNING: no boot/heartbeat from device — "
                  "is the correct firmware loaded?\n")

        self.test_valid_payloads()
        self.test_errors()
        self.test_noise_recovery()
        self.test_sequential_frames()
        self.test_zero_copy_api()

        self.ser.close()

        print()
        print(header)
        total = self.passed + self.failed
        print(f"Results: {self.passed}/{total} passed", end="")
        if self.failed == 0:
            print(" — ALL TESTS PASSED")
        else:
            print(f" — {self.failed} FAILED")
        print(header)
        return self.failed == 0


# =========================================================================
# Build helpers
# =========================================================================
def build_sketch() -> bool:
    print(f"Building firmware in {SKETCH_DIR} ...")
    result = subprocess.run(
        ["pio", "run", "-d", SKETCH_DIR, "-e", SKETCH_ENV],
        capture_output=True, text=True, cwd=PROJECT_ROOT,
    )
    if result.returncode != 0:
        print(result.stderr)
        print("BUILD FAILED")
        return False
    # Print size info
    for line in result.stdout.splitlines():
        if "RAM:" in line or "Flash:" in line:
            print(f"  {line.strip()}")
    print("  Build OK")
    return True


def upload_sketch(port: str) -> bool:
    print(f"Uploading to {port} ...")
    result = subprocess.run(
        ["pio", "run", "-d", SKETCH_DIR, "-e", SKETCH_ENV,
         "--target", "upload", "--upload-port", port],
        capture_output=True, text=True, cwd=PROJECT_ROOT,
    )
    if result.returncode != 0:
        print(result.stderr)
        print("UPLOAD FAILED")
        return False
    print("  Upload OK")
    return True


# =========================================================================
# CLI
# =========================================================================
def main():
    parser = argparse.ArgumentParser(
        description="LLP Hardware Test — Arduino Nano",
    )
    parser.add_argument("--port", default=DEFAULT_PORT,
                        help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD,
                        help=f"Baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--no-upload", action="store_true",
                        help="Skip build & upload (device already has firmware)")
    args = parser.parse_args()

    if not args.no_upload:
        if not build_sketch():
            sys.exit(1)
        if not upload_sketch(args.port):
            sys.exit(1)
        print("  Waiting for device to restart...")
        time.sleep(3)

    tester = Tester(args.port, args.baud)
    success = tester.run_all()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
