# Phase 0 — Non-destructive instrumentation of the stock board

Two independent investigations using bench kit already on hand: **NanoDAP** (CMSIS-DAP probe) on
the `U5` SWD pads, and the connected logic analyser on the motor drivers, plus an ADC logger on the pot wipers.

**Read the ranking first.** The Saleae work is worth more than the SWD work, and carries less
risk. If time is short, do §2 and skip §1.

---

## 0. The one rule that cannot be broken

> **READ ONLY. Never issue an erase, unlock, mass-erase, or "auto-unlock on connect".**

`U5` is almost certainly read-protected. Every vendor's answer to "the debugger can't read
flash" is *mass erase* — and several tools do it automatically, silently, as a convenience.
If that happens, the stock firmware is gone permanently, the toy never walks again, and the
rollback path this whole project is built on disappears.

Specifically forbidden, whatever the tool suggests:

- `pyocd erase`, `pyocd flash`, `--erase=chip`, `--connect=under-reset` combined with any
  unlock prompt
- OpenOCD `mass_erase`, `stm32fxx.cpu mass_erase`, `flash erase_sector`, `program`
- Any IDE "connect and unlock", "remove read protection", "RDP → level 0" action
- J-Link Commander `unlock` / `erase`

This overrides the tool's advice, always. A failed read is an acceptable outcome. An erased
chip is not.

---

## 1. SWD interrogation of `U5` — what it can and cannot tell us

### 1.1 What you get without unlocking anything

Even on a fully read-protected part, the debug port itself usually still answers. In descending
order of certainty:

| Read | Address / mechanism | Tells you |
|---|---|---|
| **DPIDR** | returned by the SWD line-reset handshake | JEP106 **DESIGNER** field → the silicon vendor. This alone likely identifies the manufacturer. |
| **AP IDR** | AP register 0xFC | Which access ports exist (AHB-AP etc.) — confirms a Cortex-M |
| **CPUID** | `0xE000ED00` | Definitive core: M0 / M0+ / M3 / M4 / M23 / M33, plus revision |
| ROM table | base from AP `BASE` register | CoreSight component IDs, debug feature set |
| Flash / SRAM | vendor-specific | **Only if not read-protected.** If this reads back all-zero, all-0xFF, or faults, RDP is on — accept it and stop. |

Note the pad set is `SWD`, `CLK`, `GND`, `VMCU` — **there is no `NRST` pad**. Consequence: if the
stock firmware reconfigures the SWD pins as GPIO early in boot (a common power/pin-saving trick
in toys), you cannot connect-under-reset to win the race, and there is no easy workaround. A
failed connect is more likely to mean *that* than a dead probe.

### 1.2 Electrical prep — do this before the probe touches anything

- [x] **`VMCU` measured 2026-08-04: 3.3 V.** A 3.3 V CMSIS-DAP probe is level-compatible —
      this removes the main electrical unknown. The SWD session is cleared to proceed on §1.2's
      remaining points.
- [ ] Confirm the `GND` pad really is battery negative (continuity, board off).
- [ ] **Do not connect the probe's VCC/power output to the board.** The board powers itself. If
      the NanoDAP has a `VTref` sense input, connect that to `VMCU` so its level shifters follow
      the target. If it has no `VTref`, confirm it is 3.3 V logic before proceeding.
- [ ] **Disconnect the motor supply** if the board allows logic-only power. Otherwise put the
      body on a stand with the legs hanging free.
- [ ] Connect in this order: **GND first**, then `SWD` (SWDIO), then `CLK` (SWCLK).
- [ ] Start at a **slow clock — 100 kHz**. These pads are unbuffered test points with no series
      termination; speed buys nothing here.

### 1.3 Why halting is the real hazard, not erasing

If you halt the core while the H-bridges are enabled, the driver inputs freeze in whatever
state they held. A motor left energised against a mechanical stop will stall — and a stalled
brushed motor draws locked-rotor current indefinitely, cooking the gearbox and the MX1616S.
The stock firmware's watchdog is not going to save you; you have halted the thing that feeds it.

So: **attach without halting.** With pyOCD that is `--connect=attach`, which leaves the target
running. Reading DPIDR, CPUID and the ROM table does not require a halted core.

### 1.4 Suggested sequence (pyOCD)

Verify the flags against your installed version — pyOCD's CLI has changed across releases, and
a wrong flag here is not a typo, it is a risk.

