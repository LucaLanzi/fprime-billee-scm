# billeeScm::PCA9685Manager

Drives up to 12 hobby servos through a PCA9685 16-channel I²C PWM board. Each channel is either a limited-range **positional** servo (`setServoAngle`) or a **continuous-rotation** servo (`setServoSpeed`), and every channel has calibrated min/mid/max pulse limits that no command can exceed. One instance per PCA9685 board, bound to an I²C bus and a board address via `configure()`.

## Usage Examples

### Diagrams
See "Class Diagram" and "Command Data Flow" below.

### Typical Usage
1. In the topology, instantiate `pca9685Manager` (see `instances.fpp`) and connect its `run` port to a rate group.
2. At boot, before the rate group starts ticking it, call `configure(TwoWire* wire, U8 a5a0, uint32_t i2cSpeed = 100000)` once. The deployment calls `pca9685Manager.configure(&Wire, 0, 100000)`. `configure()` calls `wire->begin()` itself, initialises the PCA9685, sets the 50 Hz servo frame and releases every channel.
3. Send `setServoAngle` (positional servos) or `setServoSpeed` (continuous servos). Use `releaseChannel` / `releaseAll` to stop pulsing.
4. If the board was powered on after the Teensy, `configure()`'s init failed; send `reinit` once the board is powered.
5. Watch `servoAngles`, `i2cErrorCount` and `lastI2cError` telemetry, and the `I2cFault` event.

## Class Diagram
`PCA9685Manager` owns:
- `m_pca: PCA9685*` — the vendored NachtRaveVL driver (`lib/vendor-lib/PCA9685/`), constructed in `configure()` with **placement new into the static buffer `m_pcaStorage`, not on the heap**. This deployment never registers a memory allocator with `Os::Baremetal::OverrideNewDelete`, so any plain `new` trips `FW_ASSERT(pAllocator != nullptr)` and halts the board.
- **`PCA9685_ServoEval` is deliberately not used.** Every one of its constructors does `new float[]`, which would hit that assert at boot. The `PCA9685` class itself never allocates. Angle-to-pulse conversion is done by `pwmForPosition()` instead.
- `kServoCal[12]` (in the `.cpp`) — per-channel `ServoType` (`POSITIONAL`/`CONTINUOUS`) and `minPwm/midPwm/maxPwm` pulse limits. All channels default to `POSITIONAL, 102, 307, 512`.
- `m_values[12]` — the last commanded value per channel (degrees, speed, or 0 when released), republished as `servoAngles` every `run` tick.
- `m_initOk` — true once `initDevice()` fully succeeded; gates every motion command.
- `m_i2cErrorCount`, `m_lastI2cError` — failed-transaction count and the Wire code of the latest transaction.

## Port Descriptions
| Name | Description |
|---|---|
| `run` | `Svc.Sched` input (async). Once per rate-group cycle it publishes the three telemetry channels. It does no I²C. |

## Component States
No state machine. Each command is validated, performs its I²C transaction(s) inline in the handler, and is answered exactly once with the real result. The only state is `m_initOk`.

## Command Data Flow
1. **Validate.** Channel out of range, wrong servo type for the command, out-of-range or NaN angle/speed, or a raw PWM outside the channel's `[min,max]` (other than 0) → `VALIDATION_ERROR`. **Nothing is sent on the bus.**
2. **Gate.** Motion commands (`setServoAngle`, `setServoSpeed`, `setChannelPWM`) are answered `EXECUTION_ERROR` if `m_initOk` is false. `releaseChannel` and `releaseAll` are **not** gated: a stop must always try the bus (they are refused only if `configure()` never ran).
3. **Convert.** `pwmForPosition(channel, position)` maps position ∈ [-1, +1] to a pulse, piecewise linear through (-1, min), (0, mid), (+1, max), then clamps to `[min,max]`. Angle uses `angle/90`; speed is used directly. Because each half has its own slope, a servo whose centre is not halfway between its end points is still centred at 0.
4. **Write.** `setChannelPWM(ch, pwm)` (or `setChannelOff(ch)` for 0, `setAllChannelsPWM(0)` for release-all). The library writes 4 bytes to that channel's LED registers.
5. **Check.** `getLastI2CError()` is read after every library call. Non-zero → counters/telemetry updated, `I2cFault` logged (once per new fault, see "Events"), response `EXECUTION_ERROR`. Zero → `m_values` updated, response `OK`.

