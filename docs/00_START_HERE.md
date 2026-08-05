# Start here

You have been handed a project that is **partly reverse-engineered and deliberately unfinished**.
That is the point. This is a learning module built on a real machine with real unknowns, not a
tutorial with the answers in the back.

Read this page fully before touching hardware. It takes ten minutes and will save you a gearbox.

---

## 1. What the project is

We bought an **APIDPOWER "SMART DOG"** — a 45 cm (18 inch) Chinese toy quadruped, about $99. It
walks, sits and dances from a bundled RF remote. Mechanically it is good: four legs, decent
linkages, a body that holds together.

Electrically it is a dead end. The brain is a closed Bluetooth-audio SoC we cannot program, and
its behaviour is a fixed list of canned tricks.

**The project replaces everything above the mechanism** — our own motor drive, our own real-time
controller, our own remote — and then adds what the toy never had: a Raspberry Pi 5 with a
camera, running detection and behaviour on board.

The toy's own PCB is kept **intact and reinstallable**. That is a hard rule, and it exists so
that when something goes wrong you always have a known-good machine to compare against.

> **The project is named after Petoi's *Quaddle*, which we do not own.** Quaddle is an
> open-source 4-servo desk quadruped and it is our *capability target*, not our hardware. Do not
> follow Quaddle or OpenCat tutorials for the drive layer — they assume RC servos and our legs
> are brushed DC motors. See `HARDWARE_REFERENCE.md` §1.

---

## 2. What you are actually learning

The robot is the excuse. The transferable skills are:

| Skill | Where you meet it |
|---|---|
| **Reverse-engineering undocumented hardware** | The whole of Phase 0. No datasheet, no schematic, no manual exists. |
| **Evidence discipline** — separating what you measured from what you assumed | Every line of `HARDWARE_REFERENCE.md` |
| **Closed-loop motor control** from first principles | Building a servo out of a motor, a gearbox and a potentiometer |
| **Real-time vs best-effort computing** | Why the gait loop cannot live on Linux |
| **Designing for failure** | Link loss, stall, brownout, e-stop |
| **Instrumentation** | Building the tool that measures the thing before building the thing |
| Embedded C, Python, ESP-IDF, Pi | Phases 1–5 |

If you finish this project you will have built a legged robot from a toy, and — more usefully —
you will know how to attack hardware nobody has documented.

---

## 3. Reading order

Do not read everything. Read in this order and stop when you have enough to do your task.

| # | Document | Why | When |
|---|---|---|---|
| 1 | **This page** | Orientation and safety | Now |
| 2 | [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) | The engineering behind the robot, from first principles, using our real measured numbers | Before any design or code |
| 3 | [`NEXT_SESSION.md`](NEXT_SESSION.md) | Exactly where the work stopped and what to do next | Before every session |
| 4 | [`TASKS.md`](TASKS.md) | The assignable backlog with acceptance criteria | To pick up work |
| 5 | [`HARDWARE_REFERENCE.md`](HARDWARE_REFERENCE.md) | What is proven / assumed / unknown about the toy | When you need a fact about the hardware |
| 6 | [`ARCHITECTURE.md`](ARCHITECTURE.md) | Target system, compute split, power, link protocol, risks | Before writing anything structural |
| 7 | [`RUNBOOK.md`](RUNBOOK.md) | Step-by-step bench procedures | At the bench |
| 8 | [`PHASE_0_TEARDOWN.md`](PHASE_0_TEARDOWN.md) | The measurement checklist currently blocking everything | When doing Phase 0 work |
| 9 | [`PHASE_0_SWD_AND_LOGIC.md`](PHASE_0_SWD_AND_LOGIC.md) | Non-destructive probing of the stock board | When doing Phase 0 work |
| 10 | [`GLOSSARY.md`](GLOSSARY.md) | Terms and abbreviations, including the board's Chinese silkscreen | Whenever something is unfamiliar |
| 11 | [`../CLAUDE.md`](../CLAUDE.md) | The execution contract: guardrails, phase gates, running state log | Skim now, revisit at each phase |

