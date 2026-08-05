# CLAUDE.md — quaddle-robot

Execution contract. Inherits `~/.claude/CLAUDE.md`; this file overrides it where they differ.

**Project:** retrofit an APIDPOWER SMART DOG 18" toy quadruped into a programmable robot with
a custom controller and on-board real-time AI (Raspberry Pi 5 + camera + sensors).

**Named after** Petoi's *Quaddle* — the capability target. We do **not** own a Quaddle; see
`docs/HARDWARE_REFERENCE.md` §1.

---

## Read first

| Document | Contents |
|---|---|
| `docs/00_START_HERE.md` | **Onboarding for anyone new** — the five rules, safety, bench setup, first hour |
| `docs/HOW_IT_WORKS.md` | The engineering explained from first principles, on this machine's real numbers |
| `docs/NEXT_SESSION.md` | Exact state + what to do next. **Read before every session, update after.** |
| `docs/TASKS.md` | Assignable backlog: learning objective, definition of done, question closed |
| `docs/HARDWARE_REFERENCE.md` | What is PROVEN / ASSUMED / UNKNOWN about the donor toy |
| `docs/ARCHITECTURE.md` | Target system, compute split, power tree, link protocol, risks |
| `docs/RUNBOOK.md` | Step-by-step bench procedures, including the two pot wiring modes |
| `docs/PHASE_0_TEARDOWN.md` | The measurement checklist that currently blocks everything |
| `docs/PHASE_0_SWD_AND_LOGIC.md` | Non-destructive probing of the stock board: SWD on `U5`, logic capture |
| `docs/GLOSSARY.md` | Terms, the board's Chinese silkscreen decode, key numbers |

---

## Guardrails

1. **Evidence Discipline applies with full force here.** This is undocumented Chinese toy
   hardware with no datasheet, no schematic and no manual. The physical unit is the only
   authority. Label every hardware claim `PROVEN` / `ASSUMED` / `UNKNOWN` and never let an
   inference from a photo be recorded as a measurement.
2. **The stock PCB is never cut, desoldered, erased or reflashed.** It is the rollback path and
   the only working reference for what the mechanism can do. Retrofit hardware is additive.
   **Read-only probing is permitted and encouraged** — SWD attach-without-halt, logic capture,
   SPI-flash dump. **Writing is not**, and that includes every tool's "unlock" / "remove read
   protection" / "auto mass-erase on connect" convenience. See `docs/PHASE_0_SWD_AND_LOGIC.md` §0.
3. **No motor is ever commanded without a current limit and a stall timeout**, from Phase 1
   onward. Toy gearboxes are nylon and will strip.
4. **Body on a stand, feet off the ground**, for all bench work through Phase 2.
5. **No parts are ordered before the measurement that selects them exists.** The driver IC is
   a function of the measured stall current; the AI accelerator is a function of the measured
   CPU inference rate. Lead time from the China warehouse is not a reason to guess.
6. **The Pi never sends raw PWM.** Intent only. Link loss must leave the robot safe with the
   Pi absent — see `docs/ARCHITECTURE.md` §2.
7. Li-ion pack of unknown provenance. Never leave it charging unattended; never bench-run from
   the pack with the charger attached until the charge topology is understood.

---

## Phases

| # | Name | Gate |
|---|---|---|
| **0** | Teardown & characterisation | Every UNKNOWN in `HARDWARE_REFERENCE.md` §3 measured; toy still works on its stock remote. Full gate in `docs/PHASE_0_TEARDOWN.md`. |
| 1 | Single joint, our electronics | One joint driven both directions from RP2350 + chosen driver. Current stays under limit; no thermal rise after 60 s of duty cycling; stall detection fires within 200 ms of a held arm. |
| 2 | 4-channel drive + position loop | All 4 legs hold a commanded angle to ±1° with no visible hunting. E-stop kills the motor rail in < 50 ms. Software limits verified 5° inside every hard stop, enforced below the trajectory generator. |
| 3 | Gait engine + IMU | Walks 2 m straight (drift < 20 cm), turns 90° ± 10°, recovers to stand from a tipped start. Gait math passes host-native unit tests before it touches hardware. |
| 4 | Custom handheld remote | ESP32-S3 handheld drives the robot. Teleop end-to-end latency < 50 ms measured with echoed sequence numbers. Link loss → stand-and-disable within 200 ms, verified by pulling power from the remote. |
| 5 | Pi 5 + camera + AI | Person detection ≥ 10 fps sustained with the gait loop running. Follow-me holds 1.5 m ± 0.3 m for 60 s. |
| 5b | Browser app | WebRTC video with detection overlay + telemetry + teleop, glass-to-glass latency < 300 ms on LAN. Control handover between remote and browser always passes through a stand. |
| 6 | Behaviours & autonomy | Scoped after Phase 5. |

