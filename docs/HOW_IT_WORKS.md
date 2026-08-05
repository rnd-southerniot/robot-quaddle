# How it works — the engineering behind the robot

The teaching document. Everything here is grounded in what we actually measured on **this**
machine, so the numbers are real. Where something has not been measured yet it says so.

Read §1–§4 before writing any control code. Read §5–§7 before designing a gait. Read §8–§10
before touching the Pi.

Each section ends with **Check yourself** — questions you should be able to answer before moving
on. They are not busywork; each one is a mistake somebody makes on this kind of project.

---

## 1. What the machine physically is

Four legs. **One brushed DC motor per leg**, two wires each. **One potentiometer per leg**, three
wires each, mounted in the gearbox housing and geared to the **output** shaft. That is the whole
actuation system — no head motor, no tail motor, confirmed by counting the connectors on the
board, which the manufacturer helpfully labelled.

```
        FRONT
    FL ─────── FR          FL = ZQDJ motor + ZQDWQ pot   (white connectors  = left)
    │           │          FR = YQDJ motor + YQDWQ pot   (yellow connectors = right)
    │   body    │          RL = ZHDJ motor + ZHDWQ pot
    │           │          RR = YHDJ motor + YHDWQ pot
    RL ─────── RR
        REAR
```

**One degree of freedom per leg.** This is the single most important fact about the machine, and
it determines what is and is not achievable:

| Achievable | Not achievable |
|---|---|
| Walk forward and backward | Strafing sideways |
| Turn | Leg abduction (swinging a leg outward) |
| Speed control | Dynamic balance recovery |
| Static poses — sit, lie, bow, paw | Backflips, ceiling walking |
| Body height, pitch and roll while standing | Anything needing more than 4 actuated variables |

Do not promise Quaddle-class agility. Quaddle also has 4 servos, but a different linkage and a
much lighter body. Our capability envelope is what §7 describes.

### Check yourself
- How many actuated degrees of freedom does the whole robot have? *(Four.)*
- Why can it not strafe? *(A leg can only move along one path; there is no sideways axis.)*

---

## 2. Each leg is a servo built from parts

A hobby RC servo contains exactly three things: a **DC motor**, a **gearbox**, and a
**potentiometer on the output shaft** — plus a small circuit that closes the loop between the pot
reading and the commanded position.

This toy has the first three, discretely, with the loop closed in the main board's firmware
instead of inside a servo case. That is why the design looked strange at first — four H-bridge
chips is not something you see on a servo-driven robot — and why it is actually good news.

```
   command ──►┌──────┐   PWM   ┌──────────┐    ┌─────────┐    ┌─────┐
              │ PID  ├────────►│ H-bridge ├───►│ DC motor├───►│ gear├──► leg
          ┌──►└──────┘         └──────────┘    └─────────┘    │ box │
          │                                                   └──┬──┘
          │        ┌───────────────┐                             │
          └────────┤ potentiometer │◄────────────────────────────┘
             angle └───────────────┘   reads the OUTPUT shaft
```

### Why the pot being on the *output* shaft matters

A gearbox has **backlash** — slack between the teeth. When the motor reverses, it turns a few
degrees before the output moves at all.

If the sensor were on the *motor* shaft, the controller would think the leg had moved when it had
not. Position error would be invisible and uncorrectable.

Because our pot reads the **output**, the loop closes on where the leg genuinely is. Backlash
sits *inside* the loop. That means:

- Position is always true. Good.
- But the loop has a **dead zone**: reverse the command and nothing happens for a moment, then
  everything happens at once. A naive PID responds to "no movement" by increasing output, then
  the slack takes up and the leg lurches past target. This is how you get **hunting** — an
  oscillation that sounds like chattering and eats gear teeth.
- Therefore **slew limits and a deadband are mandatory**, not optional polish. See §4.

### Check yourself
- Why is a sensor on the output shaft better than on the motor shaft?
- What failure mode does gearbox backlash cause in a PID loop, and what prevents it?

---

## 3. Driving a DC motor: the H-bridge

A brushed DC motor spins one way when you apply voltage, the other way when you reverse it. To
reverse it from a microcontroller you need four switches in an **H** around the motor:

```
      +V                     +V
      │                      │
   ┌──┴──┐               ┌───┴──┐
   │ A   │               │  B   │       A + D on  ->  current left-to-right  -> forward
   └──┬──┘               └───┬──┘       B + C on  ->  current right-to-left  -> reverse
      ├────[ MOTOR ]─────────┤          A + B on  ->  both terminals to +V   -> BRAKE
   ┌──┴──┐               ┌───┴──┐       C + D on  ->  both terminals to GND  -> BRAKE
   │ C   │               │  D   │       all off   ->  motor terminals open   -> COAST
   └──┬──┘               └───┬──┘
      │                      │
     GND                    GND
```

