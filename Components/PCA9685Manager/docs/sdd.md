# billeeScm::PCA9685Manager

Drives up to 12 hobby servos through a PCA9685 16-channel I²C PWM board. Every servo, positional or continuous-rotation, is driven with one command that selects the servo and gives it a **PWM magnitude and a direction** around its calibrated neutral pulse (`setServo`). Every channel has calibrated min/mid/max pulse limits that no command can exceed. One instance per PCA9685 board, bound to an I²C bus and a board address via `configure()`.

## Usage Examples

### Diagrams
See "Class Diagram" and "Command Data Flow" below.

### Typical Usage
1. In the topology, instantiate `pca9685Manager` (see `instances.fpp`) and connect its `run` port to a rate group.
2. At boot, before the rate group starts ticking it, call `configure(TwoWire* wire, U8 a5a0, uint32_t i2cSpeed = 100000)` once. The deployment calls `pca9685Manager.configure(&Wire, 0, 100000)`. `configure()` calls `wire->begin()` itself, initialises the PCA9685, sets the 50 Hz servo frame and releases every channel.
3. Send `setServo(servo, pwm, direction)`: `servo` is `SERVO0`-`SERVO11`, `pwm` is 0-1000 (0.0-100.0% of the servo's calibrated span in that direction), `direction` is `FORWARD`/`REVERSE`/`STOPPED`. Use `releaseChannel` to make one servo limp and `hardStop` to release every channel at once.
4. If the board was powered on after the Teensy, `configure()`'s init failed; send `reinit` once the board is powered.
5. Watch `servoStates`, `i2cErrorCount` and `lastI2cError` telemetry, and the `I2cFault` event.

## Class Diagram
`PCA9685Manager` owns:
- `m_pca: PCA9685*` — the vendored NachtRaveVL driver (`lib/vendor-lib/PCA9685/`), constructed in `configure()` with **placement new into the static buffer `m_pcaStorage`, not on the heap**. This deployment never registers a memory allocator with `Os::Baremetal::OverrideNewDelete`, so any plain `new` trips `FW_ASSERT(pAllocator != nullptr)` and halts the board.
- **`PCA9685_ServoEval` is deliberately not used.** Every one of its constructors does `new float[]`, which would hit that assert at boot. The `PCA9685` class itself never allocates. PWM-to-pulse conversion is done by `pulseFor()` instead.
- `kServoCal[12]` (in the `.cpp`) — per-channel `minPwm/midPwm/maxPwm` pulse limits and a `reversed` flag. All channels default to `102, 307, 512, false`.
- `m_states: ServoStates` — the last commanded `{direction, pwm, pulse}` per channel, republished as `servoStates` every `run` tick.
- `m_initOk` — true once `initDevice()` fully succeeded; gates every motion command.
- `m_i2cErrorCount`, `m_lastI2cError` — failed-transaction count and the Wire code of the latest transaction.

## Port Descriptions
| Name | Description |
|---|---|
| `run` | `Svc.Sched` input (async). Once per rate-group cycle it publishes the three telemetry channels. It does no I²C. |

## Component States
No state machine. Each command is validated, performs its I²C transaction(s) inline in the handler, and is answered exactly once with the real result. The only state is `m_initOk`.

## Command Data Flow
1. **Validate.** An invalid `servo` or `direction`, `pwm` > 1000 (`setServo`), or a raw PWM outside the channel's `[min,max]` other than 0 (`setChannelPWM`) → `VALIDATION_ERROR`. **Nothing is sent on the bus.**
2. **Gate.** Motion commands (`setServo`, `setChannelPWM`) are answered `EXECUTION_ERROR` if `m_initOk` is false. `releaseChannel` and `hardStop` are **not** gated: a stop must always try the bus (they are refused only if `configure()` never ran).
3. **Convert.** `pulseFor(channel, pwm, direction)` turns the magnitude and direction into a pulse:
   - `STOPPED`, or `pwm` 0 → `mid` (the neutral pulse is held, not released).
   - Otherwise the direction is swapped if the channel is `reversed`, then FORWARD → `mid + round(pwm * (max - mid) / 1000)` and REVERSE → `mid - round(pwm * (mid - min) / 1000)`.
   - The result is clamped to `[min,max]`. The math is integer only.
   Each side of `mid` has its own span, so a servo whose neutral is not halfway between its end points still maps 0 to neutral and 1000 to each end point.
4. **Write.** `setChannelPWM(ch, pulse)` (or `setChannelOff(ch)` for 0, `setAllChannelsPWM(0)` for `hardStop`). The library writes 4 bytes to that channel's LED registers.
5. **Check.** `getLastI2CError()` is read after every library call. Non-zero → counters/telemetry updated, `I2cFault` logged (once per new fault, see "Events"), response `EXECUTION_ERROR`. Zero → `m_states` updated, response `OK`.

### How direction works on a servo
A servo has no direction line: direction is the side of the neutral pulse the command is on. The vendored library's notes (`PCA9685.h`) and `PCA9685_ServoEval` show this: within the 20 ms / 50 Hz frame, 2.5% (102 counts) is one extreme, 7.5% (307) is neutral and 12.5% (512) is the other extreme. `pwmForSpeed(s)` is literally `pwmForAngle(s * 90)`. For a **continuous** servo, neutral is stopped and the distance from neutral is speed. For a **positional** servo, the same distance is position from centre. So one PWM + direction mapping drives both kinds, and the old per-channel `ServoType` and angle/speed commands are gone. What can damage a *limited-range* servo is a pulse outside its mechanical range, which is why the per-channel pulse limits still exist.

### PWM contract
- `pwm` uses the scale shared by every motor command in this project (`RoboclawManager.setMotor` uses the same one): **0-1000 = 0.0-100.0%** of that servo's calibrated span in the commanded direction.
- At the default calibration one side is 205 counts, so the hardware resolution (~0.5% per count) is coarser than the 0.1% command step. The command scale never limits precision.
- 1001-65535 is rejected with `VALIDATION_ERROR`, not clamped.

## Parameters
None. `loadParameters()` is disabled on Arduino deployments (no file system), so a parameter could only ever hold its default. Per-channel calibration is a compile-time table in `PCA9685Manager.cpp`; bench-tune with `setChannelPWM`, then copy the values into the table.

## Commands
| Name | Description |
|---|---|
| `setServo(servo, pwm, direction)` | Drive one servo (`SERVO0`-`SERVO11`). `pwm` 0-1000 is the magnitude (0.0-100.0% of the calibrated span) and `direction` is `FORWARD`/`REVERSE`/`STOPPED`. `STOPPED` or `pwm` 0 holds the neutral pulse. `VALIDATION_ERROR` if `pwm` > 1000 or an enum is invalid. |
| `setChannelPWM(servo, pwm)` | Raw pulse for bring-up and calibration. `pwm` 0 releases the channel; any other value must lie inside that channel's `[minPwm,maxPwm]`, otherwise `VALIDATION_ERROR` (4096 "full on" is never allowed). |
| `releaseChannel(servo)` | Stop pulsing one channel (the servo goes limp). |
| `hardStop()` | Stop pulsing every channel in one transaction (all servos go limp). This is the software stop: the PCA9685's OE pin is not wired. Not gated on init. |
| `reinit()` | Re-run init, 50 Hz frame and release-all. Use after powering the board on after the Teensy. Logs `I2cFault` on failure. |

## Events
| Name | Description |
|---|---|
| `I2cFault(channel, code)` | Warning (high). An I²C transaction failed. `channel` is the channel being written, or 255 for init / `hardStop`. Logged **once per new fault** (first failure or a changed code), not once per repeated failure: a dead bus would otherwise emit one event per command and overflow `eventLogger`'s depth-3 queue, which is an `FW_ASSERT`. Not logged from `configure()` (the logger has not drained yet); the failure is visible in telemetry. There is deliberately no per-move event, for the same reason. |

## Telemetry
| Name | Description |
|---|---|
| `servoStates` | `billeeScm.ServoStates` ([12] `ServoState`, indexed by `ServoId`): last commanded `direction`, `pwm` (0-1000) and `pulse` (counts actually sent, 0 = released) per channel. A raw `setChannelPWM` shows `STOPPED / 0 / <pulse>`. All `STOPPED / 0 / 0` at boot. |
| `i2cErrorCount` | U32, failed I²C transactions since boot. |
| `lastI2cError` | U8, Wire result of the most recent transaction: 0 ok, 1 data too long, 2 address NACK, 3 data NACK, 4 other / short read, 5 timeout. |

All three are written every `run` tick (not only on change), so they have a value from the first tick and are visible to a GDS that connects mid-run.

## Unit Tests
No automated unit tests exist yet — `register_fprime_ut` in `CMakeLists.txt` is present but commented out. Verification is manual bench testing (see "GDS Bench Checklist").
| Name | Description | Output | Coverage |
|---|---|---|---|
| *(none yet)* | | | |

## Requirements
| Name | Description | Validation |
|---|---|---|
| PCA9685-001 | Shall drive the selected servo at `mid ± round(pwm/1000 × span)` on the commanded direction's side (swapped for a `reversed` channel), and hold `mid` for `STOPPED` or `pwm` 0. | Bench: checklist #4 |
| PCA9685-002 | Shall never send a pulse outside a channel's `[minPwm,maxPwm]` (except 0 = released), for any command. | GDS: checklist #2, #3 |
| PCA9685-003 | Shall reject an invalid servo or direction, or `pwm` > 1000, with `VALIDATION_ERROR` and send nothing on the bus. | GDS: checklist #3 |
| PCA9685-004 | Shall answer every command exactly once, with a response reflecting that command's real I²C result. | GDS: checklist #7 |
| PCA9685-005 | `hardStop` / `releaseChannel` shall always attempt the bus, even if init failed. | Bench: checklist #8 |
| PCA9685-006 | Shall start with all channels released, regardless of what the PCA9685 was doing before a Teensy reset. | Bench: checklist #9 |
| PCA9685-007 | Shall report I²C failures via `i2cErrorCount`, `lastI2cError` and `I2cFault` without flooding the event queue. | Bench: checklist #7 |
| PCA9685-008 | Shall not allocate from the heap. | Code inspection ("Class Diagram") |

## Deployment Notes
- **Bus.** `Wire` (SDA pin 18, SCL pin 19). Not `Wire1`: its pins (16/17) are `Serial4`'s, used by `roboclaw2Manager`.
- **Address.** `a5a0` is the value of the board's A5..A0 pins (0-61), **not** the 7-bit address; the library adds the 0x40 base. A default board is `0` (-> 0x40).
- **Speed.** 100 kHz, not the library's 400 kHz default. Effective pull-up on each line is the breakout's 10 kΩ in parallel with the Teensy's internal ~22 kΩ (`WireIMXRT.cpp` enables it) ≈ 6.9 kΩ, which is marginal for 400 kHz rise times over jumper wires and comfortable at 100 kHz. A single-servo update is ~0.5 ms and a servo frame is 20 ms, so 100 kHz costs nothing. Do not add external resistors "to match" the breakout's: pull-ups on one bus are in parallel, so extra resistors only lower the total. Add one only if a scope shows slow edges.
- **Logic voltage.** Power the breakout's logic VCC from the Teensy's **3.3 V**. Teensy 4.1 pins are not 5 V tolerant and the breakout's pull-ups go to its logic VCC. Servo power is the separate V+ rail; tie the grounds together.
- **`BUFFER_LENGTH`.** On Teensy 4.1 `Wire.h` -> `WireIMXRT.h` defines it as 136, so the library batches `(136-1)/4` = 33 channels per transaction (all 16 fit in one). The library's 32-byte fallback and `#warning` never apply. `WIRE_INTERFACES_COUNT` (3) is only used under `PCA9685_ENABLE_DEBUG_OUTPUT`.
- **`Wire` library.** `Wire.h` is a Teensy *library* header (not core). `lib/vendor-lib/PCA9685/CMakeLists.txt` declares it with `target_use_arduino_libraries("Wire")` so the include path and link do not depend on `Arduino/Drv/I2cDriver` happening to declare it.
- **`resetDevices()` is never called.** It is an I²C general-call reset and would reset every device on the bus that honours it.
- **Task priority / queue.** `pca9685Manager` starts between `eventLogger` (98) and `pumpManager` (97) in alphabetical order, so its priority must be 97 or 98; it uses 97. Queue depth is `Default.PCA9685_QUEUE_SIZE` (10) so a burst of servo commands cannot overflow into an `FW_ASSERT`.
- **No hardware kill.** The PCA9685's OE pin is not wired (left at the board default). The only stops are `hardStop`, power-cycling the board, or removing V+.
- **Reset behaviour.** The PCA9685 keeps running when the Teensy resets (there is no reset line, and the assert hook reboots to HalfKay), so it would keep driving the last pulses. `initDevice()` therefore releases every channel, so servos go limp on every Teensy boot and `servoStates` (all `STOPPED / 0 / 0`) matches the hardware.
- **Blocking.** `Wire` is blocking and the TaskRunner is cooperative. A command's transaction is short (~0.5 ms at 100 kHz), but with the board absent each failed transaction waits for the bus NACK/timeout.
- **License.** The vendored library is GPL v3.

## GDS Bench Checklist
**Not yet run on hardware.** Send from GDS (names prefixed `billee_deployment.pca9685Manager.`). Start with one servo on `SERVO0`, no mechanical load. Pulses below are for the default calibration (102/307/512).
| # | Action | Expected |
|---|---|---|
| 0 | Before power-up: meter the breakout's logic VCC = 3.3 V, SDA/SCL idle high at 3.3 V; scope SDA/SCL rise time at 100 kHz | 3.3 V; rise time well inside 1 µs |
| 1 | Boot with the board wired; watch telemetry | `i2cErrorCount` 0, `lastI2cError` 0, `servoStates` all `STOPPED / 0 / 0`, servo limp |
| 2 | `setChannelPWM` SERVO0, pwm 4096, then pwm 50 | `VALIDATION_ERROR` both times, no motion (outside 102-512) |
| 3 | `setServo` SERVO0 1001 FORWARD | `VALIDATION_ERROR`, no motion |
| 4 | `setServo` SERVO0 0 STOPPED; 1000 FORWARD; 1000 REVERSE; 500 FORWARD; 500 REVERSE | `servoStates[0].pulse` 307, 512, 102, 410, 204. A positional servo goes to centre, each end, and halfway; a continuous one stops, then runs full speed each way, then half. Recalibrate `kServoCal` if an end binds or neutral creeps; set `reversed` if FORWARD turns the wrong way |
| 5 | `setChannelPWM` SERVO0 at 102, 307, 512 | same three positions; `servoStates[0]` reads `STOPPED / 0 / <pulse>` |
| 6 | `releaseChannel` SERVO0 | servo goes limp; `servoStates[0]` `STOPPED / 0 / 0` |
| 7 | Unplug the board's SDA or power, send `setServo` | `EXECUTION_ERROR`; `i2cErrorCount` rises; one `I2cFault` (not one per repeat); `lastI2cError` non-zero |
| 8 | With the board still unplugged, send `hardStop` | `EXECUTION_ERROR` (it did try the bus); no assert, board keeps running |
| 9 | Re-plug, `reinit`, `setServo` SERVO0 | `reinit` `OK`; motion works again; `lastI2cError` 0 |
| 10 | Several servos moving, `hardStop` | all go limp at once; `servoStates` all `STOPPED / 0 / 0` |
| 11 | Hold a servo at 500 FORWARD, reset the Teensy | servo goes limp on boot; `servoStates` all `STOPPED / 0 / 0` |
| 12 | Leave running >100 s with GDS attached | steady heartbeats, no assert/reboot (no priority or queue problem) |

## Known Limitations
- **`servoStates` is the commanded value, not a read-back.** The PCA9685 is never read; a servo can be blocked or stalled and telemetry will still show the commanded direction and PWM.
- **PWM is open-loop.** For a positional servo, `pwm` is a fraction of its pulse span, not a calibrated angle; for a continuous servo, speed vs. pulse is the servo's own (often non-linear) curve. Per-motor behaviour comes from the calibration table only.
- **Init failure is only visible after the fact.** `configure()` does not log an event (the logger has not drained). If the board was absent at boot, `i2cErrorCount` / `lastI2cError` show it and motion commands return `EXECUTION_ERROR` until `reinit`.
- **`initDevice()` checks each library call separately**, but a library call made of several bus transactions reports only the last one's result, so a transient failure early in a multi-transaction call could be masked.
- **No hardware kill and no read-back of OE.** See "Deployment Notes".
- **Calibration is compile-time.** Parameters are not loadable on this platform, so changing a channel's limits or `reversed` flag means editing `kServoCal` and reflashing.
- **Piecewise-linear, not a cubic spline.** `PCA9685_ServoEval`'s cubic-spline option is not available (it heap-allocates), so a servo with an off-centre midpoint gets two linear segments with a slope change at neutral rather than a smooth curve. For a hobby servo this is not physically noticeable.
- **Only 12 of the chip's 16 channels are exposed** (`NUM_SERVO_CHANNELS`, also the size of `billeeScm::ServoStates` and the number of `billeeScm::ServoId` entries).

## Change Log
| Date | Description |
|---|---|
| 2026-09-25 | Initial implementation: positional/continuous servo commands with per-channel pulse limits, raw PWM within limits, release/release-all, reinit, I²C fault counting and rate-limited `I2cFault` event, telemetry. Builds on the vendored PCA9685 library (`lib/vendor-lib/PCA9685`). Not yet run on hardware. |
| 2026-10-29 | PWM + direction refactor: angle/speed scheme removed (`setServoAngle`, `setServoSpeed`, per-channel `ServoType`). One command `setServo(servo: ServoId, pwm: 0-1000, direction: motorDir)` drives every servo around its neutral pulse; `STOPPED`/0 holds neutral. `reversed` flag added to `kServoCal`. Channels selected by the `ServoId` enum. `releaseAll` renamed `hardStop`. Telemetry `servoAngles` ([12] F32) replaced by `servoStates` ([12] `{direction, pwm, pulse}`). Opcodes renumbered. |