No phase advances without its gate green. On FAIL: revert, log in State below, stop.

---

## Conventions

- Firmware: C, Pico SDK, CMake. Host-native unit tests for all pure logic (gait math, IK,
  COBS/CRC codec) — never debug math on hardware.
- Host: Python 3.12, `uv`, `ruff`, `mypy --strict`, `pytest`.
- Remote: ESP-IDF.
- Commits: Conventional Commits, phase ID in the body, e.g. `feat(joint): pot feedback ADC (P2-03)`.
- Bench measurements go in the tables in `docs/PHASE_0_TEARDOWN.md`, not in chat.
- Photos → `hardware/photos/`, descriptive filenames. Flash dumps → `hardware/dumps/`, read-only.

---

## State

<!-- append; newest last -->

- **2026-08-03** — Project opened. Identified from six microscope photos: ≥4× `MX1616S` dual
  H-bridge (U4/U6/U7/U10) → the legs are **brushed DC motors, not servos**; JieLi
  `AF24C105159-65E4` TSSOP-28 SoC (closed toolchain → brain transplant, not a firmware mod);
  `25V16066` SPI NOR flash. Confirmed the toy is an APIDPOWER "SMART DOG" and that the
  YouTube reference is Petoi Quaddle, a different machine. Architecture drafted (Pi 5 cortex +
  RP2350 spinal cord). **Blocked on Phase 0.**
- **2026-08-03** — Operator opened the body (photos `mech-leg-pair-*.jpg`). **`PROVEN`: 4 legs,
  one 2-wire brushed DC motor each, plus one 3-wire sensor per leg on a small green PCB at the
  gearbox.** So it is a **1-DOF-per-leg machine**, not the 8-motor/2-DOF layout the four
  MX1616S chips implied — Quaddle-class tricks are off the table, walk/turn/pose are not.
  Original Q1 (wires per motor) closed. Two questions now gate the firmware architecture:
  **Q1 — what the 3-wire sensor actually is** (pot / Hall / opto all use 3 wires), and
  **Q7 — whether the leg output cranks continuously or sweeps an arc.** Docs updated:
  `ARCHITECTURE.md` §3b now carries both motion branches; `PHASE_0_TEARDOWN.md` Step 2 is the
  discriminator test. Also open: **Q8** — 8 driver channels exist, only 4 legs found.
- **2026-08-03** — **Q1 and Q7 closed, best case both.** The sensor is a **potentiometer**
  (absolute angle, no homing) and the leg output **sweeps a limited arc**. So each leg is a
  servo built from discrete parts: motor + gearbox + output pot, loop closed in firmware.
  Motion architecture **LOCKED to position control** — the crank/phase-lock branch is deleted,
  not deferred (`ARCHITECTURE.md` §3b). Unlocked: static poses, and body height/pitch/roll
  while standing (4 leg positions → 3 body DOF). Still open before Phase 1: **Q2/Q8** (count
  the motor and sensor connectors on the board — 8 driver channels vs 4 reported legs is not
  yet reconciled, and "8" would mean 2 DOF/leg), **Q3** stall current, **Q4** battery,
  **Q5** pot range at both stops, **Q9** foot-path calibration per leg.