`pwmForSpeed()` in the library is literally `pwmForAngle(speed*90)`, i.e. the same pulse mapping under a different name: a continuous-rotation servo is just a servo whose pulse means speed instead of angle. What can damage a *limited-range* servo is a pulse outside its mechanical range, which is why the per-channel type and pulse limits exist.

## Parameters
None. `loadParameters()` is disabled on Arduino deployments (no file system), so a parameter could only ever hold its default. Per-channel calibration is a compile-time table in `PCA9685Manager.cpp`; bench-tune with `setChannelPWM`, then copy the values into the table.

## Commands
| Name | Description |
|---|---|
| `setServoAngle(channel, angle)` | Move a POSITIONAL servo. `channel` 0-11, `angle` -90.0..+90.0 (0 = calibrated centre). `VALIDATION_ERROR` on a bad channel, a CONTINUOUS channel, or an out-of-range/NaN angle. |
| `setServoSpeed(channel, speed)` | Drive a CONTINUOUS servo. `speed` -1.0..+1.0 (0 = the calibrated stop pulse). `VALIDATION_ERROR` on a POSITIONAL channel (none are CONTINUOUS by default). |
| `setChannelPWM(channel, pwm)` | Raw pulse for bring-up. `pwm` 0 releases the channel; any other value must lie inside that channel's `[minPwm,maxPwm]`, otherwise `VALIDATION_ERROR` (4096 "full on" is never allowed). |
| `releaseChannel(channel)` | Stop pulsing one channel (the servo goes limp). |
| `releaseAll()` | Stop pulsing every channel. This is the software stop: the PCA9685's OE pin is not wired. |
| `reinit()` | Re-run init, 50 Hz frame and release-all. Use after powering the board on after the Teensy. Logs `I2cFault` on failure. |

## Events
| Name | Description |
|---|---|
| `I2cFault(channel, code)` | Warning (high). An I²C transaction failed. `channel` is the channel being written, or 255 for init / release-all. Logged **once per new fault** (first failure or a changed code), not once per repeated failure: a dead bus would otherwise emit one event per command and overflow `eventLogger`'s depth-3 queue, which is an `FW_ASSERT`. Not logged from `configure()` (the logger has not drained yet); the failure is visible in telemetry. There is deliberately no per-move event, for the same reason. |

## Telemetry
| Name | Description |
|---|---|
| `servoAngles` | `billeeScm.ServoAngles` ([12] F32): last commanded value per channel — degrees for POSITIONAL, speed for CONTINUOUS, 0 if released, never commanded, or set by a raw `setChannelPWM`. |
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
| PCA9685-001 | Shall move a POSITIONAL servo to the commanded angle (-90..+90, 0 = calibrated centre). | Bench: checklist #4 |
| PCA9685-002 | Shall never send a pulse outside a channel's `[minPwm,maxPwm]` (except 0 = released), for any command. | GDS: checklist #2, #3 |
| PCA9685-003 | Shall reject a command for the wrong servo type, a bad channel, or an out-of-range/NaN value with `VALIDATION_ERROR` and send nothing on the bus. | GDS: checklist #3, #5 |
| PCA9685-004 | Shall answer every command exactly once, with a response reflecting that command's real I²C result. | GDS: checklist #7 |
| PCA9685-005 | `releaseAll` / `releaseChannel` shall always attempt the bus, even if init failed. | Bench: checklist #8 |
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
- **No hardware kill.** The PCA9685's OE pin is not wired (left at the board default). The only stops are `releaseAll`, power-cycling the board, or removing V+.
- **Reset behaviour.** The PCA9685 keeps running when the Teensy resets (there is no reset line, and the assert hook reboots to HalfKay), so it would keep driving the last pulses. `initDevice()` therefore releases every channel, so servos go limp on every Teensy boot and `servoAngles` (all 0) matches the hardware.
- **Blocking.** `Wire` is blocking and the TaskRunner is cooperative. A command's transaction is short (~0.5 ms at 100 kHz), but with the board absent each failed transaction waits for the bus NACK/timeout.
- **License.** The vendored library is GPL v3.

