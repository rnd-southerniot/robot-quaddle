# pot_logger_uno

Phase 0 joint-trajectory logger. Reads the four leg potentiometers of the **stock** board while
the toy runs its **own** firmware, and streams timestamped CSV over USB.

The wipers are the joint trajectories. Capturing them gives us the stock motion library as
measured data — the input Phase 3 needs, from a machine that already walks well.

## Wiring

**GND to the toy's board GND is the first wire connected, every time.**

| Uno | Board connector | Leg | Housing |
|---|---|---|---|
| A0 | `ZQDWQ` wiper (blue, centre pin) | front-left | white |
| A1 | `YQDWQ` wiper | front-right | yellow |
| A2 | `ZHDWQ` wiper | rear-left | white |
| A3 | `YHDWQ` wiper | rear-right | yellow |
| A4 | pot supply rail — either outer pin of any `DWQ` | — | — |
| D2 | marker button to the **Uno's own GND** (internal pull-up) | — | — |
| GND | **one** wire to the toy, landed on a `DWQ` **ground pin** — see below | — | — |

### Two wiring modes — the toy powers the pots, or we do. Never both.

Discovered on the bench 2026-08-04: with the toy switched off, **every channel including `rail`
reads 0**. The pots have no supply of their own — the stock board feeds them 3.3 V. So the two
jobs this rig does need two different wirings.

**Mode A — behaviour capture. Toy ON.**
The stock board powers the pots; we only read wipers, high-impedance. `DWQ` connectors stay
plugged into the board and we back-probe them. **Never inject 3.3 V in this mode** — it fights
the stock regulator.

**Mode B — hand calibration (Step 2a pot range, Step 2.5 foot path). Toy OFF.**
You want the toy dead for this: the stock firmware cannot fight you and no motor can run. But
then the pots are unpowered, so **we** must supply them:

1. Switch the toy off.
2. **Unplug the four `DWQ` connectors from the stock board.** This is not optional. Injecting
   3.3 V into a still-plugged connector back-feeds the board's whole 3.3 V net and can
   parasitically part-power the JieLi SoC and `U5` — unpredictable, and exactly the kind of
   thing guardrail 2 exists to prevent.
3. Pot track ends → Uno **`3V3`** and Uno **`GND`**. Wipers → A0–A3. A4 → the Uno's own `3V3`,
   so ratiometric normalisation still works and reads its usual ~676 counts.
4. Four pots at 10 kΩ draw 1.3 mA total; the Uno's `3V3` pin is good for ~50 mA.

Going back to Mode A means removing the injected 3.3 V **before** reconnecting the `DWQ`
connectors and powering the toy.

| Symptom | Meaning |
|---|---|
| **All channels including `rail` read exactly 0** | The pot supply is dead — toy switched off, flat pack, or `KG1` open. Not a wiring fault. |
| Legs read 0 but `rail` reads ~676 | Wiper taps wrong, or `DWQ` connectors unplugged while still in Mode A. |
| Everything drifts around 290–350 with no supply | Inputs floating: nothing connected at all. |

### Grounds — two different things, do not conflate them

**The marker button goes to the Uno's own `GND` pin.** D2 is pulled up internally to the Uno's
5 V, so its return must be the Uno's ground. The button is a local Uno peripheral; nothing about
it touches the toy.

**The measurement ground is one single wire, and where you land it matters.** It is the reference
for all five analog readings, and the toy's ground carries *amps* of motor return current. A drop
of even a few millivolts along that path lands directly on every ADC reading.

- Land it at the **ground pin of a `DWQ` connector** — the same node the pots' bottom ends sit
  on. Any drop between there and battery negative is then common to both the pot divider and the
  ADC reference, and cancels.
- **Not** at the battery negative terminal, and **not** at a motor connector. Those carry the
  motor return current you are trying to stay out of.
- **One ground wire only.** A second creates a loop through the toy's ground plane, and motor
  current in that loop becomes noise on the trajectories.
- Which outer pin of the `DWQ` connector is ground is still `ASSUMED` — white and red are the
  track ends, but which is which has not been metered. Buzz it to board `GND` with the toy
  **powered off** before connecting anything.

