# Phase 0 — Teardown & Characterisation

**Goal:** replace every `UNKNOWN` in `HARDWARE_REFERENCE.md` §3 with a measured value, so the
driver, MCU and power decisions in `ARCHITECTURE.md` stop being guesses.

**Entry criteria:** toy in hand, working. Bench PSU with current limit, multimeter, calipers,
phone/microscope camera, Saleae Logic 2.

**Nothing is cut, desoldered, or modified in this phase.** Connectors are unplugged, not
snipped. The stock board must still boot the toy at the end of Phase 0 — that is the rollback path.

---

## Step 0 — Baseline the working toy (do this FIRST, before opening anything)

You cannot recognise "broken" later without a record of "working" now.

- [ ] Video every remote button, one at a time, saying the button name aloud. → `hardware/video/`
- [ ] Log each button and what it does in the table in §6 below.
- [ ] Note which motions are simultaneous multi-joint (implies coordinated control) vs
      one-joint-at-a-time (implies a simple sequencer).
- [ ] Install the phone app if available; note its name, and whether it pairs as BLE or classic BT.
- [ ] Time a full run-down from full charge → runtime in minutes. Record ambient temp.

**Why it matters:** if the stock firmware moves two joints on a coordinated trajectory, the
mechanism tolerates it. If everything is sequential, suspect weak gearing.

---

## Step 1 — Battery and power tree (closes Q4)

- [ ] Open the battery hatch. Photograph the pack, its label, and the connector.
- [ ] Measure pack voltage at rest: `______ V` → cell count = round(V / 3.7) = `______ S`
- [ ] Chemistry from the label (Li-ion / LiPo / NiMH): `______`
- [ ] Capacity from the label: `______ mAh`
- [ ] Is there a visible BMS/protection board on the pack? Y / N
- [ ] Trace the charge port: does it go to a charge IC (TP4056-class) on the main board, or
      straight to the pack? Photograph.
- [ ] Measure current draw at the pack: idle `______ mA`, walking `______ mA`, peak `______ A`
      (multimeter in series, or a USB power meter if the pack is 1S).

**GATE:** pack nominal voltage and cell count recorded. If nominal > 9 V, the MX1616S rating
inference in `HARDWARE_REFERENCE.md` §2.1 is wrong — stop and re-examine the driver ICs.

---

## Step 2 — Sensor identification — **CLOSED 2026-08-03**

Answered by the operator: the 3-wire sensor is a **potentiometer** (absolute angle) and the leg
**sweeps a limited arc**. Q1 and Q7 are settled; the discriminator test is no longer needed.

What remains from this step:

### 2a — Pot characterisation, per leg

Sensor unplugged from the main board, leg moved by hand between hard stops:

**Known:** the pot is marked **`B103` / `330°`** → 10 kΩ linear, 330° electrical travel
(`hardware/photos/mech-actuator-pot-b103-330deg.jpg`). Pin order from the photo: white / **blue
(wiper)** / red.

- [ ] **Confirm the wiper before powering anything.** Ohmmeter across white↔red should read a
      steady ~10 kΩ regardless of leg position: `______ kΩ`. Blue↔white and blue↔red should sum
      to that and change as the leg moves. If blue does *not* behave as the wiper, stop and
      re-map — an RP2350 ADC pin will not survive being wired to a supply rail.

**Logger-derived data, 2026-08-04** — from `tools/pot_logger_uno` with the toy powered (rail =
674 counts = 3.289 V, so full track = 674 counts = 330°, **0.49°/count**). These are
**hand-swept ranges, i.e. LOWER BOUNDS** — the legs were not driven to their hard stops, so the
real travel is at least this and probably more. Q5 is not closed until the stops are reached.

| Leg | Channel | Rest (counts) | Swept min–max | Range | ≥ degrees | % of 330° track |
|---|---|---|---|---|---|---|
| FL | A0 `ZQDWQ` | 356 | 224–572 | 348 | **≥ 170.5°** | 52 % |
| FR | A1 `YQDWQ` | 298 | — | 314 | ≥ 153.8° | 47 % |
| RL | A2 `ZHDWQ` | 319 | — | 393 | ≥ 192.6° | 58 % |
| RR | A3 `YHDWQ` | 350 | — | 313 | ≥ 153.4° | 47 % |