```bash
pyocd list --probes                      # confirm the NanoDAP enumerates

pyocd commander \
  --connect=attach \                     # DO NOT change to halt/under-reset
  --target=cortex_m \                    # generic target: no vendor pack, no flash algo loaded
  --frequency=100000
```

Then, inside the commander, read-only commands:

```
status                 # core state — confirms the DP answered
read32 0xE000ED00      # CPUID
show map               # memory regions the AP reports
```

Using `--target=cortex_m` matters: a generic target loads **no flash algorithm**, so there is
nothing present that could erase anything even by accident.

If flash turns out readable, dump it to `hardware/dumps/` and stop there. Disassembling a toy's
motion firmware to recover gait tables is weeks of work for data that §2 gives you in an
afternoon.

### 1.5 What we will actually do with the result

Honest scope: **this does not change the plan.** Option C (our own board) is chosen, and
Option D (reflashing `U5`) is excluded by the reversibility decision. The value is:

- Closes **Q10** — confirms or kills the "U5 is an ARM Cortex-M" inference, which currently
  rests on two silkscreen labels.
- Tells us what the stock board is worth as a fallback if the custom board slips.
- Costs an hour and risks nothing, provided §0 holds.

---

## 2. Logic capture — **instrument reality check first**

### 2.0 What is actually on the bench (checked 2026-08-03)

`ioreg` enumeration of the connected analyser:

| Field | Value |
|---|---|
| idVendor | **0x0925** (Lakeview Research) |
| idProduct | **0x3881** |
| USB product string | **none** |
| USB vendor string | **none** |
| iSerialNumber | **0** |
| bcdDevice | 1 |

`PROVEN` — that is the enumeration. What follows is inference:

**`ASSUMED`: this is an FX2LP (Cypress CY7C68013A) clone of the *original* 2008 Saleae Logic —
8 digital channels, up to 24 MS/s, and NO ANALOG INPUTS.** Reasoning: `0x0925:0x3881` is the
original Logic's ID, universally reused by unbranded clones; and genuine Saleae hardware
reports proper USB string descriptors and a serial number, where this device reports neither.
Modern Saleae units (Logic 8, Logic Pro 8/16 — the ones with analog) enumerate under
**`0x21A9`**, not `0x0925`.

**Consequences, both of which matter:**

1. **Saleae Logic 2 will not see this device.** Logic 2 targets the `0x21A9` family. Use
   **sigrok** instead — `brew install sigrok-cli` (formula exists, verified available, not yet
   installed). Note `sigrok-cli` alone is not enough: FX2LP clones ship with no firmware, so
   libsigrok uploads `fx2lafw` at runtime and that blob is **not** in Homebrew — fetch
   `sigrok-firmware-fx2lafw-bin` from sigrok.org and drop it where libsigrok looks
   (`$(brew --prefix)/share/sigrok-firmware/`; confirm the search path with `sigrok-cli -L`).
   There is no PulseView cask in Homebrew; `sigrok-cli` exports CSV directly, which is what we
   want anyway.
2. **§2.1 as originally written is impossible on this hardware.** Capturing pot wiper voltages
   needs analog channels. This instrument has none.

**Definitive check, one minute:** run `sigrok-cli --scan`. If it appears as `fx2lafw`, the
inference above is confirmed and it is digital-only.

So the work splits across two instruments.

### 2.1 Pot trajectories — needs an ADC logger, not this analyser

Still the highest-value data in Phase 0: the four wiper voltages *are* the joint trajectories of
a machine that already walks. We just cannot get them with an FX2LP clone.

**Build a 4-channel ADC logger. This is not a detour — it is the Phase 1/2 feedback front end,
brought forward.** Same wiring, same scaling code, same CSV format. In Phase 1 the driver stage
gets added to the identical harness.

**Split the job by whether the robot must be loose.** Static poses happen on a stand where a USB
tether is harmless — do those with the Arduino Uno, now, with nothing to buy. Walking captures
need the robot untethered, which is what the ESP32 is for.

**Do not use the ESP32's internal ADC for this** (the Uno's is fine — see the table). The ESP32 classic ADC is
non-linear, noisy, and non-monotonic in places, and its usable input span is roughly
150 mV–2450 mV — not the full rail. Systematic non-linearity could be calibrated out; the
non-monotonic patches cannot, and they would corrupt the trajectory exactly at the travel
extremes, which is where the pose data lives. Use an external ADC and treat the MCU as a logger.

