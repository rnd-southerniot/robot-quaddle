#!/usr/bin/env python3
"""Live readout of pot_logger_uno — one refreshing line per channel.

    uv run --with pyserial tools/live.py

Wiggle one leg and watch which row moves. The `moved(3s)` column is the peak-to-peak
swing over the last three seconds, so the channel being driven is unmistakable.

Options:
    --port PORT     serial port (auto-detected if omitted)
    --seconds N     exit after N seconds instead of running until Ctrl-C
"""

from __future__ import annotations

import argparse
import glob
import sys
import time
from collections import deque

import serial  # type: ignore[import-untyped]

BAUD = 500_000
UNO_RESET_WAIT_S = 2.0
MOVE_WINDOW_S = 3.0
MOVE_THRESHOLD = 8          # counts of peak-to-peak that count as real motion, not noise
POT_TRAVEL_DEG = 330.0      # B103 electrical travel

CHANNELS = [
    ("A0", "fl", "ZQDWQ", "front-left"),
    ("A1", "fr", "YQDWQ", "front-right"),
    ("A2", "rl", "ZHDWQ", "rear-left"),
    ("A3", "rr", "YHDWQ", "rear-right"),
]


def find_port() -> str:
    candidates = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*"))
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        sys.exit("no USB serial port found — is the Uno plugged in?")
    sys.exit("several ports found, pass --port:\n  " + "\n  ".join(candidates))


def render(latest: list[int], history: list[deque[int]], rows_drawn: int) -> int:
    rail = latest[4]
    powered = rail > 100

    lines = []
    lines.append("")
    if powered:
        lines.append(f"  rail   {rail:4d} counts   {rail * 5.0 / 1024:5.3f} V   -> full {POT_TRAVEL_DEG:.0f} deg of track")
    else:
        lines.append("  rail      0 counts   NO SUPPLY — toy is off, or Mode B rail not connected")
    lines.append("  " + "-" * 74)
    lines.append(f"  {'pin':<4}{'ch':<4}{'connector':<11}{'leg':<13}{'counts':>7}{'volts':>8}{'% track':>9}{'moved(3s)':>11}")

    for i, (pin, ch, conn, leg) in enumerate(CHANNELS):
        v = latest[i]
        pp = (max(history[i]) - min(history[i])) if history[i] else 0
        frac = (v / rail) if powered and rail else 0.0
        flag = f"{pp:5d} <== MOVING" if pp >= MOVE_THRESHOLD else f"{pp:5d}"
        lines.append(
            f"  {pin:<4}{ch:<4}{conn:<11}{leg:<13}{v:7d}{v * 5.0 / 1024:8.3f}{frac * 100:8.1f}%{flag:>11}"
        )
    lines.append("")
    lines.append("  Ctrl-C to quit")

    # Redraw in place: jump back over what was printed last time.
    if rows_drawn:
        sys.stdout.write(f"\033[{rows_drawn}A")
    for line in lines:
        sys.stdout.write("\033[2K" + line + "\n")
    sys.stdout.flush()
    return len(lines)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port")
    ap.add_argument("--seconds", type=float)
    args = ap.parse_args()

    port = args.port or find_port()
    history: list[deque[int]] = [deque(maxlen=int(MOVE_WINDOW_S * 200)) for _ in range(4)]
    latest = [0, 0, 0, 0, 0]
    rows_drawn = 0

    with serial.Serial(port, BAUD, timeout=0.3) as ser:
        print(f"opened {port}, waiting {UNO_RESET_WAIT_S:.0f}s for the Uno to reboot...")
        time.sleep(UNO_RESET_WAIT_S)
        ser.reset_input_buffer()
        ser.write(b"s")
        ser.flush()

        started = time.monotonic()
        last_draw = 0.0
        try:
            while True:
                if args.seconds is not None and time.monotonic() - started >= args.seconds:
                    break

                line = ser.readline().decode("utf-8", errors="replace").strip()
                parts = line.split(",")
                if len(parts) == 7 and parts[0].isdigit():
                    latest = [int(x) for x in parts[1:6]]
                    for i in range(4):
                        history[i].append(latest[i])

                now = time.monotonic()
                if now - last_draw >= 0.1:          # 10 Hz redraw; the link runs at 200 Hz
                    rows_drawn = render(latest, history, rows_drawn)
                    last_draw = now
        except KeyboardInterrupt:
            pass
        finally:
            ser.write(b"x")
            ser.flush()
            print()


if __name__ == "__main__":
    main()
