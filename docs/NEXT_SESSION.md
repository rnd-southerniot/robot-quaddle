# Next session

**Updated:** 2026-08-04 · **Phase:** 0 (teardown & characterisation) · **Status:** in progress,
blocking Phase 1

Read this before every session. Update it at the end of every session.

---

## Where we are in one paragraph

The toy has been identified and largely characterised. It is a **4-actuator machine, one brushed
DC motor per leg, each with a potentiometer on the gearbox output** — so every leg is a servo
built from discrete parts, and the motion architecture is locked to **position control**. A
measurement rig is built, uploaded and verified on hardware: an Arduino Uno logging all four pot
wipers plus the supply rail at 200 Hz with zero dropped samples. The channel map has been
confirmed against a physical leg. **Nothing has been captured yet** — the next session takes the
stock motion library, which is the highest-value data in Phase 0.

---

## Bench state, physically

| Item | State |
|---|---|
| Toy | Opened, mechanism accessible, **still works on its stock remote** |
| Stock PCB | Intact. Nothing cut, desoldered, erased or reflashed. |
| Uno | Flashed with `tools/pot_logger_uno`, on `/dev/cu.usbmodem1301` |
| Pot taps | Wired **Mode A** (toy powers the pots; we read only). Taps were permuted once and have been corrected **at the wires**. |
| Marker button | On D2, returning to the **Uno's** GND |
| Logic analyser | FX2LP clone, `0x0925:0x3881`. **Digital only.** sigrok not yet installed. |
| Captures taken | **None** |

⚠ **The toy has been switched off between sessions.** With it off, every channel including `rail`
reads exactly 0. That is not a fault — the pots have no supply of their own.

---

## Verified so far

| Fact | Value | Status |
|---|---|---|
| Actuators | 4 motors, 4 pots, one per leg. No head/tail/mouth motors. | `PROVEN` — connector silkscreen + count |
| Sensor | Potentiometer, `B103` = 10 kΩ linear, 330° travel, on the **output** shaft | `PROVEN` — three ways: ohmmeter, body marking, silkscreen `DWQ` = 电位器 |
| Leg motion | Limited arc, sweeps back and forth. Not a continuous crank. | `PROVEN` |
| Pot supply rail | 3.3 V (measured 3.289 V = 674 counts) | `PROVEN` |
| `VMCU` | 3.3 V — so a 3.3 V CMSIS-DAP probe is level-compatible | `PROVEN` |
| Board | `JXD-8002-Blue-YW-RV2`, dated 2024-01-19. **No documentation exists online — searched, do not repeat.** | `PROVEN` |
| Channel A0 | = `ZQDWQ` = front-left. 348 counts (170.5°) while others stayed ≤ 10. **34.8× separation, no cross-talk.** | `PROVEN` |
| Channels A1–A3 | front-right / rear-left / rear-right | `ASSUMED` — follow from the corrected tap order, not individually driven |
| Leg travel | ~170° per leg, roughly half the pot's 330° track | `PROVEN` as **lower bounds** — hand sweeps, stops not reached |
| Logger scale | 0.49°/count on the Uno rig; 200 Hz, ±0.24 % jitter, 0 overruns | `PROVEN` |
| Legs backdrivable by hand | Yes — swept 100+ counts each while powered | `ASSUMED` — assumes they were moved by hand, not by remote |

---

## Still open

| Q | Question | Blocks |
|---|---|---|
| **Q3** | Stall current per motor | Driver IC selection — **nothing gets ordered until this exists** |
| **Q4** | Battery: chemistry, cells, capacity, charge topology | The whole power tree |
| **Q5** | Pot counts at the true mechanical stops | Software limits; Phase 2 |
| **Q8** | Are the MX1616S halves paralleled onto one motor? | Driver current floor |
| **Q9** | Foot-path curve per leg | The leg model; all of Phase 3 |
| **Q10** | What is `U5`? | Nothing critical — informational |

---

## Do next, in this order

### 1. Switch the toy on and confirm the rig (2 min)

```bash
cd <repo root>
uv run --with pyserial tools/live.py
```

Expect `rail ≈ 674 counts / 3.29 V` and four legs mid-track. If `rail` reads 0, the toy is off.
Wiggle one leg and confirm `<== MOVING` lands on the row you expect.

### 2. Capture the stock motion library — **P0-08, the highest-value task**

```bash
uv run --with pyserial tools/capture.py sit
uv run --with pyserial tools/capture.py stand
uv run --with pyserial tools/capture.py lie_down
uv run --with pyserial tools/capture.py paw
# then on the floor:
uv run --with pyserial tools/capture.py walk_forward --seconds 10
uv run --with pyserial tools/capture.py walk_backward --seconds 10
uv run --with pyserial tools/capture.py turn_left --seconds 10
uv run --with pyserial tools/capture.py turn_right --seconds 10
```

Poses on a stand, walking on the floor — note which per run. Press the marker button as you press
the remote. **Reject any capture whose trailer does not say `overruns=0`.**

Why first: this data can only be taken while the toy still works, and every later step carries
some risk to that. Capture the perishable thing first.

### 3. Three quick measurements while the body is open (30 min total)

- **P0-03 / Q8** — continuity-check whether each MX1616S has both halves on one motor
- **P0-04 / Q10** — microscope photo of `U5`'s marking
- **P0-02 / Q4** — battery label, pack voltage, charge path

### 4. Then the measurements that unblock purchasing

- **P0-06 / Q3** — stall current per motor → selects the driver IC
- **P0-05 / Q5** — pot counts at the true stops, in **Mode B** wiring

---

## Do NOT do

- **Do not order any parts.** The driver depends on Q3, which is not measured.
- **Do not erase, unlock or reflash `U5`.** Read-only, always. See `PHASE_0_SWD_AND_LOGIC.md` §0.
- **Do not halt the core over SWD with the H-bridges live** — a frozen driver input stalls a motor
  against a stop.
- **Do not inject 3.3 V into a `DWQ` connector that is still plugged into the stock board.** That
  back-feeds the board's 3.3 V net and can part-power the JieLi and `U5`. Unplug first — that is
  the whole point of Mode B.
- **Do not write a per-channel remap constant in software.** The taps were fixed at the wires and
  it must stay that way.
- **Do not start Phase 1 firmware.** Phase 0's gate is not met.

---

## Open judgement calls for whoever picks this up

1. **A1–A3 are `ASSUMED`, not proven.** One 15-second run moving rear-left alone would close it.
   Cheap insurance, or let the first `paw` capture cross-check it — your call, but decide
   deliberately rather than by forgetting.
2. **The diagonal resting pattern** (FL ≈ RR, FR ≈ RL) hints the toy parks in a trot stance. The
   walk captures will settle it. Recorded as a hypothesis, not a finding.
3. **Source a second toy (STD-01).** It is already sold out at the original reseller. A stripped
   gearbox is the most likely way this project dies.

---

## Session log format

Append to the **State** block at the bottom of `CLAUDE.md`:

```
- **YYYY-MM-DD** — <what changed, what was measured, what it means, and WHY a decision went the
  way it did. Include the numbers. Note anything that was corrected, so the wrong version cannot
  be inherited.>
```

Then update this page: bench state, verified table, still-open table, and the do-next list.