- **2026-08-03** — Pot photographed and identified: **`B103` / `330°`** → 10 kΩ linear, 330°
  electrical travel, mounted in the gearbox housing reading the **output** shaft
  (`hardware/photos/mech-actuator-pot-b103-330deg.jpg`). Pin order white / **blue = wiper** /
  red — this corrects the earlier colour guess, so wire from the ohmmeter, not the colours.
  Front-end design now settled (`ARCHITECTURE.md` §3b): straight into the RP2350 ADC, no buffer
  (2.5 kΩ worst-case source impedance), **ratiometric off the same 3.3 V that feeds `ADC_VREF`**,
  1.3 mA total for four pots, 0.081°/count at 12-bit. **Hard rule: 3.3 V only — the RP2350 ADC
  is not 5 V tolerant**, so measure what the stock board feeds the pot tops before reusing any
  harness. Because the pot reads the output shaft, backlash sits inside the loop: a modest PID
  works, but slew and deadband limits are mandatory.
- **2026-08-03** — First whole-board photo. Board is **`JXD-8002-Blue-YW-RV2`, dated 2024-01-19**;
  searched, **no documentation exists online** — do not re-search. **Q2 closed: exactly 4 motors
  and 4 sensors**, one per leg, no head/tail/mouth motors. New: **`U5`, a TSSOP-28 with
  `SWD`/`CLK`/`GND`/`VMCU` test pads** — so the stock design is *already* a two-brain split
  (JieLi for BT/audio + a separate ARM MCU for motion), mirroring our architecture. That adds
  **Option D** (reflash the stock MCU) to `ARCHITECTURE.md` §1, **excluded** by the
  reversibility decision: an unknown read-protected MCU can only be flashed via mass erase,
  which destroys the stock firmware permanently. Remaining quick checks: **Q8** — continuity-test
  whether each MX1616S has both halves paralleled onto one motor (implies >1.3 A motors) or the
  spare channels belong to a bigger SKU; **Q10** — read `U5`'s marking.
- **2026-08-03** — Added `docs/PHASE_0_SWD_AND_LOGIC.md` for read-only interrogation of the stock
  board with the NanoDAP and Saleae already on the bench. Guardrail 2 clarified: reading is
  encouraged, writing/erasing is forbidden — an unlock-on-connect would destroy the rollback
  path. Two hazards called out: (a) tools that auto-mass-erase a read-protected part, (b)
  **halting the core with the H-bridges enabled**, which freezes a motor against a stop and
  stalls it — hence `--connect=attach`, motor rail off, legs unloaded. No `NRST` pad exists, so
  connect-under-reset is unavailable; if the stock firmware repurposes the SWD pins at boot,
  a failed connect is expected and is not a fault. **The higher-value half is the Saleae work:**
  capture the four pot wipers while the toy runs each stock behaviour → the working joint
  trajectories, measured rather than derived, which combined with Q9 turns Phase 3 into
  replication against a known-good reference. Also extract PWM frequency, drive scheme and
  braking behaviour from the MX1616S inputs.
- **2026-08-03** — Connector-row photos (`pcb-connectors-*.jpg`). **The board labels its own
  wiring map.** Eight connectors, nothing else: `ZQDJ`/`YQDJ`/`ZHDJ`/`YHDJ` (电机 = motor) and
  `ZQDWQ`/`YQDWQ`/`ZHDWQ`/`YHDWQ` (电位器 = potentiometer), where Z/Y = 左/右 left/right and
  Q/H = 前/后 front/rear. Mapped to our FL/FR/RL/RR in `HARDWARE_REFERENCE.md` §2.5. **Housing
  colour is a left/right key: white = left, yellow = right** — keep this convention in our loom.
  `DWQ` is the manufacturer's own word for potentiometer, so **Q1 is now settled three ways**
  (operator ohmmeter + `B103` marking + silkscreen), and **Q2 is closed by board evidence**:
  exactly 4 motors and 4 pots, no head/tail/mouth actuators. Also readable: `SPK`, `LED`, `KG1`
  (switch), `+BAT`/`BAT-`, `CD` (charge?), `F1`, `C26–C28`, and an **unpopulated `Gun` accessory
  footprint** — soft evidence this is a platform PCB shared across SKUs, which supports the
  "spare H-bridge channels belong to another variant" reading of Q8. Saleae tap points in
  `PHASE_0_SWD_AND_LOGIC.md` §2.1 now name the exact connectors; no tracing needed.