## GDS Bench Checklist
**Not yet run on hardware.** Send from GDS (names prefixed `billee_deployment.pca9685Manager.`). Start with one servo on channel 0, no mechanical load.
| # | Action | Expected |
|---|---|---|
| 0 | Before power-up: meter the breakout's logic VCC = 3.3 V, SDA/SCL idle high at 3.3 V; scope SDA/SCL rise time at 100 kHz | 3.3 V; rise time well inside 1 µs |
| 1 | Boot with the board wired; watch telemetry | `i2cErrorCount` 0, `lastI2cError` 0, `servoAngles` all 0, servo limp |
| 2 | `setChannelPWM` ch 0, pwm 4096, then pwm 50 | `VALIDATION_ERROR` both times, no motion (outside 102-512) |
| 3 | `setServoAngle` ch 0, angle 91, then NaN; `setServoSpeed` ch 0, speed 0.5; `setServoAngle` ch 12 | `VALIDATION_ERROR` each (out of range, positional channel, bad channel), no motion |
| 4 | `setServoAngle` ch 0 at -90, 0, +90 | servo reaches each end and the centre; `servoAngles[0]` follows; recalibrate `kServoCal` if the ends bind or the centre is off |
| 5 | `setChannelPWM` ch 0 at 102, 307, 512 | same three positions; `servoAngles[0]` reads 0 (raw pulse) |
| 6 | `releaseChannel` ch 0 | servo goes limp; `servoAngles[0]` 0 |
| 7 | Unplug the board's SDA or power, send `setServoAngle` | `EXECUTION_ERROR`; `i2cErrorCount` rises; one `I2cFault` (not one per repeat); `lastI2cError` non-zero |
| 8 | With the board still unplugged, send `releaseAll` | `EXECUTION_ERROR` (it did try the bus); no assert, board keeps running |
| 9 | Re-plug, `reinit`, `setServoAngle` ch 0 | `reinit` `OK`; motion works again; `lastI2cError` 0 |
| 10 | Hold a servo at +45, reset the Teensy | servo goes limp on boot; `servoAngles` all 0 |
| 11 | Leave running >100 s with GDS attached | steady heartbeats, no assert/reboot (no priority or queue problem) |

## Known Limitations
- **`servoAngles` is the commanded value, not a read-back.** The PCA9685 is never read; a servo can be blocked or stalled and telemetry will still show the commanded angle. A raw `setChannelPWM` reports 0 (unknown).
- **Init failure is only visible after the fact.** `configure()` does not log an event (the logger has not drained). If the board was absent at boot, `i2cErrorCount` / `lastI2cError` show it and motion commands return `EXECUTION_ERROR` until `reinit`.
- **`initDevice()` checks each library call separately**, but a library call made of several bus transactions reports only the last one's result, so a transient failure early in a multi-transaction call could be masked.
- **No hardware kill and no read-back of OE.** See "Deployment Notes".
- **Calibration is compile-time.** Parameters are not loadable on this platform, so changing a channel's limits or type means editing `kServoCal` and reflashing.
- **Piecewise-linear, not a cubic spline.** `PCA9685_ServoEval`'s cubic-spline option is not available (it heap-allocates), so a servo with an off-centre midpoint gets two linear segments with a slope change at 0 rather than a smooth curve. For a hobby servo this is not physically noticeable.
- **Only 12 of the chip's 16 channels are exposed** (`NUM_SERVO_CHANNELS`, also the size of `billeeScm::ServoAngles`).

## Change Log
| Date | Description |
|---|---|
| 2026-09-25 | Initial implementation: positional/continuous servo commands with per-channel pulse limits, raw PWM within limits, release/release-all, reinit, I²C fault counting and rate-limited `I2cFault` event, telemetry. Builds on the vendored PCA9685 library (`lib/vendor-lib/PCA9685`). Not yet run on hardware. |
