# quaddle-robot

Turning an **APIDPOWER "SMART DOG"** 18-inch toy quadruped into a programmable robot with a
custom controller and on-board real-time AI.

The toy's mechanism — chassis, linkages, gearmotors, potentiometers — is good and stays.
Everything electrical above it gets replaced: a Raspberry Pi 5 for vision and behaviour, an
RP2350 for the real-time gait and safety loops, our own motor drive and power tree, our own
handheld remote and a browser app.

The stock PCB is kept **intact and reinstallable**. It is the rollback path and the only working
reference for what the mechanism can do.

> Named after Petoi's [Quaddle](https://www.petoi.com/pages/quaddle-educational-robot-kit) — the
> **capability target**, not the hardware we own. Quaddle is servo-driven; our legs are brushed
> DC motors, so its firmware does not port to our drive layer.

---

## Status

**Phase 0 — teardown & characterisation.** The machine is characterised, a measurement rig is
built and verified, and the next step is capturing the stock motion library.

| | |
|---|---|
| Architecture | Locked — position control, 4 actuators, 1 DOF per leg |
| Measurement rig | Built, uploaded, verified — 200 Hz, 0 dropped samples |
| Captures taken | None yet |
| Open questions | Q3 stall current · Q4 battery · Q5 true stops · Q8 driver paralleling · Q9 foot path · Q10 `U5` identity |
| Parts ordered | None — and none will be until the measurements that select them exist |

---

## New here?

Start with **[`docs/00_START_HERE.md`](docs/00_START_HERE.md)**. It covers what the project is,
the five rules, safety, bench setup, and your first hour.

Then read **[`docs/HOW_IT_WORKS.md`](docs/HOW_IT_WORKS.md)** — the engineering behind the robot
from first principles, grounded in this machine's real measured numbers.

---

## Documents

| Document | Contents |
|---|---|
| [`docs/00_START_HERE.md`](docs/00_START_HERE.md) | Onboarding, the five rules, safety, bench setup |
| [`docs/HOW_IT_WORKS.md`](docs/HOW_IT_WORKS.md) | The engineering explained — servo loops, H-bridges, gait, the two-brain split |
| [`docs/NEXT_SESSION.md`](docs/NEXT_SESSION.md) | Exact state and what to do next — read before every session |
| [`docs/TASKS.md`](docs/TASKS.md) | Assignable backlog with acceptance criteria and learning objectives |
| [`docs/HARDWARE_REFERENCE.md`](docs/HARDWARE_REFERENCE.md) | What is `PROVEN` / `ASSUMED` / `UNKNOWN` about the donor toy |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Target system, compute split, power tree, link protocol, risks |
| [`docs/RUNBOOK.md`](docs/RUNBOOK.md) | Step-by-step bench procedures |
| [`docs/PHASE_0_TEARDOWN.md`](docs/PHASE_0_TEARDOWN.md) | The measurement checklist that currently blocks everything |
| [`docs/PHASE_0_SWD_AND_LOGIC.md`](docs/PHASE_0_SWD_AND_LOGIC.md) | Non-destructive probing of the stock board |
| [`docs/GLOSSARY.md`](docs/GLOSSARY.md) | Terms, the board's Chinese silkscreen, key numbers |
| [`CLAUDE.md`](CLAUDE.md) | Execution contract: guardrails, phase gates, dated state log |

---

## Tools

| Tool | What it does |
|---|---|
| [`tools/pot_logger_uno/`](tools/pot_logger_uno/) | Arduino Uno rig logging all four leg potentiometers + supply rail at 200 Hz |
| [`tools/capture.py`](tools/capture.py) | Captures one behaviour run to `hardware/captures/<name>.csv` |
| [`tools/live.py`](tools/live.py) | Refreshing live readout — wiggle a leg, watch which channel moves |

```bash
arduino-cli compile --fqbn arduino:avr:uno -u -p /dev/cu.usbmodem1301 tools/pot_logger_uno
uv run --with pyserial tools/live.py
uv run --with pyserial tools/capture.py walk_forward --seconds 10
```

---

## The machine in one table

| | |
|---|---|
| Actuators | 4 brushed DC gearmotors, **one per leg**, 1 DOF each |
| Feedback | 4 × `B103` potentiometer, 10 kΩ linear, 330° travel, on the **output** shaft |
| Pot supply | 3.3 V — ratiometric with the ADC reference |
| Stock brain | JieLi BT-audio SoC + a separate ARM MCU (`U5`) for motion — the same two-brain split we independently designed |
| Stock drive | 4 × `MX1616S` dual H-bridge (8 channels, 4 used) |
| Board | `JXD-8002-Blue-YW-RV2`, 2024-01-19 — no documentation exists online |
| Can do | Walk, turn, speed control, static poses, body height/pitch/roll while standing |
| Cannot do | Strafe, abduct, dynamically balance, backflip |

---

## Layout

```
docs/              design, reference and teaching documents
hardware/photos/   board and mechanism photographs (evidence)
hardware/captures/ logged joint trajectories
hardware/dumps/    flash dumps, read-only
tools/             bench instrumentation
firmware/          RP2350 — gait, joint control, safety   (Pico SDK, C)
host/              Raspberry Pi 5 — vision, behaviour, API (Python 3.12)
remote/            ESP32-S3 handheld controller            (ESP-IDF)
```
