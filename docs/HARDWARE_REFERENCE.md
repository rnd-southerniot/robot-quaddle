# Hardware Reference — APIDPOWER "SMART DOG" 18-inch toy quadruped

**Status:** partial. Everything below is labelled `PROVEN` / `ASSUMED` / `UNKNOWN` per the
Evidence Discipline rule. Do not plan against an `ASSUMED` value without flagging it.

Last updated: 2026-08-03

---

## 1. The unit

| Item | Value | Evidence |
|---|---|---|
| Retail name | APIDPOWER **SMART DOG** — "Multifunctional Toy", ages 3+ | `PROVEN` — `hardware/photos/retail-box-apidpower-smart-dog.jpg` (box art) |
| Reseller listing | horaeplay.com "Robot dog 18inch", $99, sold out | `PROVEN` — Shopify product JSON; listing carries **no** spec text at all |
| Form factor | Unitree Go1/Spot-style white quadruped, 2 visible joints per leg | `ASSUMED` — from box photo silhouette only, not measured |
| Control inputs shipped | (a) gamepad-style RF remote, ~14 buttons + D-pad, (b) phone app ("THERE IS AN APP MOBILE PHONE CONTROL") | `PROVEN` — box art |
| Manufacturer datasheet / manual | none found online | `UNKNOWN` — the physical unit is the only authority |

> The YouTube link in the original brief (`0Z7ZijSYeV8`) is **Petoi Quaddle**, a *different*
> product — an open-source 4-servo desk quadruped running OpenCat firmware. It is the
> capability target for this project, not the hardware we own. Its firmware
> (github.com/PetoiCamp/OpenCat-Quadruped-Robot) is a useful gait reference, but it assumes
> RC servos and will not port directly to DC-motor legs.

---

## 2. Electronics and actuators

Six microscope photos of one board (all timestamped within 30 s of each other) plus two
interior photos of the leg mechanism.

### 2.1 Motor drive — `MX1616S` × ≥4

| Field | Value | Evidence |
|---|---|---|
| Marking | `MX1616S` + `444AAK` + slanted-Z vendor logo, SOP-16 | `PROVEN` — legible in 4 photos |
| Designators seen | **U4, U6, U7, U10** | `PROVEN` — silkscreen in `pcb-u7-u6-u4-mx1616s.jpg`, `pcb-u10-mx1616s.jpg` |
| Part | Mixic (Sinotech) MX1616 family — **dual full H-bridge, brushed DC** | `ASSUMED` — distributor pages, no vendor PDF read yet |
| Ratings | ~2–9.6 V supply, ~1.3 A/ch continuous, ~2.5 A peak, thermal shutdown | `ASSUMED` — distributor copy, unverified against a datasheet |
| Available channels | 4 chips × 2 = **8 brushed DC channels** | `ASSUMED` — from the dual-bridge part inference |
| Channels actually used by legs | **4** — the machine has exactly **4 motors and 4 sensors**, total | `PROVEN` — operator count, 2026-08-03 |
| Remaining 4 channels | **spare.** Two explanations remain, and they differ in consequence | `UNKNOWN` — see Q8 |

**This is the single most important finding.** H-bridges mean the legs are driven by
**brushed DC gearmotors**, *not* RC servos. Every servo-based quadruped tutorial, and OpenCat
itself, is therefore inapplicable to the drive layer without change.

### 2.1b Board identity and topology

Source: `hardware/photos/pcb-overview-jxd-8002-blue-yw-rv2.jpg` (first whole-board photo).

| Field | Value | Evidence |
|---|---|---|
| Board silkscreen | **`JXD-8002-Blue-YW-RV2`**, dated **`2024 1 19`** | `PROVEN` — legible on the board |
| Documentation | **none** — web searched 2026-08-03 for `JXD-8002` / `JXD 8002` + robot dog, nothing relevant returned | `PROVEN` (that the search was run and came up empty). Do not re-run it; the physical board is the only source. |
| Naming | `Blue` and `RV2` suggest a **colour variant of a revision-2 platform board**, i.e. one PCB serving several SKUs | `ASSUMED` — inference from the naming pattern |
| Motor drivers | 4 × SOP-16 in a row along the top edge | `PROVEN` |
| **Second MCU — `U5`** | TSSOP-28, mid-board, with adjacent test pads silkscreened **`SWD`**, **`CLK`**, **`GND`**, **`VMCU`** | `PROVEN` — labels legible |
| U5 identity | `SWD` + `CLK` is the ARM Cortex-M serial-wire debug port → **U5 is an ARM microcontroller, distinct from the JieLi SoC** | `ASSUMED` — from the pad labels alone. **U5's own marking has not been read** — needs a microscope shot. |
| Power section | Bottom-right: `U8`, `U9`, `D2`, `D3`, `Q5`, `Q6`, a wound component, `VMCU` rail | `PROVEN` — designators legible; function not traced |