- **2026-08-03** — **Bench instrument checked over USB — it is not a modern Saleae.** `ioreg`
  gives `idVendor 0x0925 / idProduct 0x3881`, **no USB product or vendor string, iSerialNumber 0**
  (`PROVEN`). `ASSUMED`: an FX2LP/CY7C68013A clone of the original 2008 Saleae Logic — 8 digital
  channels, 24 MS/s, **no analog inputs**; genuine analog-capable Saleae units enumerate under
  `0x21A9` and report string descriptors. Two consequences: **(1)** Saleae Logic 2 will not see
  it — use **sigrok** (`brew install sigrok-cli`, formula verified available; the `fx2lafw`
  firmware blob is *not* in Homebrew and must come from sigrok.org; no PulseView cask exists).
  **(2)** The pot-wiper capture as originally planned is impossible — it needs analog. Plan split
  in `PHASE_0_SWD_AND_LOGIC.md` §2: the clone still does the MX1616S digital work well (PWM
  frequency, drive scheme, brake-vs-coast), while the pot trajectories need a **4-channel ADC
  logger — ADS1115 + Pico at 5 V**, whose ±6.144 V range tolerates the stock pot rail whatever it
  measures. That logger is **not a detour**: it is the Phase 1/2 feedback front end built early,
  same harness and same CSV path. Confirm with `sigrok-cli --scan` → expect `fx2lafw`.
- **2026-08-04** — Measured: **pot supply rail = 3.3 V, `VMCU` = 3.3 V** (`PROVEN`). Three
  consequences. **(1)** The stock board already feeds the pots from the MCU's own regulated rail
  — the ratiometric arrangement we designed independently; our board mirrors it, and the front
  end needs **no divider, no level shift, no buffer**. **(2) SWD is cleared electrically** — a
  3.3 V CMSIS-DAP probe is level-compatible with `VMCU`, removing the main unknown from
  `PHASE_0_SWD_AND_LOGIC.md` §1.2. **(3) Logger decided: Arduino Uno**, tethered, at its default
  5 V reference. Channel count beat resolution — the Uno's **six** ADC channels take 4 pots + the
  rail + a spare, where an ADS1115 has 4 and a Pico 2 W only 3 (`GPIO29` is shared with VSYS and
  the CYW43 radio). A 0–3.3 V wiper spans 676 counts of the pot's 330°, so ~185 counts across a
  ~90° arc ≈ **0.5°/count**, well below gearbox backlash. **`AREF` is deliberately left alone**:
  tying it to 3.3 V gains only ~276 counts and risks shorting the internal reference if
  `analogReference(EXTERNAL)` is not called before the first `analogRead()`. Bench inventory
  recorded in `ARCHITECTURE.md` §3 — **Pi 5 and Pico 2 W (RP2350) are both already in hand**, so
  neither compute choice needs purchasing.
- **2026-08-04** — Built `tools/pot_logger_uno/` (Arduino sketch) + `tools/capture.py` (host).
  Streams `t_us,fl,fr,rl,rr,rail,mark` CSV at 200 Hz over 500 kbaud; verified with
  `arduino-cli compile --fqbn arduino:avr:uno` (11% flash, 10% RAM). Design points worth keeping:
  ADC prescaler left at the stock 128 (125 kHz, inside the 50–200 kHz accuracy window) because
  linearity is the point and there is no time pressure at 200 Hz; **one conversion discarded after
  each mux change** so S/H residue from the previous channel cannot masquerade as motion;
  timestamp taken **before** the serial print so TX blocking cannot leak into the timing; missed
  deadlines counted as `overruns` and reported in the trailer, since a capture with unknown
  dropped samples is useless for measuring gait period. The `r` command (single sample on demand)
  makes the same rig serve Step 2a (pot range at stops) and Step 2.5 (foot-path calibration).
