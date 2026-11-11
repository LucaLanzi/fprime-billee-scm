# billeeScm::StepperManager

Drives the Sample Cache carousel: a NEMA 17 with an integrated driver on optocoupled inputs.

| Driver input | Teensy pin | Meaning when on |
|---|---|---|
| PU- | T_GPIO_2 | Step pulse |
| DR- | T_GPIO_3 | Direction (on = REVERSE unless `invertDirection`) |
| MF- | T_GPIO_4 | Motor free (de-energized) |
| XCOM | +5 V | Common anode |

T_GPIO_5 is spare.

## Pin drive

The pins are open-drain (`OUTPUT_OPENDRAIN`): LOW sinks current through the input's optocoupler (input on), HIGH leaves
the pin high-impedance (input off). Driving HIGH to 3.3 V instead would leave 1.7 V across an input whose XCOM is at 5 V,
which can keep the optocoupler partly on. If the driver still sees phantom pulses, add a level shifter.

## Motion

Step pulses come from an `IntervalTimer` interrupt running at twice the step rate (pulse on, pulse off). Each move follows
a trapezoid: it starts at 200 steps/s (or the cruise rate if lower), accelerates at the configured rate (4000 steps/s^2 in
the topology) to the cruise rate, and decelerates to land exactly on the target. Measured on the Teensy: 3200 steps at
1600 steps/s take 2.31 s, as computed. DR- is set 20 us before the first pulse.

## Commands

| Command | Answered | Notes |
|---|---|---|
| `MOVE_STEPS dir steps speedHz` | on arrival (OK) | 1-1000000 steps, 1-5000 steps/s. |
| `MOVE_TO position speedHz` | on arrival (OK; at once if already there) | Position in steps from `ZERO`. |
| `STOP` | at once | Stops without ramping; the move in progress answers `EXECUTION_ERROR`. |
| `HOLD ON\|OFF` | at once | ON energizes (holds position, the default from boot); OFF frees the motor. Refused while moving. |
| `ZERO` | at once | Current position becomes 0. Refused while moving. |

A new move supersedes the one in progress (`EXECUTION_ERROR`). There is no home switch: positions are relative to the
last `ZERO`, and steps lost by the driver are not detected. Commands take steps; add a steps-per-revolution constant
once the microstepping and gearing are fixed.

## Telemetry and events

- `carousel`: moving, holding, position, target, current speed.
- `MoveStarted`, `MoveDone`, `MoveStopped`.