**The stock design is the same two-brain split we independently arrived at.** A JieLi SoC for
Bluetooth, app and audio; a separate ARM MCU (`U5`) reading four pots and driving four
H-bridges for motion. That is a useful validation of `ARCHITECTURE.md` §2 — and it means the
existing harness, connectors and driver stage are already wired the way our RP2350 would want
them.

### 2.2 Main SoC — JieLi

| Field | Value | Evidence |
|---|---|---|
| Marking | `AF24C105159-65E4` + slanted `JL` logo, TSSOP-28 | `PROVEN` — `pcb-jieli-af24c105159-65e4.jpg` |
| Vendor | Zhuhai JieLi Technology (珠海杰理) | `PROVEN` — JL logo is JieLi's |
| Part | JieLi markings are deliberately obfuscated; the real part is encoded after the dash. `-65E4` decodes toward the **AC69xx BT-audio SoC family** | `ASSUMED` — per kagaimiq.github.io/jielie chip-marks guide, summarised not read in full |
| Nearby silkscreen | `DM` (USB D−), `GND`, `R2/R3/R8`, multi-pad test pattern | `PROVEN` — same photo |
| Toolchain | JieLi SDK is closed, NDA/licence-gated, no public flashing path | `ASSUMED` — widely reported; not attempted here |

**Consequence:** the stock brain is not reprogrammable by us on any sane schedule. It is a
BT-audio-class SoC doing double duty as toy MCU (app link + bark/voice playback + LEDs).
Plan for **brain transplant, not firmware modification.**

### 2.3 Firmware storage

| Field | Value | Evidence |
|---|---|---|
| Marking | `25V16066` `M1I02`, SOIC-8, crystal adjacent | `PROVEN` — `pcb-spiflash-25v16066.jpg` |
| Part | SPI NOR flash, `25`-series, `16` = **16 Mbit / 2 MB** | `ASSUMED` — from the `25Q/25V` naming convention |
| Contents | JieLi firmware image + audio assets (barks, voice lines) | `ASSUMED` |

Dumping it with a CH341A/flashrom clip is cheap and non-destructive, and would confirm the
SoC family and let us extract the sound assets. Optional, not on the critical path.

### 2.4 Actuator assembly — **one motor per leg, with a sensor**

Source: `hardware/photos/mech-leg-pair-*.jpg` (two interior views, front pair and rear pair)
plus the operator's count on 2026-08-03.

| Field | Value | Evidence |
|---|---|---|
| Motor count | **4 total — one per leg** | `PROVEN` — operator count |
| Motor type | Small brushed DC can motors, ~130-class, orange brush/EMI caps visible, **2 wires each** (red/black, red/yellow) | `PROVEN` — clearly visible in both photos |
| Gearbox | Large finned black housing per leg, motor mounted alongside and geared into it | `PROVEN` — both photos |
| Feedback | **One 3-wire sensor per leg** on a small green PCB, mounted at the gearbox output | `PROVEN` — operator statement + two green PCBs with red/white/blue cable visible in each photo |
| **Sensor type** | **Potentiometer — absolute angle** | `PROVEN` — operator, 2026-08-03 |
| **Output motion** | **Limited arc, sweeps back and forth** (not a continuous crank) | `PROVEN` — operator observation on the running toy, 2026-08-03 |
| **Pot part** | Marked **`B103`** and **`330°`** — green square body, 3 in-line pins, mounted in the gearbox housing | `PROVEN` — `hardware/photos/mech-actuator-pot-b103-330deg.jpg` |
| **Pot value / taper** | **10 kΩ, linear** (`B` = linear taper, `103` = 10 × 10³ Ω) | `ASSUMED` — standard marking convention; verify with one ohmmeter reading |
| **Pot electrical travel** | **330°** | `PROVEN` — printed on the body |
| Pin / wire mapping | left pin = **white**, centre pin = **blue**, right pin = **red** | `PROVEN` — same photo |
| **Pot supply rail** | **3.3 V** | `PROVEN` — operator measurement, 2026-08-04 |
| **`VMCU`** | **3.3 V** | `PROVEN` — operator measurement, 2026-08-04 |
| Wiper | **blue** (centre pin) | `ASSUMED` — centre pin is the wiper on every standard 3-pin pot; confirm with an ohmmeter before powering anything |
| Gearbox housing | moulded **`2`** on one half — the four actuator modules appear to be numbered | `PROVEN` — same photo |