---

## 4. The five rules

These are not suggestions. Breaking any of them costs the project real time or real hardware.

### R1 — Label every claim: `PROVEN` / `ASSUMED` / `UNKNOWN`

This is undocumented hardware. The physical unit is the only authority. Write down what you
**measured**, separately from what you **inferred**, and never let a photo or a distributor
webpage be recorded as a measurement.

Why it matters: an unknown that gets written down as a fact is inherited by the next person, who
plans around it. We have already had to correct two of these — a wire-colour guess and a
motor-count inference. Both were caught because they were labelled honestly.

### R2 — The stock PCB is never cut, desoldered, erased or reflashed

It is the rollback path and the only working reference for what the mechanism can do.

**Reading is encouraged** — SWD attach-without-halt, logic capture, flash dumps. **Writing is
forbidden**, and that includes every tool's helpful "unlock" / "remove read protection" /
"auto mass-erase on connect" button.

### R3 — No motor is commanded without a current limit and a stall timeout

From Phase 1 onward, no exceptions. Toy gearboxes are nylon. A stalled brushed motor draws
locked-rotor current until something gives, and what gives is usually the gear teeth.

### R4 — Body on a stand, feet off the ground

For all bench work through Phase 2. A robot that unexpectedly stands up on a bench falls off it.

### R5 — No parts are ordered before the measurement that selects them exists

The motor driver is a function of the measured stall current. The AI accelerator is a function
of the measured inference rate. Lead time from the China warehouse is not a reason to guess —
guessing wrong costs more time than waiting.

---

## 5. Safety

- **Li-ion pack of unknown provenance.** Never charge unattended. Never bench-run from the pack
  with the charger attached until the charge topology is understood (that is open question Q4).
- **Pinch points.** This is a 45 cm machine with linkages. Fingers out of the legs when powered.
- **Stalled motors get hot fast.** If a motor buzzes and does not move, kill power immediately —
  do not wait to see whether the firmware notices.
- **The RP2350 ADC is not 5 V tolerant.** Our pot rail is 3.3 V and must stay that way.

---

## 6. Set up your bench

```bash
git clone <repo> && cd quaddle-robot

# Firmware / tools
brew install --cask gcc-arm-embedded
brew install arduino-cli cmake
arduino-cli core install arduino:avr

# Host side
brew install uv
```

Confirm the Arduino toolchain works by building the existing logger — this touches no hardware:

```bash
arduino-cli compile --fqbn arduino:avr:uno tools/pot_logger_uno
```

Expect roughly `Sketch uses 3808 bytes (11%)`. If that builds, your toolchain is good.

---

## 7. Your first hour

1. Read [`HOW_IT_WORKS.md`](HOW_IT_WORKS.md) §1–§4. That is the core of the machine.
2. Read [`NEXT_SESSION.md`](NEXT_SESSION.md) to see where things stand.
3. Build the logger sketch as above.
4. Pick a task from [`TASKS.md`](TASKS.md) marked **`good first task`**.
5. Before you touch the robot, find the person holding the bench and confirm nobody else has it
   half-disassembled.

---

## 8. How to record what you find

- **Bench measurements** go into the tables in `PHASE_0_TEARDOWN.md`. Not into chat, not into
  your notebook, not into a message.
- **Photos** go into `hardware/photos/` with descriptive filenames — `pcb-u5-marking.jpg`, not
  `IMG_4471.jpg`.
- **Captures** go into `hardware/captures/`, one file per behaviour.
- **Decisions and findings** get a dated entry appended to the **State** block at the bottom of
  `CLAUDE.md`. Include *why*, not just *what* — the reasoning is the part that is expensive to
  reconstruct.
- **If you correct something**, fix the document too, not just the conversation. A wrong claim
  left in a doc will be inherited.