If a capture comes back with all four legs showing the same small wiggle at motor-switching
frequency, this is the cause — the ground reference is picking up motor current, not the legs
moving together.

- **Back-probe at the connector housings. Cut nothing.**
- **Do not power the pots from the Uno.** The stock board supplies them at 3.3 V; we only read
  the wipers, high-impedance. Driving both ends fights the stock rail.
- **Leave `AREF` unconnected.** Rationale in the sketch header — the resolution gain is small and
  the failure mode is a damaged chip.

The marker button is optional but recommended: press it as you press the remote, so each
behaviour is delimited inside the capture rather than guessed at afterwards.

## Use

Build and upload with the Arduino IDE or `arduino-cli`, board `arduino:avr:uno`.

Then, per behaviour:

```bash
uv run --with pyserial tools/capture.py sit
uv run --with pyserial tools/capture.py walk_forward --seconds 10
```

Files land in `hardware/captures/<name>.csv`. The script auto-detects the port, waits out the
Uno's DTR reset, starts the stream, and on exit collects the sketch's trailer so the **overrun
count is always recorded**. Non-zero overruns mean samples were dropped and the timing has gaps —
that capture is not trustworthy for period measurement.

Interactively, any serial monitor at **500000 baud** works:

| Key | Effect |
|---|---|
| `s` | start streaming |
| `x` | stop, print trailer (sample count, overruns, marks) |
| `r` | print **one** sample immediately |
| `m` | software marker |
| `h` | reprint header |

## Wiring sanity check — do this before the first capture

The sketch was bench-verified on 2026-08-04 with **nothing connected**: 201 samples/s, period
5000 µs mean with ±12 µs jitter, **0 overruns**. Floating inputs read ~290–350 counts, which is
meaningless noise — that is what "not wired up" looks like.

Once wired, press `r` and check against these before capturing anything:

| Channel | Expected | If it disagrees |
|---|---|---|
| `rail` | **≈ 676 counts** (3.3 V at the default 5 V reference) | Much lower → the ground or rail tap is wrong, or you are on the wrong outer pin. Near 1023 → you are on a 5 V node, not the pot rail. |
| `fl` `fr` `rl` `rr` | somewhere in **0 – 676**, and each one **changes when you move that leg by hand** | Stuck at ~1023 or 0 → not on the wiper, or the connector is unplugged. Doesn't move → wrong leg, or you are on a track end rather than the centre pin. |

Move each leg one at a time and confirm **only** that leg's column responds. That single check
catches every cross-plugged tap, and it is far cheaper than discovering it after eight captures.

## The `r` command does two other Phase 0 jobs

- **Step 2a — pot range at the stops.** Move a leg by hand to each hard stop, press `r`, record.
  Gives the software limits and the counts-per-degree scale.
- **Step 2.5 — foot-path calibration.** At each hand-positioned step, press `r` and measure the
  foot position off the grid. Those pairs become the leg model.

## Output

```
# quaddle-robot pot_logger_uno v1
# sample_hz=200 adc_bits=10 aref=DEFAULT_5V pot=B103_10k_330deg rail_nominal=3.3V
# ch: fl=A0(ZQDWQ) fr=A1(YQDWQ) rl=A2(ZHDWQ) rr=A3(YHDWQ) rail=A4
# mark increments on each button press / 'm' command
t_us,fl,fr,rl,rr,rail,mark
1048576,412,398,455,441,676,0
```

`t_us` is `micros()` at sample time — taken *before* the print, so serial blocking cannot leak
transmission jitter into the timing. Comment lines start with `#`; skip them when parsing.

Counts are raw 10-bit at the 5 V reference. To normalise ratiometrically, divide by the `rail`
column rather than by a constant — that is what the fifth channel is for.

## Capture list

Body on a stand for poses, on the floor for walking. Record which, per run.

| Behaviour | Captured | Stand/floor | Notes |
|---|---|---|---|
| stand → sit | | | |
| sit → stand | | | |
| walk forward | | | |
| walk backward | | | |
| turn left | | | |
| turn right | | | |
| lie down | | | |
| paw / handshake | | | |
| dance / demo | | | |