> **Colour-convention correction.** An earlier entry in this file assumed red/white/blue =
> VCC/signal/GND. The photo shows the **centre pin — blue — is the wiper**, with white and red
> on the track ends. Do not wire from the colour guess; the ohmmeter check is one minute.

**What this is, in one line: four home-made servos.** Motor + gearbox + output potentiometer,
with the position loop closed in the stock firmware — the same construction as a hobby servo,
built discretely on the main board. We unplug the stock board and close that loop ourselves.

With one actuator per leg the foot travels a **fixed 1-D path** set by the linkage, parameterised
by pot angle. No abduction, no strafe. But four independently positioned legs still give three
useful body degrees of freedom while standing — **height, pitch and roll** (four inputs, three
outputs, one redundant) — so levelling on a slope, bowing, and tilting to track a face are all
reachable. Measuring the foot path (Q9) is what turns that from a claim into a controller.

### 2.5 Connector map — the board labels every leg

Source: `hardware/photos/pcb-connectors-*.jpg`. The connector row along one board edge is fully
silkscreened in pinyin initials, which is the manufacturer telling us the wiring map directly.

**Decode** (`ASSUMED` — standard Chinese toy-PCB convention, self-consistent with everything
measured so far; confirm one connector by continuity before trusting the rest):

| Token | Chinese | Meaning |
|---|---|---|
| `DJ` | 电机 *dianji* | **motor** |
| `DWQ` | 电位器 *dianweiqi* | **potentiometer** |
| `Z` / `Y` | 左 / 右 | left / right |
| `Q` / `H` | 前 / 后 | front / rear |

**Eight connectors — four motors, four pots. Nothing else.**

| Board label | Our name | Housing colour | Function |
|---|---|---|---|
| `ZQDJ` | FL motor | white | left-front motor |
| `YQDJ` | FR motor | yellow | right-front motor |
| `ZHDJ` | RL motor | white | left-rear motor |
| `YHDJ` | RR motor | yellow | right-rear motor |
| `ZQDWQ` | FL pot | white | left-front potentiometer |
| `YQDWQ` | FR pot | yellow | right-front potentiometer |
| `ZHDWQ` | RL pot | white | left-rear potentiometer |
| `YHDWQ` | RR pot | yellow | right-rear potentiometer |

**Logger channel map — verified on the bench 2026-08-04** (`tools/pot_logger_uno`):

| Uno pin | Connector | Leg | Verification |
|---|---|---|---|
| A0 | `ZQDWQ` | front-left | `PROVEN` — moved FL alone, A0 swung **348 counts (170.5°)** while the other three stayed ≤ 10 counts. **34.8× separation.** |
| A1 | `YQDWQ` | front-right | `ASSUMED` — follows from the A0 result plus the corrected tap order; not individually driven |
| A2 | `ZHDWQ` | rear-left | `ASSUMED` — as above |
| A3 | `YHDWQ` | rear-right | `ASSUMED` — as above |

An earlier run had the taps permuted; they were corrected **at the wires**, not in software, so
no per-channel remap constant exists anywhere in the codebase. The 34.8× separation also shows
**no measurable cross-talk** between channels, which validates the mux-settling discard and the
single-point grounding.

**Housing colour is a left/right key: white = `Z` (left), yellow = `Y` (right)** — consistent
across all eight. `PROVEN` from the photos. Preserve this convention in our own loom; it makes
cross-plugging a leg physically obvious.

`DWQ` = 电位器 is **independent confirmation from the manufacturer's own silkscreen that the
sensors are potentiometers**, alongside the `B103` marking and the operator's ohmmeter check.
Q1 is settled three ways.

Other silkscreen on this edge:

