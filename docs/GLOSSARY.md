# Glossary

Terms as used **in this project**. Where a word has a broader meaning elsewhere, the definition
here is the one that applies to our machine.

---

## The board's own labels

The manufacturer silkscreened the wiring map in pinyin initials. This is the single most useful
piece of documentation the toy came with.

| Token | Chinese | Meaning |
|---|---|---|
| `DJ` | 电机 *dianji* | **motor** |
| `DWQ` | 电位器 *dianweiqi* | **potentiometer** |
| `Z` / `Y` | 左 / 右 | left / right |
| `Q` / `H` | 前 / 后 | front / rear |
| `KG` | 开关 *kaiguan* | switch |
| `CD` | 充电 *chongdian* (`ASSUMED`) | charge |

So `ZQDJ` = left-front motor, `YHDWQ` = right-rear potentiometer.

**Housing colour is a left/right key: white = `Z` (left), yellow = `Y` (right).** Preserve this
in our own loom — it makes a cross-plugged leg physically obvious.

---

## Project vocabulary

**Backdrivable** — a gearbox you can turn by pushing on the output. Ours are, which is why hand
calibration works. A worm-drive gearbox would not be, and Phase 0 would have needed rethinking.

**Backlash** — slack between gear teeth. Reverse direction and the motor turns a few degrees
before the output moves. Because our pot reads the output, backlash sits *inside* the control
loop, which is why slew limits and a deadband are mandatory. See `HOW_IT_WORKS.md` §2.

**Brake vs coast** — at the end of a move, shorting the motor terminals together makes it resist
turning (brake); leaving them open lets it spin freely (coast). Braking helps hold a pose against
gravity without burning holding current.

**COBS** — Consistent Overhead Byte Stuffing. A framing scheme that removes a chosen byte value
from a data stream so it can be used unambiguously as a frame delimiter. Used on the Pi ↔ MCU
link so a resynchronising receiver can always find a frame boundary.

**Cortex / spinal cord** — our names for the two-brain split. The **cortex** (Pi 5) does vision
and behaviour and may be slow or absent. The **spinal cord** (RP2350) does the 200 Hz control and
safety and must never be late. See `HOW_IT_WORKS.md` §8.

**Deadman** — the rule that the MCU expects a command frame at least every 100 ms. Miss three and
it ramps to a stand and disables the drivers. Makes the robot safe with the Pi absent.

**Duty cycle** — the fraction of a PWM period the output is on. How motor speed is controlled.

**Duty factor** — the fraction of a gait cycle a given leg spends in stance. Above 0.5 means more
than half the legs are down and the gait is statically stable. *Not the same as duty cycle.*

**Evidence label** — every hardware claim carries one:
- `PROVEN` — measured or directly observed, with the evidence cited (`file:line`, a photo, an
  instrument reading)
- `ASSUMED` — believed, with the reason stated and the fact that it is unverified stated too
- `UNKNOWN` — never investigated, said plainly

Absence of evidence is a statement about our work, not about the world. "We never tried X" is not
"X does not work".

**Foot path / `legpath()`** — with one actuator per leg the foot travels a single fixed curve set
by the linkage, parameterised by pot angle. This curve *is* the kinematic model; there is no
inverse kinematics to solve. Measured, not derived. Open question Q9.

**Gait** — the repeating pattern of which feet are on the ground and when, defined by four phase
offsets. Walk, trot and bound differ only in those four constants.

**H-bridge** — four switches arranged around a motor so current can be driven either way,
allowing reverse. Our stock board uses `MX1616S`, two bridges per chip.

**Hunting** — a control oscillation where the leg overshoots, corrects, overshoots the other way,
and never settles. On this machine it is usually backlash plus too little deadband, and it eats
gear teeth.

**Intent** — the only thing the Pi is allowed to send: body velocity, gait mode, pose. **Never
raw PWM.** The MCU decides how to achieve it, which is what makes link loss survivable.

**Lock-antiphase vs sign-magnitude** — two ways to apply PWM to an H-bridge. Sign-magnitude puts
PWM on one input and holds the other low; lock-antiphase drives both complementarily, with 50 %
meaning stopped. Which the stock firmware uses is task P0-09.

**Mode A / Mode B** — the two pot wiring modes. **A**: toy on, it powers the pots, we read only.
**B**: toy off, `DWQ` connectors unplugged from the board, we power the pots from the Uno.
Confusing them back-feeds the stock board. See `RUNBOOK.md` §1.

