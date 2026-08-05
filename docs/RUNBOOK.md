# Runbook — bench procedures

Step-by-step procedures for things done repeatedly. If you find yourself working something out
from first principles at the bench, it belongs here afterwards.

---

## Before every session

- [ ] Confirm nobody else has the robot half-disassembled.
- [ ] **Body on a stand, feet off the ground** for anything through Phase 2.
- [ ] Toy switched on if you are in Mode A; check the pack is not flat.
- [ ] Read [`NEXT_SESSION.md`](NEXT_SESSION.md).

## After every session

- [ ] **Verify the toy still works on its stock remote.** This is the Phase 0 exit gate and it
      cannot be recovered once lost.
- [ ] Measurements into the tables in `PHASE_0_TEARDOWN.md`.
- [ ] Dated entry appended to the **State** block in `CLAUDE.md`, including *why*.
- [ ] Update `NEXT_SESSION.md`.
- [ ] Photos and captures committed with descriptive names.

---

## 1. The two wiring modes

The pots have no supply of their own. Which one you are in decides who powers them, and mixing
them up back-feeds the stock board.

### Mode A — toy powers the pots (behaviour captures)

Toy **ON**. `DWQ` connectors stay plugged into the stock board; we back-probe them and read only.
**Never inject 3.3 V in this mode.**

| Uno | Connector | Leg |
|---|---|---|
| A0 | `ZQDWQ` wiper (blue, centre pin) | front-left |
| A1 | `YQDWQ` wiper | front-right |
| A2 | `ZHDWQ` wiper | rear-left |
| A3 | `YHDWQ` wiper | rear-right |
| A4 | pot supply rail — either outer pin of any `DWQ` | — |
| D2 | marker button → **the Uno's own GND** | — |
| GND | **one** wire to the toy, landed on a `DWQ` **ground pin** | — |

### Mode B — we power the pots (hand calibration)

Toy **OFF**, so the firmware cannot fight you and no motor can run.

1. Switch the toy off.
2. **Unplug all four `DWQ` connectors from the stock board.** Not optional — injecting 3.3 V into
   a still-plugged connector back-feeds the board's 3.3 V net and can part-power the JieLi and
   `U5`.
3. Pot track ends → Uno `3V3` and Uno `GND`. Wipers → A0–A3. A4 → the Uno's own `3V3`.
4. Four pots draw 1.3 mA total; the Uno's `3V3` pin is good for ~50 mA.

Returning to Mode A: remove the injected 3.3 V **before** reconnecting the connectors and
powering the toy.

### Grounding — the part people get wrong

- **One** ground wire between Uno and toy. A second makes a loop, and motor current in that loop
  becomes noise on every trajectory.
- Land it at a **`DWQ` ground pin**, not battery negative and not a motor connector. The toy's
  ground carries amps of motor return current; at the `DWQ` ground the drop is common to both the
  pot divider and our ADC reference and cancels.
- The marker button returns to the **Uno's** GND. It is a local circuit.

**Symptom to recognise:** all four legs showing the same small wiggle at motor-switching
frequency is a ground problem, not the legs moving together.

---

## 2. Bring the logger up

```bash
cd <repo root>

# find the board
arduino-cli board list                  # expect Arduino UNO on /dev/cu.usbmodemXXXX

# build only
arduino-cli compile --fqbn arduino:avr:uno tools/pot_logger_uno

# build and upload
arduino-cli compile --fqbn arduino:avr:uno -u -p /dev/cu.usbmodem1301 tools/pot_logger_uno
```

Expect roughly `Sketch uses 3808 bytes (11%)`, `Global variables use 211 bytes (10%)`.

**If no port appears** but `ioreg` shows the device, wait a few seconds — the node can lag the USB
enumeration. Check with:

```bash
ioreg -p IOUSB -w0 -l | grep -E '"idVendor"|"idProduct"|"USB Product Name"'
```

Our Uno enumerates as `idVendor 9025 (0x2341)`, `idProduct 67 (0x0043)`, product string
`Generic CDC` — a clone descriptor. Works identically.

---

## 3. Live readout

```bash
uv run --with pyserial tools/live.py
```

Refreshes at 10 Hz. The `moved(3s)` column is peak-to-peak over the last three seconds, so
wiggling one leg puts `<== MOVING` on its row. Ctrl-C to quit.

**Acceptance check before any capture:**