| Approach | Verdict |
|---|---|
| **Arduino Uno, tethered over USB** | **CHOSEN 2026-08-04.** With the rail measured at 3.3 V, channel count beats resolution: the Uno's **six** ADC channels take four pots *plus* the supply rail *plus* a spare, where an ADS1115 has only four and a Pico 2 W only three. At the default 5 V reference a 0–3.3 V wiper spans 676 counts over the pot's full 330°, so ~185 counts across a typical ~90° leg arc ≈ **0.5°/count** — far finer than the gearbox backlash, which is what actually limits us. **Leave `AREF` alone:** tying it to 3.3 V would raise that to ~276 counts, a negligible gain for a real hazard (see below). Nothing to buy, and the ATmega328P ADC is monotonic and well-behaved, unlike the ESP32's. Crucially it is **5 V-native**, so it cannot be over-ranged by the stock pot rail whatever that turns out to be — it is the safe first instrument to touch an unmeasured signal with. 10-bit over 5 V = 4.9 mV/LSB; across a ~90° leg arc that is ~280 counts, roughly 0.3°/count. Ample for trajectory shape. `analogRead()` round-robined over 4 channels gives ~2 kSPS each — 1000× oversampled. Also closes Q5 (pot range at both stops) for free. |
| **ESP32(-S3) + ADS1115, buffer in RAM, dump over USB afterwards** | **For the walking captures only.** See the untethering note below. 16-bit, 4 channels, I²C. Powered at 5 V, the ADS1115's ±6.144 V range tolerates the stock pot rail **whatever it turns out to be** — no risk of over-ranging an input before we have measured it. 860 SPS round-robined ≈ 215 SPS per channel: ~100× oversampled for signals moving at a few Hz. |
| ESP32 + ADS1115 + **SD card** | Same, with SD instead of a RAM buffer. Worth it only for long or continuous runs — see the sizing note; most captures fit in RAM, and SD adds card-init and FAT failure modes to a job that does not need them. |
| Uno + SD shield | The untethered option if you would rather not introduce an ESP32. Uno SRAM is 2 KB, so RAM buffering is out — 30 s of 4-channel data is ~192 KB. SD is mandatory for walking captures on this MCU. |
| Pico + ADS1115, tethered USB | Works, but the Uno is already on the bench and is 5 V-safe. |
| Any MCU's internal ADC, direct | Rejected — see above. Also a Pico *module* has only 3 clean ADC channels (`GPIO29`/ADC3 is on the VSYS divider, `ARCHITECTURE.md` §3). |
| MCP3008 instead of ADS1115 | Alternative if you want 8 channels (4 pots + rail + spares) or higher rate. 10-bit, so ~300 counts across a typical leg arc — adequate, but coarser than the ADS1115. |

**Why an ESP32 specifically: the walking captures cannot be tethered.** Pose captures happen on
a stand where a USB lead is harmless, but walk/turn/dance captures need the robot loose on the
floor, and a cable to the Mac drags on it and biases the gait you are trying to measure. A
battery-powered ESP32 riding on the robot solves that. WiFi streaming is a third option, but a
RAM buffer or SD is more robust than chasing dropped UDP packets.

**Sizing:** 30 s × 200 SPS × 4 channels × 8 bytes ≈ **192 KB**. That fits in an ESP32-S3's heap,
and trivially in PSRAM. So: capture to RAM, dump over serial after. Reach for SD only if a run
needs to be minutes long.

**Getting this right matters more than the sample rate:**

- [ ] **Common ground with the toy — the first wire connected, every time.**
- [ ] **Do not power the pots from the logger.** The stock board supplies them; we only read the
      wipers, high-impedance. Powering both ends fights the stock rail.
- [ ] **Timestamp every sample** (µs since boot) in the row itself, not at write time. SD and
      serial both stall unpredictably; a timestamp taken at write time is a lie.
- [ ] **Mark the trigger.** You need to know when each behaviour began. Simplest: one file per
      behaviour, started before the remote press. Better: a pushbutton on the logger that writes
      a marker row, pressed as you press the remote.
- [ ] CSV schema `t_us,ch0,ch1,ch2,ch3` — the same schema the Phase 2 telemetry path will use,
      so the analysis code is written once. Use `micros()` on the Uno.
- [ ] **Uno `AREF` — decided: do not touch it.** Tying `AREF` to the 3.3 V rail would lift
      resolution from ~185 to ~276 counts across the arc. Not worth it: if `analogReference(EXTERNAL)`
      is not called *before* the first `analogRead()`, the internal reference is shorted to `AREF`
      and the chip can be damaged, and the wiring order at power-on is not something the sketch
      controls. Run at the default 5 V reference. Backlash dominates long before 0.5°/count does.