**This is much more travel than the ~90° originally assumed** — around 170° per leg. Good for
stride length and pose range. It also means the pot uses roughly half its 330° track, so
resolution per degree is as computed above.

Still to fill, by driving each leg to its mechanical stops:

| Leg | R white↔red | Counts @ stop A | Counts @ stop B | True arc travel |
|---|---|---|---|---|
| FL | | | | ° |
| FR | | | | ° |
| RL | | | | ° |
| RR | | | | ° |

- [ ] Is the wiper sweep **smooth and monotonic**, or does it jump/drop out anywhere? `______`
      Dropouts mean a worn track — that pot gets replaced before Phase 2, not debugged in software.
- [ ] Does the wiper reach the track ends, or does the mechanism stop first? (Mechanism first is
      what we want — the pot is then never driven into its own end stops.)
- [ ] **What voltage does the stock board feed the pot tops?** Measure white↔red with the toy
      powered and the sensor still plugged in. Take **three** readings, not one:
      - at rest, pack freshly charged: **3.3 V** ✔ measured 2026-08-04
      - **while a leg is driving** (this is when captures happen): `______ V`
      - pack near flat: `______ V`

      If the rail **sags under motor current**, logging it alongside the wipers stops being
      optional — the pots are ratiometric, so every wiper reading scales with it.

      If it reads **5 V**, note it loudly: our board must supply 3.3 V instead, because the
      RP2350 ADC is not 5 V tolerant. It also means the **Arduino Uno is the only logger on the
      bench that can read these wipers with no divider and no risk** — see
      `PHASE_0_SWD_AND_LOGIC.md` §2.1.

### 2b — Board-side census — **partly closed 2026-08-03**

**Closed:** 4 motors, 4 sensors, one actuator per leg. Board is `JXD-8002-Blue-YW-RV2`, dated
2024-01-19; no documentation exists online (searched). Full-board photo taken:
`hardware/photos/pcb-overview-jxd-8002-blue-yw-rv2.jpg`.

Two items remain, both quick:

- [ ] **Q8 — are the MX1616S halves paralleled?** Continuity-check each chip's two output pairs
      against the motor connector for that leg, board unpowered:
      - Both halves of one chip tie to the **same** motor → **paralleled**, i.e. the designer
        needed more than ~1.3 A per motor. Raises the floor on our driver choice.
      - Each half goes to a **different** motor, and two chips are then spare → the board is a
        platform shared with a bigger SKU (consistent with `RV2`), and says nothing about current.
      - Result: `______`
- [ ] **Q10 — read `U5`'s part marking** under the microscope, same rig as the earlier chip
      shots. Marking: `______`. `U5` is the ARM MCU with the `SWD`/`CLK`/`GND`/`VMCU` pads —
      identifying it tells us what the stock board is worth as a fallback.
- [ ] Photograph the **reverse** side of the board, whole board in frame.

---

## Step 2.5 — Foot-path calibration (closes Q9) — **the leg model**

**Closed 2026-08-03:** the leg sweeps an arc, not a crank. What replaces that question is the
measurement that *becomes* the kinematic model.

One actuator per leg means the foot travels a fixed 1-D curve, parameterised by pot angle. There
is no IK to solve — there is a table to measure. Do this once per leg, with the body clamped in
a stand, feet clear of the bench:

- [ ] Mark a reference grid on card behind the leg (10 mm squares) and clamp it so it does not move.
- [ ] Move the leg by hand in ~8–10 steps from stop to stop. At each step record:
      **pot wiper voltage (or resistance)** and **foot position (x, z)** off the grid.
- [ ] Photograph each step from a fixed camera position — the photos are the raw evidence and
      let the curve be re-fitted later without re-doing the bench work.
- [ ] Repeat for all four legs. They will not be identical; per-leg calibration is the point.

| Step | Pot (V or Ω) | Foot x (mm) | Foot z (mm) |
|---|---|---|---|
| 1 (stop A) | | | |
| … | | | |
| n (stop B) | | | |

**GATE:** four tables, one per leg, each spanning stop to stop. These become
`firmware/src/gait/legpath.c` and are covered by host-native tests.

**Why by hand and not under power:** the mechanism is characterised before anything drives it.
A calibration taken under a controller that is itself unvalidated proves nothing.

---

