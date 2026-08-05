/*
 * pot_logger_uno — Phase 0 joint-trajectory logger for quaddle-robot (P0-LOG)
 *
 * Reads the four leg potentiometers of the stock APIDPOWER SMART DOG board while the
 * toy runs its own firmware, and streams timestamped CSV over USB serial. The wipers
 * are the joint trajectories: capturing them gives us the stock motion library as
 * measured data rather than derived guesses.
 *
 * Also serves two other Phase 0 steps, via the 'r' command:
 *   - Step 2a  pot range at both mechanical stops
 *   - Step 2.5 foot-path calibration (pot value at each hand-positioned step)
 *
 * WIRING — read this before connecting anything
 *   - ONE ground wire between the Uno and the toy, connected FIRST, every time. Land it on
 *     the GND pin of a DWQ connector, NOT on battery negative and NOT on a motor connector:
 *     the toy's ground carries amps of motor return current, and any drop along that path
 *     appears directly on every ADC reading. At the DWQ ground pin the drop is common to
 *     both the pot divider and our ADC reference, so it cancels. A second ground wire makes
 *     a loop and puts motor current onto the measurement — use one.
 *   - The D2 marker button returns to the UNO's own GND pin, not the toy's. D2 is pulled up
 *     internally to the Uno's 5 V, so it is a local circuit.
 *   - Tap the BLUE (centre pin) wire of each DWQ connector. Back-probe at the
 *     housing. Cut nothing.
 *   - Do NOT power the pots from the Uno. The stock board supplies them at 3.3 V;
 *     we only read the wipers, high-impedance. Driving both ends fights the stock rail.
 *   - AREF is deliberately left unconnected. See the note on resolution below.
 *
 *   A0 <- ZQDWQ wiper   front-left   (white housing)
 *   A1 <- YQDWQ wiper   front-right  (yellow housing)
 *   A2 <- ZHDWQ wiper   rear-left    (white housing)
 *   A3 <- YHDWQ wiper   rear-right   (yellow housing)
 *   A4 <- pot supply rail (either outer pin of any DWQ) — the pots are ratiometric,
 *         so this is what lets captures be normalised if the rail ever moves
 *   D2 <- marker button to the UNO's GND (uses the internal pull-up)
 *
 * RESOLUTION — why the default 5 V reference is fine
 *   A 0–3.3 V wiper at the default AREF spans 676 of 1024 counts across the pot's
 *   330 degrees of electrical travel, so roughly 185 counts across a ~90 degree leg
 *   arc — about 0.5 deg/count. Gearbox backlash is far larger than that, so it is
 *   not the binding constraint.
 *
 *   Tying AREF to 3.3 V would give ~276 counts instead. That is not worth the hazard:
 *   if analogReference(EXTERNAL) does not execute before the first analogRead(), the
 *   internal reference is shorted to AREF and the chip can be damaged — and the sketch
 *   cannot control the wiring state at power-on. Leave AREF alone.
 *
 * ADC CLOCK
 *   The stock Arduino prescaler of 128 gives a 125 kHz ADC clock, inside the
 *   ATmega328P's 50–200 kHz window for full 10-bit accuracy. It is deliberately NOT
 *   sped up: at 200 Hz the sampling costs ~1.1 ms of a 5 ms budget, so there is
 *   nothing to gain and linearity is the whole point of this measurement.
 *
 * COMMANDS (single characters over serial)
 *   s  start streaming        x  stop streaming and print the run trailer
 *   r  print one sample immediately (calibration use)
 *   m  software marker, equivalent to the button
 *   h  reprint the header
 */

#include <Arduino.h>

// ---------------------------------------------------------------- configuration

static const uint8_t  PIN_POT[4] = {A0, A1, A2, A3};   // FL, FR, RL, RR
static const uint8_t  PIN_RAIL   = A4;
static const uint8_t  PIN_MARK   = 2;

static const uint32_t BAUD              = 500000UL;
static const uint16_t SAMPLE_HZ         = 200;         // ~100x oversampled for a few-Hz gait
static const uint32_t PERIOD_US         = 1000000UL / SAMPLE_HZ;
static const uint16_t MARK_DEBOUNCE_MS  = 40;

// ---------------------------------------------------------------- run state

static bool     streaming   = false;
static uint32_t nextDueUs   = 0;
static uint32_t sampleCount = 0;
static uint32_t overruns    = 0;       // deadlines missed — non-zero means gaps in the data
static uint16_t markCount   = 0;
static uint32_t lastMarkMs  = 0;
static uint8_t  lastMarkPin = HIGH;