**Never turn on A and C together** (or B and D) — that is a direct short across the supply,
called *shoot-through*. Integrated driver chips prevent this internally, which is one reason we
use them rather than discrete FETs.

The stock board uses four **`MX1616S`** chips, each containing two H-bridges. Eight channels for
four motors, so half are spare — whether they are paralleled onto the same motors for more
current is open question **Q8**, and the answer changes which driver we buy.

### Speed control: PWM

You do not vary the voltage. You switch it fully on and off, quickly, and vary the **fraction of
time it is on** — the duty cycle. The motor's inductance and inertia average it out.

- **Too low a switching frequency** (< ~15 kHz) puts the switching in the audio band, and the
  motor whines audibly.
- **Too high** and the driver's switching losses climb and it gets hot.

We will read the stock board's choice off the logic analyser rather than guess — the original
designer already found a value that works with these exact motors.

### Two ways to wire PWM onto an H-bridge

| Scheme | How | Trade-off |
|---|---|---|
| **Sign-magnitude** | One input gets the PWM, the other sits low. Direction is which input carries it. | Simple, less ripple current, motor coasts during off-time |
| **Lock-antiphase** | Both inputs get complementary PWM. 50 % duty = stopped, above = forward, below = reverse. | Better low-speed control and it holds position more stiffly, but current flows constantly, so more heating |

Which the stock firmware uses is something the Saleae capture will tell us.

### Brake vs coast

At the end of a move, shorting both motor terminals together **brakes** — the motor becomes a
generator driving into a short, and resists turning. Leaving them open lets it **coast**.

For a legged robot this matters: braking helps hold a pose against gravity without burning
holding current. Whether the stock firmware brakes is on the capture list.

### Check yourself
- What is shoot-through and what prevents it?
- Why does a motor whine at some PWM frequencies?
- Your robot must hold a standing pose. Brake or coast? Why?

---

## 4. Closing the position loop

### From volts to degrees

The potentiometer is a **`B103`** — `B` = linear taper, `103` = 10 × 10³ = **10 kΩ** — with
**330°** of electrical travel. It is wired as a voltage divider across the 3.3 V rail, and the
wiper voltage is proportional to shaft angle.

Measured on our Uno rig, referenced to its 5 V ADC reference:

| Quantity | Value |
|---|---|
| Pot supply rail | 3.3 V, **measured 3.289 V = 674 counts** |
| Full 330° of track | 674 counts |
| **Scale** | **0.49° per count** |
| Noise, peak-to-peak | 3–5 counts ≈ ±1° |
| Leg travel actually used | ~170°, so roughly half the track |

On the production RP2350 at 12 bits referenced to 3.3 V, the same pot gives **0.081°/count** —
six times finer. Resolution is not our limiting factor; **gearbox backlash is**.

### Ratiometric measurement — why we log the rail

The pot is a divider. Its wiper voltage is `Vrail × (angle / 330°)`. If `Vrail` sags, every
reading scales with it — the leg has not moved but the number changes.

Two defences:

1. **Power the pot from the same rail that feeds the ADC reference.** Then the sag appears in
   both numerator and denominator and cancels exactly. The stock designer did this, and our board
   copies it.
2. **Log the rail as a fifth channel** so any capture can be normalised afterwards. We saw the
   rail move from 673 to 707 counts during one session — a 5 % excursion that would otherwise
   have looked like 17° of phantom leg movement.

### The control loop

```
target angle ──► [ slew limiter ] ──► [ + ] ──► [ PID ] ──► [ PWM + direction ] ──► H-bridge
                                       ▲ −                                             │
                                       │                                               ▼
                                       └────────── [ ADC, oversampled ] ◄──── potentiometer
```

Runs at **200 Hz**, all four legs in one timer callback.

**Proportional** — output proportional to error. Alone, it leaves a steady-state error, because
as the error shrinks so does the effort, until it can no longer overcome friction.

**Integral** — accumulates error over time, so persistent small errors eventually produce enough
effort. This is what kills the steady-state error. It is also what causes **wind-up**: if the leg
is blocked, the integral grows without limit, and when the obstruction clears the leg slams. Clamp
it.

**Derivative** — responds to rate of change, damping overshoot. On a noisy pot signal it amplifies
noise into torque, so filter the input or leave D small.

**The non-negotiable additions for this machine:**

| Guard | Why |
|---|---|
| **Slew limit on the setpoint** | The trajectory generator must never ask for a step change. A step is an infinite-velocity command and the loop answers it with maximum current. |
| **Deadband around the target** | Backlash means the leg cannot be positioned more precisely than the slack. Chasing error smaller than the backlash produces permanent hunting. |
| **Integral clamp** | Prevents wind-up during a stall or against a limit. |
| **Software angle limits, 5° inside the measured stops** | Enforced *below* the trajectory generator so no command path can bypass them. |
| **Stall detection** | Current above threshold **and** angle not changing, both, for 200 ms → cut that channel and raise a fault. Current alone false-trips on every acceleration. |

