# Tasks

The assignable backlog. Every task has a **learning objective**, a **definition of done**, and
where relevant the **open question** it closes.

**Rules for picking up a task**

1. Check prerequisites are actually done, not just claimed.
2. Claim it by putting your name in the Owner column and committing that change.
3. Record measurements in the tables in `PHASE_0_TEARDOWN.md`, not in chat.
4. Append a dated entry to the **State** block in `CLAUDE.md` when you finish — including *why*
   you did what you did.
5. If a task turns out to be wrong or impossible, say so and change it. The backlog is not sacred.

Sizes: **S** ≈ half a day · **M** ≈ 1–2 days · **L** ≈ a week.

---

## Phase 0 — Teardown & characterisation  ⟵ *current phase, blocks everything*

Phase 0 is not paperwork. Until it is done, we cannot choose a motor driver, we cannot size a
battery, and we cannot write a leg model. **No parts get ordered from Phase 0 guesses.**

| ID | Task | Size | Owner | Prereq | Closes |
|---|---|---|---|---|---|
| P0-01 | Baseline the working toy on video | S | | — | — |
| P0-02 | Battery and charge topology | S | | — | Q4 |
| P0-03 | Are the MX1616S halves paralleled? | S | | — | Q8 |
| P0-04 | Read `U5`'s part marking | S | | — | Q10 |
| P0-05 | Pot range at the true mechanical stops | M | | P0-01 | Q5 |
| P0-06 | Per-motor stall current | M | | P0-02 | Q3 |
| P0-07 | Foot-path calibration, all four legs | L | | P0-05 | Q9 |
| P0-08 | Capture the stock motion library | M | | — | — |
| P0-09 | Extract PWM scheme from the driver inputs | M | | P0-08 | — |
| P0-10 | SWD interrogation of `U5` | M | | P0-04 | Q10 |
| P0-11 | Analysis script for captures | M | | P0-08 | — |

---

### P0-01 — Baseline the working toy on video · `good first task`

**Learning objective:** you cannot recognise "broken" later without a record of "working" now.
This is the cheapest insurance in the project.

**Do:** Video every remote button, one at a time, saying the button name aloud. Log each into the
remote function table in `PHASE_0_TEARDOWN.md` §6. Note whether motions are coordinated
multi-leg or one-leg-at-a-time. Install the phone app if it exists; note BLE vs classic BT. Time
a full run-down from a full charge.

**Done when:** every button on the remote has a row in the table and a video file in
`hardware/video/`.

**Watch for:** if the stock firmware moves several legs on a coordinated trajectory, the
mechanism tolerates coordinated motion. If everything is strictly sequential, suspect weak
gearing and be gentler in Phase 2.

---

### P0-02 — Battery and charge topology · `good first task`

**Learning objective:** the power tree is the thing that constrains every later choice, and
Li-ion of unknown provenance is the biggest safety item on the project.

**Do:** `PHASE_0_TEARDOWN.md` Step 1. Pack voltage at rest, cell count, chemistry, capacity,
whether the pack carries a protection board. Trace the charge port — does it reach a charge IC on
the main board, or go straight to the cells? Photograph everything. Measure current at the pack:
idle, walking, peak.

**Done when:** Q4 has measured values, not label-reading alone, and the charge path is drawn.

**Safety:** never charge unattended. If the pack is swollen, hot, or has no protection board,
stop and escalate before doing anything else.

---

### P0-03 — Are the MX1616S halves paralleled? · `good first task`

**Learning objective:** how a design choice you can see on a board tells you something about a
component you cannot measure yet.

**Do:** Board unpowered. Continuity-check each chip's two output pairs against the motor
connector for that leg.

- Both halves of one chip → the **same** motor = **paralleled**. The designer needed more than
  ~1.3 A per motor, which raises the floor on our driver selection before Q3 is even measured.
- Each half → a **different** motor, two chips spare = platform board shared with a larger SKU
  (consistent with the `RV2` naming and the unpopulated `Gun` footprint). Tells us nothing about
  current.

**Done when:** the result is recorded in `PHASE_0_TEARDOWN.md` §2b with which pins were probed.

---

### P0-04 — Read `U5`'s part marking · `good first task`

**Learning objective:** chip identification from markings; how much of a board's design you can
infer from one part number.

**Do:** Microscope photo of `U5` (TSSOP-28, mid-board, next to the `SWD`/`CLK`/`GND`/`VMCU`
pads). File as `hardware/photos/pcb-u5-marking.jpg`. Search the marking. Note that JieLi-style
obfuscated markings exist — if it looks like nonsense, it may be deliberate.

**Done when:** the marking is legible in a photo and recorded in `HARDWARE_REFERENCE.md` §2.1b,
with whatever identification it supports labelled `PROVEN` or `ASSUMED` accordingly.