This rig has a second life: it becomes the robot's telemetry logger from Phase 2 onward.

- [ ] **Measure the stock pot rail first** (`PHASE_0_TEARDOWN.md` Step 2a) before choosing.
- [ ] Common ground between the logger and the toy — mandatory, and the first wire connected.
- [ ] Tap the **blue, centre-pin** wire of each `DWQ` connector. Back-probe at the housing;
      **cut nothing**.

| Logger channel | Board connector | Leg | Housing |
|---|---|---|---|
| 0 | `ZQDWQ` | front-left | white |
| 1 | `YQDWQ` | front-right | yellow |
| 2 | `ZHDWQ` | rear-left | white |
| 3 | `YHDWQ` | rear-right | yellow |

Log the **pot supply rail** too if a fifth channel is free. The pots are ratiometric: as the pack
sags, every wiper voltage scales with it, and the rail is what lets captures be normalised
afterwards. On a 4-channel ADS1115 it competes with the fourth leg — in that case record the rail
voltage manually at the start and end of each run instead.

Capture one run per remote button, body on a stand for poses, on the floor for walking. Note
which, per run. Export CSV to `hardware/captures/`.

| Behaviour | File | Stand/floor | Rail V start/end | Notes |
|---|---|---|---|---|
| stand → sit | | | | |
| sit → stand | | | | |
| walk forward, 5 s | | | | |
| walk backward | | | | |
| turn left / right | | | | |
| lie down | | | | |
| paw / handshake | | | | |
| dance / demo | | | | |

**What this yields:** the complete stock motion library as four synchronised joint trajectories,
measured from a working machine. Combined with the foot-path calibration (Q9,
`PHASE_0_TEARDOWN.md` Step 2.5) it converts directly into foot trajectories in the body frame —
exactly the input our gait engine needs. Phase 3 becomes replication against a known-good
reference rather than gait design from scratch.

### 2.2 Driver inputs — the FX2LP clone does this job well

Digital-only is no handicap here. The MX1616S inputs are logic-level and the PWM carrier will be
in the low kHz, so 8 channels at 24 MS/s is far more instrument than the job needs.

- [ ] Probe the two logic inputs of the half-bridge driving one leg, then move the probes.
- [ ] Common ground to board `GND`.
- [ ] `sigrok-cli` example shape — verify against your installed version:
      `sigrok-cli -d fx2lafw --config samplerate=1m --channels D0,D1,D2,D3 --time 5s -O csv > walk.csv`

| What to extract | Why it matters |
|---|---|
| **PWM carrier frequency** | Our starting point. Too low and the motors whine audibly; too high and MX1616S switching losses climb. The stock designer already found a value that works with these motors. |
| **Drive scheme** — sign-magnitude (one input PWM'd, other low) vs lock-antiphase (complementary) | Changes our output stage and the current ripple. Copy what works. |
| **Braking behaviour** — both inputs high (short brake) vs both low (coast) at end of move | How the stock firmware holds a pose against gravity, which is the hardest part of a static stand. |
| **Duty range actually used** | Never exceeding ~60 % means headroom and a gearbox-limited design. Saturating at 100 % means the motors are working hard. |
| Simultaneous vs sequential leg motion | Whether the mechanism tolerates coordinated multi-leg trajectories. |

### 2.3 Optional — the JieLi ↔ `U5` link

If a UART runs between the two chips, capturing it during each remote button press reveals the
internal command vocabulary. Interesting, and cheap to grab while the probes are already
attached — but not on the critical path, because we are replacing both chips.

---

## Exit gate

**PASS** requires:

1. `VMCU` and the pot supply rail measured and recorded.
2. Either a DPIDR/CPUID reading for `U5` (Q10 closed), **or** a documented failed-connect with
   the reason, **and the chip not erased.**
3. Pot-wiper captures for at least: stand, sit, walk forward, turn — exported to
   `hardware/captures/` from the ADC logger (§2.1), not the logic analyser.
4. PWM frequency, drive scheme and braking behaviour extracted from the driver-input capture.
5. **The toy still works on its stock remote.**

**FAIL** if the chip was erased, at which point log it honestly in `CLAUDE.md` State — the
rollback path is gone and Phase 0's "toy still works" gate can never be met again.