### Check yourself
- The rail sags 5 % under motor load. Which of the two defences fixes it, and why does the other
  one only help afterwards?
- Why must stall detection require *two* conditions rather than just high current?
- What is integral wind-up and when would this robot experience it?

---

## 5. From joint angle to foot position

A conventional quadruped leg has 2–3 joints, and you solve **inverse kinematics** — given a
desired foot position, compute the joint angles.

**We do not have that problem.** With one actuator per leg, the foot cannot go anywhere it likes.
It travels along a **single fixed curve** set by the linkage, parameterised by one number.

```
    foot_x, foot_z  =  legpath(theta)
```

There is no equation to solve. There is a **curve to measure**, once per leg, and then a lookup.

That measurement is open question **Q9** and it is done by hand: clamp the body, put a
graph-paper grid behind a leg, step the leg from stop to stop, and record (pot reading, foot x,
foot z) at each step. Fit a curve. Store the table.

Every leg gets its **own** table. They will not be identical — manufacturing tolerance on a $99
toy is not tight, and our four hand-swept ranges already differ by 40°.

Once you have `legpath()`, gait design reduces to choosing a `theta(t)` profile and four phase
offsets. All of it is pure arithmetic, and all of it gets **host-native unit tests before it
touches hardware**. Never debug maths on a robot.

### Check yourself
- Why is there no inverse kinematics to solve on this machine?
- Why does each leg need its own calibration table rather than one shared one?

---

## 6. What a gait actually is

A gait is a **repeating pattern of which feet are on the ground and when**.

Each leg cycles through **stance** (foot planted, pushing the body along) and **swing** (foot
lifted, returning to the start). The fraction of the cycle spent in stance is the **duty factor**.

- Duty factor > 0.5 → more than half the legs are down at any moment → a **walk**, statically
  stable, slow.
- Duty factor ≤ 0.5 → **trot** and faster gaits, dynamically stable, and they need active balance
  we do not have the DOF for.

**The gait is defined by the four phase offsets** — where each leg sits in the cycle relative to
the others:

| Gait | FL | FR | RL | RR | Notes |
|---|---|---|---|---|---|
| **Walk** | 0.0 | 0.5 | 0.25 | 0.75 | One foot lifted at a time. Three always down. Start here. |
| **Trot** | 0.0 | 0.5 | 0.5 | 0.0 | Diagonal pairs move together. Faster, needs balance. |
| **Bound** | 0.0 | 0.0 | 0.5 | 0.5 | Front pair, then rear pair. |

Changing gait is changing four constants. That is genuinely all it is.

> **Interesting observation, not yet a finding.** At rest our four legs sit in diagonal pairs —
> FL ≈ 175°, RR ≈ 172°, FR ≈ 155°, RL ≈ 158°. That is what you would see if the toy parks in a
> trot stance. The walk captures will confirm or kill it. Recorded as a hypothesis, per rule R1.

**Static stability** means the body's centre of mass stays inside the triangle formed by the feet
that are down. With three feet down that triangle exists and the robot cannot fall. With two it
does not, and you are relying on momentum. Given our DOF budget, **stay statically stable.**

### Turning

With no sideways DOF, you turn by making one side take longer strides than the other — the same
principle as a tracked vehicle. Differential stride length, not differential wheel speed.

### Check yourself
- What is duty factor, and what value makes a gait statically stable?
- Why does a trot need active balance when a walk does not?
- How does this robot turn without a steering joint?

---

## 7. Body pose: four inputs, three outputs

While standing, the four leg positions determine the body's **height**, **pitch** and **roll**.
Four inputs, three outputs — the system is **over-determined by one**, which means there are many
leg combinations giving the same body pose, and you can pick among them.

That gets you:

- **Levelling on a slope** — sense body attitude with the IMU, adjust the four legs to keep the
  body flat.
- **Bowing, crouching, standing tall** — useful expressive behaviours, and useful for the camera.
- **Tilting to track a face** — the vision layer wants to look up at a standing person.

What it does **not** get you is yaw (that needs stepping) or lateral translation (no sideways DOF).

### Check yourself
- Why is having four inputs for three outputs useful rather than wasteful?
- Which body motions are unavailable while standing still, and why?

---

## 8. Why two brains