---

### P0-05 — Pot range at the true mechanical stops

**Learning objective:** the difference between a range you happened to observe and a limit that
actually exists — and why software limits must sit inside measured stops.

**Do:** Mode B wiring (`tools/pot_logger_uno/README.md`): toy **off**, `DWQ` connectors
**unplugged from the stock board**, pots powered from the Uno's `3V3`. Move each leg gently to
each hard stop by hand, press `r`, record.

Current data are **hand-swept lower bounds only** — FL ≥ 170.5°, RL ≥ 192.6°, FR ≥ 153.8°,
RR ≥ 153.4°. The stops have not been reached.

**Done when:** the stops table in `PHASE_0_TEARDOWN.md` §2a has counts at both stops for all four
legs, plus a note on whether the sweep is smooth and monotonic or has dropouts.

**Watch for:** a scratchy or dropping-out track means a worn pot. Replace it — do not try to
filter around it. `B103` 330° pots are a commodity part.

---

### P0-06 — Per-motor stall current

**Learning objective:** how a single measurement selects a component, and why locked-rotor
current is the number that matters rather than rated current.

**Do:** `PHASE_0_TEARDOWN.md` Step 3. Each motor disconnected, on a current-limited bench supply
at the measured pack voltage, limit starting at 1 A. Record free-run current and stall current.
Note whether each joint is backdrivable by hand.

**Done when:** worst-case stall across all four motors is recorded. **This number selects the
driver IC** (`ARCHITECTURE.md` §4) and nothing gets ordered before it exists.

**Safety:** stall a motor only briefly. Ramp the limit up, do not start high.

---

### P0-07 — Foot-path calibration, all four legs

**Learning objective:** this is the leg model. On a 1-DOF leg there is no inverse kinematics —
there is a curve you measure. Everything downstream depends on it.

**Do:** `PHASE_0_TEARDOWN.md` Step 2.5. Body clamped, feet clear. Graph-paper grid clamped behind
the leg. Step the leg by hand from stop to stop in 8–10 steps; at each, record the pot reading
(`r` command) and the foot (x, z) off the grid. Photograph each step from a fixed camera
position. Repeat per leg — they will differ.

**Done when:** four tables spanning stop to stop, plus the photo series, committed. These become
`firmware/src/gait/legpath.c`.

**Watch for:** do this by hand, unpowered. A calibration taken under a controller that is itself
unvalidated proves nothing.

---

### P0-08 — Capture the stock motion library

**Learning objective:** measuring a working system beats deriving one. The toy already walks; its
pot traces *are* the joint trajectories.

**Do:** Mode A wiring, toy **on**. `uv run --with pyserial tools/capture.py <name>` per remote
button. Poses on a stand, walking on the floor — record which. Press the marker button as you
press the remote. Cover at minimum: sit, stand, lie, paw, walk forward, walk backward, turn left,
turn right.

**Done when:** one CSV per behaviour in `hardware/captures/`, each with `overruns=0` in its
trailer, and the capture-list table in the tool README filled in.

**Reject a capture with non-zero overruns** — samples were dropped and the timing has gaps, so
period measurement from it is worthless.

---

### P0-09 — Extract the PWM scheme from the driver inputs

**Learning objective:** reading a design decision off a working board with a logic analyser.

**Do:** `PHASE_0_SWD_AND_LOGIC.md` §2.2. Our analyser is an FX2LP clone — `0x0925:0x3881`,
digital only, no analog — so use **sigrok**, not Saleae Logic 2. Probe one MX1616S's inputs.

Extract: PWM carrier frequency · sign-magnitude vs lock-antiphase · brake vs coast at the end of
a move · the duty range actually used.

**Done when:** all four recorded in `PHASE_0_TEARDOWN.md`, with the capture file kept.

**Setup note:** `brew install sigrok-cli`, then fetch `sigrok-firmware-fx2lafw-bin` from
sigrok.org — the firmware blob is **not** in Homebrew and libsigrok uploads it at runtime.

---

### P0-10 — SWD interrogation of `U5`

**Learning objective:** what a debug port tells you about a chip you have no documentation for,
and how to probe hardware without damaging it.

**Do:** `PHASE_0_SWD_AND_LOGIC.md` §1. `VMCU` is measured at 3.3 V so a 3.3 V CMSIS-DAP probe is
level-compatible. Attach **without halting** (`--connect=attach`), read DPIDR, CPUID at
`0xE000ED00`, and the ROM table.

**Done when:** either a DPIDR/CPUID reading is recorded, **or** a failed connect is documented
with the reason — **and the chip is not erased.**