// ---------------------------------------------------------------- sampling

/*
 * One conversion is discarded after each mux change. The ADC's sample-and-hold has to
 * charge through the source impedance when the input switches, and the first result
 * carries residue from the previous channel. Discarding it costs 104 us and removes a
 * cross-channel error that would otherwise look like real motion.
 */
static uint16_t readSettled(uint8_t pin)
{
  (void)analogRead(pin);
  return (uint16_t)analogRead(pin);
}

static void emitSample(uint32_t tUs)
{
  uint16_t v[5];
  for (uint8_t i = 0; i < 4; i++) {
    v[i] = readSettled(PIN_POT[i]);
  }
  v[4] = readSettled(PIN_RAIL);

  /*
   * The timestamp is taken before this print, not after. Serial blocks when the TX
   * buffer fills, so a timestamp recorded at write time would encode transmission
   * jitter as if it were sample timing.
   */
  Serial.print(tUs);
  for (uint8_t i = 0; i < 5; i++) {
    Serial.print(',');
    Serial.print(v[i]);
  }
  Serial.print(',');
  Serial.println(markCount);
}

// ---------------------------------------------------------------- output framing

static void printHeader(void)
{
  Serial.println(F("# quaddle-robot pot_logger_uno v1"));
  Serial.print(F("# sample_hz="));   Serial.print(SAMPLE_HZ);
  Serial.print(F(" adc_bits=10 aref=DEFAULT_5V pot=B103_10k_330deg rail_nominal=3.3V"));
  Serial.println();
  Serial.println(F("# ch: fl=A0(ZQDWQ) fr=A1(YQDWQ) rl=A2(ZHDWQ) rr=A3(YHDWQ) rail=A4"));
  Serial.println(F("# mark increments on each button press / 'm' command"));
  Serial.println(F("t_us,fl,fr,rl,rr,rail,mark"));
}

static void printTrailer(void)
{
  Serial.print(F("# stopped samples="));  Serial.print(sampleCount);
  Serial.print(F(" overruns="));          Serial.print(overruns);
  Serial.print(F(" marks="));             Serial.println(markCount);
  if (overruns > 0) {
    Serial.println(F("# WARNING overruns non-zero: samples were dropped, timing has gaps"));
  }
}

// ---------------------------------------------------------------- input handling

static void serviceMarkButton(void)
{
  const uint8_t now = digitalRead(PIN_MARK);
  const uint32_t nowMs = millis();

  if (lastMarkPin == HIGH && now == LOW && (nowMs - lastMarkMs) > MARK_DEBOUNCE_MS) {
    markCount++;
    lastMarkMs = nowMs;
  }
  lastMarkPin = now;
}

static void serviceCommands(void)
{
  if (!Serial.available()) {
    return;
  }

  switch (Serial.read()) {
    case 's':
      sampleCount = 0;
      overruns    = 0;
      markCount   = 0;
      printHeader();
      nextDueUs = micros();
      streaming = true;
      break;

    case 'x':
      streaming = false;
      printTrailer();
      break;

    case 'r':
      emitSample(micros());
      break;

    case 'm':
      markCount++;
      break;

    case 'h':
      printHeader();
      break;

    default:
      break;   // ignore stray newlines and unknown characters
  }
}

// ---------------------------------------------------------------- entry points

void setup(void)
{
  Serial.begin(BAUD);
  pinMode(PIN_MARK, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  // Settle the pull-up and take an initial reading of the button so the first
  // loop() pass cannot register a phantom press.
  delay(10);
  lastMarkPin = digitalRead(PIN_MARK);

  Serial.println(F("# ready — 's' start, 'x' stop, 'r' single sample, 'm' mark, 'h' header"));
}

void loop(void)
{
  serviceCommands();
  serviceMarkButton();

  if (!streaming) {
    return;
  }

  // Signed comparison so the micros() wrap at ~71.6 minutes is handled correctly.
  if ((int32_t)(micros() - nextDueUs) < 0) {
    return;
  }

  const uint32_t tUs = micros();
  emitSample(tUs);
  sampleCount++;
  digitalWrite(LED_BUILTIN, (sampleCount & 0x20) ? HIGH : LOW);   // ~3 Hz activity blink

  nextDueUs += PERIOD_US;

  /*
   * If we are already past the next deadline the loop could not keep up. Count it and
   * resynchronise rather than trying to catch up, which would emit a burst of samples
   * with meaningless spacing.
   */
  if ((int32_t)(micros() - nextDueUs) >= 0) {
    overruns++;
    nextDueUs = micros() + PERIOD_US;
  }
}