| Channel | Expected | If not |
|---|---|---|
| `rail` | **≈ 674 counts / 3.29 V** | 0 → toy is off, or Mode B rail not connected. Near 1023 → you are on a 5 V node. Much lower → wrong ground or wrong outer pin. |
| `fl` `fr` `rl` `rr` | 0–674, each responds to **its own** leg alone | Pinned at 0 or 1023 → not on the wiper. Wrong row lights up → taps crossed; **fix at the wires, never in software.** |

---

## 4. Take a behaviour capture

Mode A, toy on.

```bash
uv run --with pyserial tools/capture.py sit
uv run --with pyserial tools/capture.py walk_forward --seconds 10
```

- One file per remote button, named for the button. Files land in `hardware/captures/`.
- Poses on a stand, walking on the floor. **Record which, per run.**
- Press the marker button as you press the remote, so the trigger is inside the data.
- **Check the trailer says `overruns=0`.** Non-zero means dropped samples and gaps in the timing —
  discard and retake. A capture with unknown overruns cannot be used for gait period.

The script waits out the Uno's DTR reset, starts the stream, and on exit always collects the
trailer. It refuses to overwrite an existing capture.

---

## 5. Single readings for calibration

Mode B, toy off. Any serial monitor at **500000 baud**, or `tools/live.py`.

| Key | Effect |
|---|---|
| `s` | start streaming |
| `x` | stop, print trailer |
| `r` | print **one** sample — this is the calibration command |
| `m` | software marker |
| `h` | reprint header |

```bash
uv run --with pyserial python -m serial.tools.miniterm /dev/cu.usbmodem1301 500000
```

Used for **P0-05** (pot counts at each hard stop) and **P0-07** (pot reading at each
hand-positioned foot-path step). Avoid `screen` — it captures the terminal and needs `Ctrl-A K`
to escape.

---

## 6. Logic analyser (sigrok)

Our analyser is **not** a modern Saleae. It enumerates as `0x0925:0x3881` with no USB strings —
an FX2LP clone of the original Logic. **Digital only, 8 channels, no analog inputs.** Saleae
Logic 2 will not see it.

```bash
brew install sigrok-cli
# then fetch sigrok-firmware-fx2lafw-bin from sigrok.org and place it where libsigrok looks:
sigrok-cli -L                       # shows the firmware search path
sigrok-cli --scan                   # expect the device as fx2lafw
```

The firmware blob is **not** in Homebrew — these clones ship with no firmware and libsigrok
uploads `fx2lafw` at runtime. There is no PulseView cask either; `sigrok-cli` exports CSV, which
is what we want.

```bash
sigrok-cli -d fx2lafw --config samplerate=1m --channels D0,D1,D2,D3 --time 5s -O csv > walk.csv
```

Use it for the MX1616S driver inputs (**P0-09**), not the pots — pot capture needs analog and
that is what the Uno rig is for.

---

## 7. SWD probing — read only

⚠ **Read `PHASE_0_SWD_AND_LOGIC.md` §0 first.** The single rule: never erase, unlock, or
mass-erase, whatever the tool suggests. A failed read is acceptable. An erased chip destroys the
rollback path permanently.

- `VMCU` is measured at 3.3 V, so a 3.3 V CMSIS-DAP probe is level-compatible.
- **Do not** connect the probe's VCC to the board. Use `VTref` if the probe has one.
- Connect **GND first**, then `SWD` (SWDIO), then `CLK` (SWCLK). Start at 100 kHz.
- Motor supply off if possible; otherwise body on a stand, legs hanging free.
- **Attach without halting** — `pyocd commander --connect=attach --target=cortex_m`. The generic
  `cortex_m` target loads no flash algorithm, so nothing is present that could erase.
- There is **no `NRST` pad**. Connect-under-reset is unavailable, and a failed connect most likely
  means the stock firmware repurposed the SWD pins at boot. That is expected, not a fault.

---

## 8. Restore the toy to stock

Do this whenever the bench is handed over or the session ends messily.

1. Remove all taps and probe wires.
2. Reconnect all eight connectors — **housing colour is the left/right key: white = left, yellow
   = yellow/right.** `ZQ`/`YQ` are front, `ZH`/`YH` are rear.
3. Reconnect the battery.
4. Power on and run several remote functions.
5. Record the result in the State block.

If it does **not** work, stop and escalate before doing anything else. Something in the harness
is wrong and it is far cheaper to find now than after ten more sessions.