**⚠ Read §0 of that document first and mean it.** Never issue an erase, unlock, or mass-erase,
whatever a tool offers. There is no `NRST` pad, so connect-under-reset is unavailable; if the
stock firmware repurposes the SWD pins at boot, a failed connect is the expected result and is
not a fault. And do not halt the core with the H-bridges live — a frozen driver input stalls a
motor against a stop.

---

### P0-11 — Analysis script for captures

**Learning objective:** turning raw logs into the numbers that drive a design decision.

**Do:** `host/analysis/` in Python 3.12 with `uv`. Load a capture CSV, skip `#` comments,
normalise each channel against the `rail` column, convert counts to degrees, and report:

- gait period and duty factor per leg
- phase offsets between legs → which gait the toy actually uses
- per-leg angular range used by each behaviour
- a plot of all four trajectories against time

**Done when:** running it on `walk_forward.csv` prints the gait period and four phase offsets,
and `pytest` covers the counts→degrees and phase-offset maths with synthetic data.

**First question to answer with it:** the resting angles pair on the diagonals (FL ≈ RR,
FR ≈ RL). Is that a real trot, or just resting geometry?

---

## Phase 1 — Single joint, our electronics

*Blocked on P0-06 (driver selection) and P0-05 (software limits).*

| ID | Task | Size | Prereq |
|---|---|---|---|
| P1-01 | Select and order the driver IC from the measured stall current | S | P0-06 |
| P1-02 | Breadboard: RP2350 + one driver + one leg, on the bench | M | P1-01 |
| P1-03 | PWM output stage in Pico SDK C, both directions | M | P1-02 |
| P1-04 | ADC front end: pot read, oversampled, counts → degrees | M | P1-02 |
| P1-05 | Current sense + stall detection | M | P1-03 |

**Phase 1 gate:** one joint driven both directions from the RP2350. Current stays under the
limit. No thermal rise after 60 s of duty cycling. Stall detection fires within 200 ms of a held
arm.

---

## Phase 2 — Four channels + position loop

| ID | Task | Size | Prereq |
|---|---|---|---|
| P2-01 | Four-channel driver board, current sense per channel | L | P1-05 |
| P2-02 | Position loop: PID + slew limit + deadband + integral clamp | L | P1-04 |
| P2-03 | Software angle limits below the trajectory generator | M | P0-05 |
| P2-04 | E-stop cutting the motor rail | M | P2-01 |
| P2-05 | Power tree: buck, pack monitoring, brownout isolation | L | P0-02 |

**Phase 2 gate:** all four legs hold a commanded angle to ±1° with no visible hunting. E-stop
kills the motor rail in < 50 ms. Software limits verified 5° inside every hard stop.

---

## Phase 3 — Gait engine + IMU

| ID | Task | Size | Prereq |
|---|---|---|---|
| P3-01 | `legpath()` from the P0-07 tables, with host-native tests | M | P0-07 |
| P3-02 | Gait generator: phase offsets, duty factor, host-tested | L | P3-01 |
| P3-03 | Replicate the stock walk from the P0-08 captures | M | P3-02, P0-11 |
| P3-04 | IMU integration, body attitude estimate | M | P2-02 |
| P3-05 | Stand-up-from-tipped recovery | M | P3-04 |

**Phase 3 gate:** walks 2 m straight with under 20 cm drift, turns 90° ± 10°, recovers to a stand
from a tipped start. **Gait maths passes host-native unit tests before it touches hardware.**

---

## Phase 4–5b — Remote, Pi, browser

Scoped once Phase 3 is green. Outline in `ARCHITECTURE.md` §7 and the phase table in `CLAUDE.md`.

| ID | Task | Prereq |
|---|---|---|
| P4-01 | COBS + CRC-16 link codec, host-native tests both ends | P2-02 |
| P4-02 | ESP32-S3 handheld: two sticks, OLED, ESP-NOW | P4-01 |
| P4-03 | Deadman: link loss → stand and disable within 200 ms | P4-01 |
| P5-01 | Pi 5 + Camera Module 3, measure CPU inference rate | P3-03 |
| P5-02 | Decide on the AI accelerator **from that measurement** | P5-01 |
| P5-03 | Follow-me behaviour with its own deadband | P5-01 |
| P5b-01 | FastAPI + WebRTC video, telemetry, teleop | P5-01 |
| P5b-02 | Control handover between remote and browser, always via a stand | P4-02, P5b-01 |

---

## Standing tasks

| ID | Task | Notes |
|---|---|---|
| STD-01 | Source a second identical toy as a parts donor | **Do this while stock lasts.** A stripped gearbox is the most likely way this project dies, and the unit is already sold out at the original reseller. |
| STD-02 | Keep the State block in `CLAUDE.md` current | Every session. The reasoning is the expensive part. |
| STD-03 | Re-verify the toy still works on its stock remote | After every session that touched the harness. It is the Phase 0 exit gate and it cannot be recovered once lost. |