```
┌────────────────────────────┐         ┌─────────────────────────────┐
│  Raspberry Pi 5            │  intent │  RP2350                     │
│  "cortex" — best effort    ├────────►│  "spinal cord" — real time  │
│                            │◄────────┤                             │
│  camera, detection,        │  state  │  200 Hz gait + position     │
│  behaviour, WebRTC, voice  │         │  loops, safety, IMU         │
└────────────────────────────┘         └──────────────┬──────────────┘
       Linux. Can pause.                              │
       Can crash. Can be                              ▼
       rebooted mid-walk.                     H-bridges, motors
```

Linux is a **best-effort** system. The scheduler can stop your process for tens of milliseconds
to service something else, and usually that is invisible. In a 200 Hz control loop, a 50 ms pause
means ten missed cycles with a motor energised. That is how you strip a gearbox.

So the split is by **timing requirement**, not by capability:

- The **RP2350** has no OS. Its timer interrupt fires when it says it will. It owns everything
  that must happen on time: the position loops, stall detection, the watchdog, the e-stop.
- The **Pi 5** does everything that can tolerate jitter: vision, behaviour, the web interface.

### The intent rule

**The Pi never sends raw PWM. It sends intent** — "walk forward at 0.2 m/s", "stand 40 mm tall",
"trot". The MCU decides how.

This is not stylistic. It means the robot is safe **with the Pi absent**. If the Pi crashes, the
cable falls out, or WiFi drops, the MCU notices the missing command frames and independently
ramps to a stable stand, then disables the drivers.

> **Autonomy that can be lost must not be autonomy the robot depends on to stay safe.**

The link runs UART at 460800 baud with COBS framing and a CRC-16 trailer: commands at 50 Hz,
state at 100 Hz, sequence numbers echoed so latency is *measured* rather than assumed. Miss three
command frames — 100 ms — and the deadman fires.

### Check yourself
- Give a concrete failure that the intent rule prevents.
- Why can't the gait loop simply run as a high-priority Linux thread?
- What happens to the robot if you unplug the Pi mid-walk?

---

## 9. The perception and behaviour layer

```
camera ──► detector ──► tracker ──► behaviour ──► intent ──► [ link ] ──► MCU
```

- **Detector** finds people or objects in a frame. Runs on the Pi 5's CPU first; a Hailo
  accelerator gets added **only if measurement shows the CPU cannot hold the frame rate**. Buying
  it before measuring would violate rule R5.
- **Tracker** gives detections continuity across frames, so "person 1" stays person 1. Without
  this, a follow behaviour jitters between subjects.
- **Behaviour** turns a tracked target into intent. *Follow-me* is a controller in its own right:
  target distance in, forward velocity and turn rate out — with its own deadband, or the robot
  oscillates back and forth around its set distance.

Note the pattern repeats at every layer: **a sensor, a setpoint, an error, a limited response.**
The follow-me loop and the joint position loop are the same shape at different timescales.

### Check yourself
- Why does the follow-me behaviour need a deadband?
- Why do we not buy the AI accelerator now?

---

## 10. The measurement chain, end to end

Worth tracing once, because every layer can lie to you.

| Stage | What can go wrong | Our defence |
|---|---|---|
| Pot wiper | Worn carbon track → dropouts read as position jumps | Check the sweep is smooth and monotonic; replace, don't filter around it |
| Wiring | Ground drop from motor current appears as signal | Single ground wire, landed at the `DWQ` connector ground, not battery negative |
| ADC input | Source impedance too high for the sample-and-hold | Pot divider is 2.5 kΩ worst case — comfortably fine, no buffer needed |
| Multiplexer | Residue from the previous channel appears as motion | Discard one conversion after each channel change |
| Reference | Rail sag scales every reading | Ratiometric wiring, plus log the rail |
| Timing | Timestamp taken after a blocking write encodes transmission jitter | Timestamp at sample time, always |
| Dropped samples | Silent gaps make gait period wrong | Count missed deadlines and report them; a capture with unknown overruns is not usable |

Every one of those is implemented in `tools/pot_logger_uno/`. Read the sketch — it is short, and
it is a worked example of instrumenting something honestly.

### Check yourself
- You capture a walk and all four legs show the same small wiggle at motor-switching frequency.
  What is the most likely cause?
- Why is a timestamp taken after `Serial.print()` untrustworthy?

---

## 11. Where the difficulty actually is

New engineers expect the gait maths to be the hard part. It is not. In rough order of pain:

1. **Not knowing what the hardware does.** Solved only by measuring. This is Phase 0 and it is
   most of the intellectual work.
2. **Mechanical slop.** Backlash, flexing plastic, four legs that are not identical. No amount of
   control theory removes it; you design around it.
3. **Power.** Motors and a Pi 5 on one battery, with motor transients not browning out the
   compute.
4. **Failure handling.** Every layer has to behave when the one above it disappears.
5. **The gait maths.** Genuinely the easy part, and it is host-testable without hardware.

Spend your effort accordingly.