| Label | Observation |
|---|---|
| `SPK` | blue 2-pin — speaker |
| `LED` | white 3-pin |
| `KG1` | 开关 *kaiguan* = switch — the power switch |
| `+BAT` / `BAT-` | battery connections |
| `CD` | black 3-pin at the board edge — plausibly 充电 *chongdian* = charge. `UNKNOWN`, verify. |
| `F1` | fuse position |
| `C26` / `C27` / `C28` | three large electrolytics |
| **`Gun`** | **two small pads, unpopulated** — an accessory footprint this unit does not fit. Evidence that this PCB is a platform board serving several SKUs, which supports the "spare H-bridge channels belong to another variant" reading of Q8. `ASSUMED`. |

### 2.6 Not yet photographed

The 2.4 GHz RF receiver for the bundled remote, the battery/charge circuit, the LED drivers
and any sensor front-end are all on parts of the board not covered by these six shots.

---

## 3. Open questions that gate the whole project — `UNKNOWN`

Phase 0 exists to close every one of these. Ranked by how much they change the design.

| # | Question | Why it decides the architecture |
|---|---|---|
| ~~Q2~~ | **CLOSED 2026-08-03, twice over: 4 motors, 4 pots.** Operator count, then confirmed by the board's own connector silkscreen — `ZQDJ`/`YQDJ`/`ZHDJ`/`YHDJ` + `ZQDWQ`/`YQDWQ`/`ZHDWQ`/`YHDWQ` and nothing else (§2.5). The 1-DOF-per-leg model stands. | — |
| **Q8** | **Are the two halves of each MX1616S paralleled onto one motor, or is half the driver stage simply unpopulated-in-use?** Continuity-check each chip's output pair against the motor connectors — two minutes with a multimeter. | **Paralleled** would mean the designer needed more than ~1.3 A per motor, which raises the floor on our driver choice before Q3 is even measured. **Unused** more likely means this is a platform board shared with a bigger 8-motor SKU (consistent with the `RV2` naming), and says nothing about current. Cheap to settle, and it changes the BOM. |
| **Q10** | **What is `U5`?** Read the part marking under the microscope. | Determines whether the stock board's motion MCU is an identifiable, tool-supported ARM part. Does **not** change the chosen strategy — reflashing it is excluded by the reversibility decision (`ARCHITECTURE.md` §1) — but it tells us what the board is worth as a fallback, and confirms or kills the ARM inference above. |
| Q3 | **Stall current per motor at pack voltage** | Picks the replacement driver. <1.5 A → DRV8833. 1.5–3 A → TB6612FNG or DRV8871. >3 A → different board entirely. |
| Q4 | **Battery: chemistry, cell count, nominal voltage, capacity, charge circuit** | MX1616's ~9.6 V ceiling implies 2S Li-ion (7.4 V), but that is inference, not measurement. Sets the whole power tree and whether a Pi 5 can share the pack. |
| **Q9** | **Foot-path calibration:** for each leg, pot angle → foot position (x, z) in the body frame | With one actuator per leg the foot travels a fixed 1-D curve. That curve *is* the leg model — everything from stride shape to body-height control derives from it. Measured, not derived. |
| Q5 | **Pot range and mechanical hard stops** — ADC counts at both stops, per leg | Software limits sit 5° inside the stops. Also gives the counts-per-degree scale for the position loop. |
| Q6 | **Remote RF chip and protocol** | Only matters if we choose to sniff/replay rather than replace. Low priority under the chosen plan. |

**Closed on 2026-08-03:** Q1 — the sensor is a **potentiometer**, so every leg reports absolute
angle with no homing needed. Q7 — the output **sweeps a limited arc**, so this is a conventional
position-control problem, not a phase-locked crank. Together these are the best available
outcome: the toy is effectively four large servos built from motor + gearbox + pot, with the
position loop closed in the stock firmware. We rebuild that loop, better.

---

## 4. Safety notes

- Li-ion pack, unknown BMS. Never bench-run the robot from the pack with the charger
  connected until the charge topology is understood (Q4).
- Leg linkages are pinch hazards at full torque and this chassis is ~18 in / 45 cm. All
  bench work happens with the body on a stand, feet off the ground, until Phase 3.
- Stalled brushed DC motors draw peak current indefinitely and cook both the driver and the
  gearbox. No motor is commanded without a current limit and a stall timeout in firmware.