## Step 3 — Per-motor electrical characterisation (closes Q3)

For **each** motor, disconnected from the board, on a current-limited bench PSU set to the
measured pack voltage, limit initially 1 A:

- [ ] Free-run current, no load: `______ mA`
- [ ] Stall current — hold the output arm, ramp the limit until it stalls: `______ A`
- [ ] Does the joint move both directions on polarity reversal, smoothly, no grinding? Y / N
- [ ] Backdrivable by hand? Y / N (tells you the gear ratio class and whether it holds pose unpowered)

**GATE:** worst-case stall current across every motor recorded. This number selects the
driver in `ARCHITECTURE.md` §4. Do not order drivers before this number exists.

---

## Step 4 — Mechanical limits (closes Q5)

For each joint, with the motor unpowered, move it by hand between hard stops:

- [ ] Angular travel: `______°`, and the **pot wiper voltage at each hard stop** (these become the software limits, set 5° inside)
- [ ] Photograph each leg at both extremes and at the neutral standing pose
- [ ] Measure link lengths with calipers — cross-checks the Step 2.5 foot-path fit:
      upper link `______ mm`, lower link `______ mm`, hip offset `______ mm`
- [ ] Body dimensions and total mass with battery: `______ mm × ______ mm`, `______ g`

**Software limits will be set 5° inside every measured hard stop.** Nothing in firmware ever
commands a joint to its mechanical stop.

---

## Step 5 — Remaining silicon (closes Q6, low priority)

- [ ] Photograph and identify the 2.4 GHz RF chip and any crystal near it
- [ ] Identify the LED driver(s) and speaker amplifier
- [ ] Note any sensors: touch pads, IR receiver, microphone

Optional, non-destructive, and settles the SoC family question for good:

- [ ] Dump the `25V16066` SPI flash with a CH341A + SOIC-8 clip and `flashrom`. Keep the image
      in `hardware/dumps/`. **Read only — never write.** Confirms flash size and the JieLi part,
      and yields the bark/voice assets.

---

## 5. Motor → joint map (fill in)

Four legs, one motor each (`PROVEN` 2026-08-03). Leg naming: FL/FR/RL/RR.

Board connector names are known from silkscreen (`HARDWARE_REFERENCE.md` §2.5). Confirm the
pinyin decode once by buzzing `ZQDJ` through to the physically front-left motor — then the other
seven follow.

| Leg | Motor conn. | Pot conn. | Housing | Driver / half | Pot R | Arc travel | Free-run I | Stall I |
|---|---|---|---|---|---|---|---|---|
| FL | `ZQDJ` | `ZQDWQ` | white | | | | | |
| FR | `YQDJ` | `YQDWQ` | yellow | | | | | |
| RL | `ZHDJ` | `ZHDWQ` | white | | | | | |
| RR | `YHDJ` | `YHDWQ` | yellow | | | | | |

**No other motors exist** — the board carries exactly eight connectors, four `DJ` and four `DWQ`
(`HARDWARE_REFERENCE.md` §2.5). The remaining four H-bridge channels drive nothing on this unit;
whether they are paralleled onto the leg motors is Q8, still open.

Do not fit observations to this table — extend it if the machine disagrees.

---

## 6. Remote function map (fill in from Step 0)

| Button | Printed label | Observed behaviour | Joints involved |
|---|---|---|---|
| | | | |

---

## Phase 0 exit gate — PASS/FAIL

**PASS** requires all of:

1. ~~Q1 — sensor type~~ **CLOSED: potentiometer.**
2. ~~Q7 — crank or arc~~ **CLOSED: arc.**
3. **Q2/Q8 — motor and sensor connectors counted** on the board, and the 8-channel vs 4-leg
   discrepancy explained.
4. **Q9 — four foot-path tables**, one per leg, stop to stop.
5. Pack nominal voltage, cell count and chemistry — measured.
6. Worst-case stall current across every motor — measured.
7. Q5 — pot wiper voltage at both hard stops, per leg; sweep confirmed smooth and monotonic.
8. Full-board photos, both sides, in focus, whole board in frame.
9. **The toy still works on its stock remote.**

**FAIL** on any item still marked UNKNOWN. Do not advance to Phase 1 and do not order parts;
the Phase 1 BOM is a function of items 1 and 3.

**Rollback:** reassemble; nothing was modified.