- **2026-08-04** — **Logger uploaded and verified on hardware.** Uno enumerates as
  `0x2341:0x0043` "Generic CDC" (clone descriptor), serial `9543731333535130E1A1`, on
  `/dev/cu.usbmodem1301`. Measured with inputs floating: **201 samples/s, period mean 5000 µs,
  min 4980 / max 5012 (±0.24 % jitter), 0 overruns** — the 200 Hz budget has ~4 ms of headroom.
  Note 500000 baud divides exactly from 16 MHz in U2X mode on both the 16U2 and the 328P, so
  there is no framing error at that rate. Acceptance check added to the tool README: once wired,
  the `rail` channel must read **≈676 counts** (3.3 V at the 5 V reference) and each leg column
  must respond to that leg alone — catches cross-plugged taps before any captures are taken.
- **2026-08-04** — Wiring verified live: `rail` = **674.6 counts = 3.294 V** against a predicted
  676 / 3.30 V, all four legs mid-track and distinct, 3–5 counts p-p noise, 0 overruns. Scale on
  the Uno is **1 count ≈ 0.49°** (rail = full 330° of track), so its ±2-count noise is ≈ ±1° —
  a property of 10-bit-from-5 V, not of the design; the RP2350 at 12-bit/3.3 V gives 0.081°/count.
  Resting angles pair on the **diagonals** (FL 175° ≈ RR 172°, FR 155° ≈ RL 158°) — consistent
  with the toy parking in a trot stance; hypothesis to confirm from the walk captures, not a
  finding. **Then all five channels went to exactly 0 — the toy had been powered off.** The pots
  have no supply of their own. This split the procedure into **two wiring modes**, now documented
  in `tools/pot_logger_uno/README.md`: **Mode A** behaviour capture, toy ON, stock board powers
  the pots, we only read; **Mode B** hand calibration, toy OFF, **`DWQ` connectors unplugged from
  the stock board first**, pots powered from the Uno's `3V3` (1.3 mA of a ~50 mA budget). Mode B
  without unplugging would back-feed the board's 3.3 V net and parasitically part-power the JieLi
  and `U5`.
- **2026-08-04** — **Channel map verified.** A first per-leg run showed the taps permuted; they
  were corrected **at the wires, not in software**, so no remap constant exists in the codebase.
  Confirmation run: front-left alone moved → **A0 swung 348 counts (170.5°) while the other three
  stayed ≤ 10 counts — 34.8× separation, and no measurable cross-talk**, which also validates the
  mux-settling discard and the single-point ground. A0 = `ZQDWQ` = front-left is `PROVEN`; A1–A3
  are `ASSUMED` from the corrected tap order and have not been individually driven. **Travel is
  far larger than assumed: ~170° per leg, not ~90°** — hand-swept lower bounds are FL ≥ 170.5°,
  RL ≥ 192.6°, FR ≥ 153.8°, RR ≥ 153.4°, so the pot uses roughly half its 330° track. Scale on
  the Uno rig is **0.49°/count** (rail = 674 counts = full track). Q5 stays open until the legs
  are driven to their mechanical stops — these are hand sweeps, not stops. Added `tools/live.py`,
  a 10 Hz refreshing readout with a `moved(3s)` column, so leg identity can be checked at the
  bench without a round trip.
- **2026-08-04** — **Handover package written**, project reframed as a team learning module.
  Added `docs/00_START_HERE.md` (onboarding, five rules, safety, first hour),
  `docs/HOW_IT_WORKS.md` (the engineering from first principles — servo loop, backlash inside the
  loop, H-bridge/PWM/brake-vs-coast, PID guards, why no IK on a 1-DOF leg, gait phase offsets,
  4-inputs-to-3-body-DOF, the two-brain split and the intent rule, the measurement chain — each
  section with "Check yourself" questions), `docs/TASKS.md` (backlog P0-01…P5b-02, each with a
  learning objective, definition of done and the open question it closes; four marked
  `good first task`), `docs/NEXT_SESSION.md` (bench state, verified/open tables, ordered do-next,
  explicit do-NOT list), `docs/RUNBOOK.md` (both wiring modes, grounding, logger bring-up,
  capture, sigrok, SWD, restore-to-stock) and `docs/GLOSSARY.md`. README rewritten as the hub.
  Design intent: a new engineer can go from zero to a useful Phase 0 task without a handover
  conversation, and every claim they inherit carries its evidence label.