**Overrun** — a missed sampling deadline in the logger. Counted and reported in every capture's
trailer. **Non-zero overruns mean the capture has timing gaps and is not usable for measuring
gait period.**

**Ratiometric** — measuring a divider against the same reference that supplies it, so supply
variation cancels. The pot is powered from the same 3.3 V that feeds the ADC reference; we also
log the rail so captures can be normalised afterwards.

**RDP / read-out protection** — a flag on a microcontroller that blocks reading its flash. The
usual way to clear it is a **mass erase**, which is why `U5` is never written to: erasing it
would destroy the stock firmware permanently and take the rollback path with it.

**Shoot-through** — turning on both switches on one side of an H-bridge at once, shorting the
supply. Integrated driver chips prevent it internally; discrete FET bridges must handle it in
firmware with dead-time.

**Slew limit** — a cap on how fast a setpoint may change. Prevents the trajectory generator from
issuing a step change, which the control loop would answer with maximum current.

**Stall detection** — current above a threshold **and** angle not changing, both, for 200 ms.
Current alone false-trips on every acceleration.

**Stance / swing** — the two halves of a leg's gait cycle: foot planted and pushing the body
along (stance), foot lifted and returning (swing).

**Static stability** — the centre of mass stays inside the polygon formed by the feet that are
down. Three feet down gives a triangle and the robot cannot fall. Given our DOF budget, stay
statically stable.

**Wind-up** — an integral term growing without bound while the output is saturated, typically
during a stall. When the obstruction clears, the accumulated term slams the actuator. Clamp it.

---

## Parts on the toy

| Marking | What it is | Confidence |
|---|---|---|
| `MX1616S` ×4 (U4, U6, U7, U10) | Dual H-bridge brushed DC motor driver, SOP-16 | Marking `PROVEN`; function and ratings `ASSUMED` from distributor pages |
| `AF24C105159-65E4` + `JL` logo | JieLi SoC, TSSOP-28 — Bluetooth/app/audio brain | Vendor `PROVEN`; AC69xx family `ASSUMED` (JieLi markings are deliberately obfuscated) |
| `U5` | TSSOP-28 with `SWD`/`CLK`/`GND`/`VMCU` pads — the motion MCU | Pads `PROVEN`; "is an ARM Cortex-M" `ASSUMED`; part `UNKNOWN` (task P0-04) |
| `25V16066` | SPI NOR flash, SOIC-8 — JieLi firmware + audio assets | Marking `PROVEN`; 16 Mbit `ASSUMED` from the naming convention |
| `B103` / `330°` | Potentiometer, 10 kΩ linear, 330° electrical travel | Marking `PROVEN`; 10 kΩ linear `ASSUMED` from the standard convention |
| `Gun` | Unpopulated 2-pad accessory footprint | `PROVEN` it is unpopulated; that it indicates a shared platform board is `ASSUMED` |
| Board | `JXD-8002-Blue-YW-RV2`, dated 2024-01-19 | `PROVEN`. **No documentation exists online — searched 2026-08-03, do not repeat.** |

---

## Our own hardware

| Term | What it is |
|---|---|
| **RP2350 / Pico 2 W** | The chosen real-time controller. Note: on the `W` variant `GPIO29`/ADC3 is shared with the VSYS divider *and* the CYW43 radio, leaving only `GPIO26–28` — three clean ADC channels, one short of four legs. The production board uses a bare RP2350 where all four are free. |
| **FX2LP clone** | Our logic analyser — `0x0925:0x3881`, no USB strings. Digital only, 8 channels. Use sigrok, not Saleae Logic 2. |
| **NanoDAP** | CMSIS-DAP debug probe, for read-only SWD on `U5`. |
| **`pot_logger_uno`** | The Arduino Uno rig that logs four pot wipers plus the supply rail at 200 Hz. 0.49°/count. |

---

## Numbers worth memorising

| Quantity | Value |
|---|---|
| Actuators | 4, one per leg, 1 DOF each |
| Pot | 10 kΩ linear, 330° electrical travel, on the **output** shaft |
| Pot supply rail | 3.3 V (674 counts on the Uno rig) |
| `VMCU` | 3.3 V |
| Logger scale | 0.49°/count · RP2350 will give 0.081°/count |
| Leg travel | ~170° (lower bound — hand-swept, not to the stops) |
| Control loop | 200 Hz |
| Link | UART 460800, COBS + CRC-16, cmd 50 Hz, state 100 Hz, deadman 100 ms |
