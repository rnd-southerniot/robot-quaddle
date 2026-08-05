#!/usr/bin/env python3
"""Capture one behaviour run from pot_logger_uno into a CSV file.

    ./capture.py walk_forward
    ./capture.py sit --port /dev/tty.usbmodem1301 --seconds 10

Writes hardware/captures/<name>.csv. Press Ctrl-C to stop early; the run trailer
from the sketch is still collected so overrun count is never lost.

Requires pyserial:  uv run --with pyserial ./capture.py <name>
"""

from __future__ import annotations

import argparse
import glob
import sys
import time
from pathlib import Path

import serial  # type: ignore[import-untyped]

BAUD = 500_000
CAPTURE_DIR = Path(__file__).resolve().parent.parent / "hardware" / "captures"

# Opening the port asserts DTR, which resets an Uno. Nothing it prints before the
# reset completes is trustworthy, so the boot window is discarded rather than parsed.
UNO_RESET_WAIT_S = 2.0


def find_port() -> str:
    """Return the single likely Uno port, or exit with what was found."""
    candidates = sorted(glob.glob("/dev/tty.usbmodem*") + glob.glob("/dev/tty.usbserial*"))
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        sys.exit("no USB serial port found — is the Uno plugged in?")
    sys.exit(f"several ports found, pass --port explicitly:\n  " + "\n  ".join(candidates))


def capture(port: str, out_path: Path, seconds: float | None) -> None:
    with serial.Serial(port, BAUD, timeout=1.0) as ser:
        print(f"opened {port}, waiting {UNO_RESET_WAIT_S:.0f}s for the Uno to reboot...")
        time.sleep(UNO_RESET_WAIT_S)
        ser.reset_input_buffer()

        ser.write(b"s")
        ser.flush()
        print(f"streaming -> {out_path}   (Ctrl-C to stop)")

        rows = 0
        started = time.monotonic()
        with out_path.open("w", encoding="utf-8") as fh:
            try:
                while True:
                    if seconds is not None and time.monotonic() - started >= seconds:
                        break

                    line = ser.readline().decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    fh.write(line + "\n")
                    if not line.startswith("#"):
                        rows += 1
                        if rows % 200 == 0:
                            print(f"\r{rows} samples", end="", flush=True)
            except KeyboardInterrupt:
                print("\nstopping...")

            # Ask the sketch to stop and drain its trailer, which carries the
            # overrun count. A capture whose overruns are unknown is not trustworthy.
            ser.write(b"x")
            ser.flush()
            deadline = time.monotonic() + 1.0
            while time.monotonic() < deadline:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                fh.write(line + "\n")
                if line.startswith("# stopped"):
                    print(line)
                    break
                if line.startswith("# WARNING"):
                    print(line)

        print(f"{rows} samples written to {out_path}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("name", help="behaviour name, e.g. walk_forward")
    ap.add_argument("--port", help="serial port (auto-detected if omitted)")
    ap.add_argument("--seconds", type=float, help="stop automatically after N seconds")
    args = ap.parse_args()

    CAPTURE_DIR.mkdir(parents=True, exist_ok=True)
    out_path = CAPTURE_DIR / f"{args.name}.csv"
    if out_path.exists():
        sys.exit(f"{out_path} already exists — pick another name or move it aside")

    capture(args.port or find_port(), out_path, args.seconds)


if __name__ == "__main__":
    main()
