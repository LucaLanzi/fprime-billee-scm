# billeeScm::RoboclawManager

Drives up to two motors on a single BasicMicro Roboclaw packet-serial motor controller with one PWM + direction command (`setMotor`), a `hardStop` that stops both motors at once, per-motor optional limit-switch safety stop and latched communication-fault handling. Designed to be instantiated once per physical Roboclaw board — multiple independent boards are supported by running multiple instances, each bound to its own serial port and device address via `configure()`.

## Usage Examples

### Diagrams
See "Class Diagram" and "Sequence Diagrams" below.

### Typical Usage
1. In the topology, instantiate one `RoboclawManager` per physical Roboclaw board (see `instances.fpp`).
2. At boot, before the rate group starts ticking the instance, call `configure(HardwareSerial* serial, U8 address, uint32_t baud = 38400)` once to bind it to that board's serial line and packet-serial address.
3. Send `setMotor(motor, pwm, direction)` to drive one motor: `motor` is `MOTOR1`/`MOTOR2`, `pwm` is 0-1000 (0.0-100.0% duty), `direction` is `FORWARD`/`REVERSE`/`STOPPED`. Send `hardStop` to stop both motors immediately.
4. The component reports the actual resulting motor state via the `motor1`/`motor2` telemetry channels and the `motorEvent` log after every command and every limit-switch-triggered stop.
5. If the Roboclaw board stops acknowledging commands (wiring/power fault), the instance latches into an error state and rejects further `setMotor`s with `EXECUTION_ERROR` until `clearError` is sent. `hardStop` is still accepted while latched.

## Class Diagram
`RoboclawManager` owns:
- `m_roboclaw: RoboClaw*` — the vendored BasicMicro packet-serial driver (`lib/vendor-lib/RoboClaw/`), bound to this instance's `HardwareSerial`. It is constructed in `configure()` with **placement new into the static buffer `m_roboclawStorage`, not on the heap**: this deployment never registers a memory allocator with `Os::Baremetal::OverrideNewDelete`, so a plain `new` would trip `FW_ASSERT(pAllocator != nullptr)`.
- `m_address: U8` — this instance's Roboclaw device address, set in `configure()`.
- `motorControlSM: MotorControlStateMachine` — an internal F´ state machine instance (see "Component States") that sequences command dispatch, limit-switch gating, and fault latching. State-machine signals are **queued on the component's own message queue** and dispatched later, they are not run inline.
- `m_motor1SwitchTripped` / `m_motor2SwitchTripped: bool` — per-motor limit-switch state, refreshed once per `run` tick.
- `m_motor1State` / `m_motor2State: yellowJacket` — the last state reported for each motor (initially `STOPPED / 0 / OFF`), republished as telemetry every `run` tick. Updated by `reportMotorState`.
- `m_cmdPending` / `m_pendingOpCode` / `m_pendingCmdSeq` — the single `setMotor` currently in flight. Because signals are queued, `setMotor_cmdHandler` cannot know the result of the Roboclaw exchange when it returns; it records the opcode/sequence and the state-machine *action* answers via `completePendingCmd()` once the real result is known.
- `m_cancelledCmds: U8` — commands cancelled by `hardStop` whose `cmdRecv` signal is still queued. Each consumes one count in the state machine (`dropCmd`, or `rejectCmd` in `checkErr`) instead of running. It is a counter because a second `hardStop` can cancel a newer command before the first cancelled signal is dispatched; signals are FIFO, so cancelled ones are always the oldest.
- `reportMotorState(const yellowJacket&, bool logEvent = true)` — private helper, the single place that writes `motor1`/`motor2` telemetry and logs `motorEvent`; called from each of the three actions with the motor's *actual* resulting state, not the raw command, and from `hardStop` with `logEvent = false`.
- `sendDuty(motorId, I16 duty)` / `toDuty(pwm)` — the only Roboclaw motion calls: `DutyM1`/`DutyM2` with a signed duty, and the 0-1000 -> 0-32767 mapping.
- `completePendingCmd(response)` — sends the command response for the in-flight command (no-op if none). Called by `motorFwd`/`motorRev`/`motorStop`/`rejectCmd` and `hardStop`.

## Port Descriptions
| Name | Description |
|---|---|
| `run` | `Svc.Sched` input; once per rate-group cycle (~100 ms) it refreshes cached limit-switch state, publishes the four telemetry channels (see "Telemetry"), and drives the state machine's `tick` signal. |
| `limitSwGet` | A 2-element array of `Drv.GpioRead` output ports (`[2] Drv.GpioRead`); index 0 reads Motor1's limit switch, index 1 reads Motor2's. Only invoked for a motor whose `hasLimitSwitch` param is `true`. |
| `setMotor` (command) | Commands one motor (`MOTOR1`/`MOTOR2`) to a direction (`FORWARD`/`REVERSE`/`STOPPED`) at a PWM magnitude (0-1000 = 0.0-100.0% duty, see "PWM contract"). |
| `clearError` (command) | Clears a latched communication-fault (`checkErr`) state. |
| `hardStop` (command) | Stops both motors in one packet and cancels any `setMotor` in flight. Accepted in every state. |
| `motor1` / `motor2` (telemetry) | The actual resulting state of each motor after the most recent command or limit-switch stop. |
| `motorEvent` (event) | Logged every time a motor's actual state changes (command-driven or limit-switch-triggered). |
| `hardStopEvent` (event) | Logged once per `hardStop`, with whether the Roboclaw acknowledged it. |

## Component States
| Name | Description |
|---|---|
| `init` | Initial state; transitions to `doWait` on the first `tick`. A command that arrives before the first tick is accepted (`cmdRecv` goes straight to command dispatch) so it is never dropped. |
| `doWait` | Idle, waiting for a `setMotor`. Self-loops on `tick`. |
| `doCmd` | Dispatches a received command: drops it if `hardStop` cancelled it (`isCancelled` -> `dropCmd` -> `doWait`), then checks the target motor's limit switch (immediate stop + return to `doWait` if tripped and enabled), otherwise runs the matching `motorFwd`/`motorRev`/`motorStop` action and awaits `success`/`fail`. |
| `checkErr` | Latched communication-fault state, entered when a Roboclaw serial exchange fails. `setMotor_cmdHandler` rejects new commands immediately while latched; a command that was already accepted and reaches this state (queued behind the `fail` signal) is answered `EXECUTION_ERROR` by the `rejectCmd` action (or, if `hardStop` already answered it, just consumes the cancellation). Stays latched until `clearError` sends `errClr`, returning to `doWait`. |

Every accepted `setMotor` is answered exactly once: by the one action it reaches, or by `hardStop` if it was cancelled first (its signal then reaches `dropCmd`/`rejectCmd`, which send nothing). `hardStop` itself does not go through the state machine.

## Sequence Diagrams

**Normal command:**
`setMotor` received → `cmdRecv` signal → cancel check (not cancelled) → limit-switch check (not tripped) → matching action runs the Roboclaw serial call → `success`/`fail` signal → telemetry/event updated with actual result → command response (`OK`/`EXECUTION_ERROR`) → back to `doWait`.

**Limit-switch-triggered stop:**
`setMotor(FORWARD/REVERSE)` received while that motor's enabled limit switch is tripped → `motorStop` action runs directly (bypassing the commanded direction) → telemetry/event report `STOPPED` → state machine returns straight to `doWait` (not through `doCmd`'s normal `success` path).

**Hard stop:**
`hardStop` received → `DutyM1M2(address, 0, 0)` sent directly from the handler (any state, including `checkErr`) → if a `setMotor` is in flight it is answered `EXECUTION_ERROR` and `m_cancelledCmds` is incremented → on acknowledge, `motor1`/`motor2` set to `STOPPED / 0 / OFF` (no `motorEvent`) → one `hardStopEvent(ok)` → response `OK`/`EXECUTION_ERROR`. The cancelled command's `cmdRecv`, still queued, later reaches `isCancelled` → `dropCmd` and never moves the motor. A failed hard stop does not latch `checkErr`.

**Communication fault and recovery:**
Roboclaw serial exchange fails inside a `motorFwd`/`motorRev`/`motorStop`-from-`STOPPED` action (the limit-switch stop path is the exception, see "Known Limitations") → `fail` signal → `checkErr` (latched) → subsequent `setMotor`s rejected immediately with `EXECUTION_ERROR` (state checked before forwarding to the state machine) → operator sends `clearError` → `errClr` signal → back to `doWait` → normal commands work again.

## Command-to-Roboclaw Data Flow
How a `setMotor` becomes bytes on the serial line:

1. `setMotor_cmdHandler` validates the arguments (`pwm > 1000` or an invalid enum -> `VALIDATION_ERROR`, nothing sent), then checks for a latched fault / a command already in flight, records the opcode and sequence, and queues `cmdRecv(yellowJacket{motor, direction, pwm, OFF})`.
2. The state machine's `CHOOSE_CMD` choice drops a command `hardStop` cancelled. `CHOOSE_CMD_LIMIT` then runs `motorStop` if the target motor's enabled limit switch is tripped and the command is not `STOPPED`. Otherwise `CHOOSE_CMD_FWD` / `_REV` / `_STOP` select `motorFwd` / `motorRev` / `motorStop` from `motorDir`.
3. The action calls `sendDuty()`, which uses the vendored `RoboClaw` driver (`lib/vendor-lib/RoboClaw/`). The motor is selected with `motorNum == MOTOR1 ? M1 : M2`:

| Commanded | Action | Library call | Opcode byte sent (M1 / M2) |
|---|---|---|---|
| `FORWARD` | `motorFwd` | `DutyM1/M2(address, +toDuty(pwm))` | 32 / 33 |
| `REVERSE` | `motorRev` | `DutyM1/M2(address, -toDuty(pwm))` | 32 / 33 |
| `STOPPED`, or limit-switch override | `motorStop` | `DutyM1/M2(address, 0)` | 32 / 33 |
| `hardStop` (handler, both motors) | — | `DutyM1M2(address, 0, 0)` | 34 |

4. `RoboClaw::write_n` sends `[address, opcode, duty high, duty low, CRC16 high, CRC16 low]` (`DutyM1M2`: both duties) and waits for a `0xFF` acknowledge. The duty is a signed 16-bit value passed as its two's-complement word. The read timeout is 10 ms (`RoboClaw(serial, 10000)`, in µs) and `MAXRETRY` is 2, so a call makes up to 3 attempts. Returning `false` makes the action send `fail` (-> `checkErr`, except on the limit-switch path, see "Known Limitations").

### PWM contract
- `pwm` is a magnitude on the scale shared by every motor command in this project (`PCA9685Manager.setServo` uses the same one): **0-1000 = 0.0-100.0% duty**, in 0.1% steps. `direction` gives the sign.
- `toDuty(pwm) = round(pwm * 32767 / 1000)`, so 1000 is the Roboclaw's full signed duty (±32767) and 500 is ±16384. This replaces the old 7-bit compatibility commands (0-127), which had about 8x coarser steps.
- 1001-65535 is **rejected with `VALIDATION_ERROR`; it is not scaled or clamped**. The check applies to every direction including `STOPPED`, for which `pwm` is otherwise ignored (duty 0 is sent).
- The duty commands apply the new duty immediately (no acceleration ramp).

## Parameters
| Name | Description |
|---|---|
| `motor1HasLimitSwitch` | `bool`, default `true`. Whether Motor1 has a limit switch wired; if `false`, its `limitSwGet[0]` port is never invoked and its switch is ignored. |
| `motor2HasLimitSwitch` | `bool`, default `true`. Same, for Motor2 / `limitSwGet[1]`. |

## Commands
| Name | Description |
|---|---|
| `setMotor(motor, pwm, direction)` | Drive one motor (`MOTOR1`/`MOTOR2`) in the given direction (`FORWARD`/`REVERSE`/`STOPPED`) at `pwm` 0-1000 (0.0-100.0% duty). Answered `VALIDATION_ERROR` if `pwm` > 1000 (nothing is sent; also applies to `STOPPED`), `EXECUTION_ERROR` if the instance is latched in `checkErr`, another `setMotor` is still in flight, or a `hardStop` cancelled it; otherwise the response reflects the real result of the Roboclaw exchange (`OK` / `EXECUTION_ERROR`). |
| `clearError` | Clears a latched `checkErr` communication-fault state. Safe to send at any time — a no-op if not currently latched. |
| `hardStop` | Sets both motors to duty 0 in one packet (`DutyM1M2`), directly from the handler, in every state including `checkErr` and with a command in flight. Cancels an in-flight `setMotor` (answered `EXECUTION_ERROR`, never executed). `OK` if the Roboclaw acknowledged, else `EXECUTION_ERROR` (does not latch `checkErr`). |

## Events
| Name | Description |
|---|---|
| `motorEvent` | Warning (low), logged with the motor's actual resulting state (`yellowJacket`) every time a command runs or a limit switch triggers a stop. |
| `hardStopEvent(ok)` | Warning (high), logged once per `hardStop` (instead of a `motorEvent` per motor, to keep `eventLogger`'s shallow queue safe when a sequence hard-stops both Roboclaws). `ok` = the Roboclaw acknowledged. |

## Telemetry
| Name | Description |
|---|---|
| `motor1` | Actual current state (`yellowJacket`: motor number, direction, `pwm` 0-1000, on/off) of Motor1, updated after every command/stop affecting it. |
| `motor2` | Same, for Motor2. |
| `motor1LimitSwitch` | `bool`, `true` = Motor1's limit switch is tripped. `false` if the switch is not wired (`motor1HasLimitSwitch` = `false`) or the GPIO read failed. |
| `motor2LimitSwitch` | Same, for Motor2. |

**Always available.** All four channels are written on every `run` tick (~100 ms), not only on change. They therefore have a value from the first tick after boot, without any command, and a GDS that connects mid-run sees them within one cycle (`Svc.TlmChan` re-sends a channel only when it is written). A command's result is also written immediately, by `reportMotorState`, so it does not wait for the next tick.

**Initial value.** Until the first command, `motor1`/`motor2` report `STOPPED / pwm 0 / OFF`. This is an **assumption** (a Roboclaw powers up idle), not a read-back: nothing is sent to the Roboclaw at boot, because an exchange with no controller attached would block for its serial timeout and could latch `checkErr` before anyone sends a command.

## Unit Tests
No automated unit tests exist yet — `register_fprime_ut` in `CMakeLists.txt` is present but commented out. Verification is currently manual bench testing (see Step 7 of the implementation plan for the full procedure: normal commands, limit-switch trip/param-disable, communication-fault latch/`clearError`, multi-instance isolation).
| Name | Description | Output | Coverage |
|---|---|---|---|
| *(none yet)* | | | |

## Requirements
| Name | Description | Validation |
|---|---|---|
| ROBOCLAW-001 | Shall drive the commanded motor (1 or 2) forward or reverse at the commanded PWM magnitude via Roboclaw packet serial. | Bench test: checklist #3 |
| ROBOCLAW-002 | Shall immediately stop a motor, overriding any commanded direction, if that motor's limit switch is enabled and tripped. | Bench test: Step 7.5 |
| ROBOCLAW-003 | Shall not apply the limit-switch override to an explicitly commanded `STOPPED` direction. | Bench test: Step 7.5 |
| ROBOCLAW-004 | Shall latch a communication fault and reject further motor commands until explicitly cleared, after a failed Roboclaw serial exchange. | Bench test: Step 7.6 |
| ROBOCLAW-005 | Shall support multiple independently-configured instances (distinct serial port + device address) without shared state between them. | Bench test: Step 7.4/7.6 (cross-instance isolation) |
| ROBOCLAW-006 | Shall reject a `setMotor` whose `pwm` exceeds 1000 with `VALIDATION_ERROR`, without sending anything to the Roboclaw. | GDS: checklist #1 |
| ROBOCLAW-007 | Shall answer each accepted `setMotor` exactly once, with a response that reflects that command's actual Roboclaw result (not a previous command's), or `EXECUTION_ERROR` if a `hardStop` cancelled it. | GDS: checklist #2, #12 |
| ROBOCLAW-008 | Shall send `round(pwm * 32767 / 1000)` as a signed duty (negative for `REVERSE`), and duty 0 for `STOPPED`. | Code inspection ("Command-to-Roboclaw Data Flow"); Bench test: checklist #8 |
| ROBOCLAW-009 | Shall publish `motor1`, `motor2`, `motor1LimitSwitch` and `motor2LimitSwitch` telemetry from the first `run` tick after boot (no command required) and on every subsequent tick. | GDS: reconnect after boot, channels present with no command sent (checklist #9) |
| ROBOCLAW-010 | Shall report each motor's polled limit-switch state on its `motorNLimitSwitch` channel (`false` when the switch is disabled by parameter or the read fails). | GDS: ground the limit-switch pin -> channel `true` (checklist #10) |
| ROBOCLAW-011 | `hardStop` shall stop both motors in every state (including a latched fault and a command in flight), and a `setMotor` it cancels shall never move a motor. | GDS: checklist #11, #12 |

## Deployment Notes
- **Queue depth.** Because the state machine's own signals (`tick`, `cmdRecv`, `success`/`fail`) share the component queue with `run` and the commands, `instances.fpp` gives each Roboclaw instance `Default.ROBOCLAW_QUEUE_SIZE` (10) rather than the default 3. Overflow is an `FW_ASSERT`.
- **Limit-switch wiring.** A limit switch must close to **GND** when tripped: the component treats a `LOW` read as "tripped". `Arduino::GpioDriver` only offers plain `INPUT`, so `setupTopology()` enables the Teensy's internal pull-up (`pinMode(pin, Arduino::DEF_INPUT_PULLUP)`) on the limit-switch pins 6-9; an open switch reads `HIGH`. Both `motorNHasLimitSwitch` parameters default to `true`, so an unwired pin with the pull-up reads `HIGH` (not tripped) and is harmless.
- **Addresses / ports.** `roboclaw1Manager` is address `0x80` on `Serial3` (pins 14 TX / 15 RX); `roboclaw2Manager` is address `0x81` on `Serial4` (pins 17 TX / 16 RX), 38400 baud.
- **Task priority.** All active components must be given priorities that are non-increasing in alphabetical start order (see the comment in `instances.fpp`); a bug in `Os::Baremetal::TaskRunner::addTask()` otherwise drops a task and its queue overflows.
- **Hard stop scope.** `hardStop` stops the motors of one Roboclaw only. To stop everything, send it to both instances (and `pca9685Manager.hardStop`); a sequence can do this in three commands.
- **Blocking.** A Roboclaw serial call with no controller attached blocks the cooperative main loop for its serial timeout (10 ms per try, up to 3 tries = roughly 30 ms) before the command fails and latches `checkErr`.
- **Telemetry traffic.** Each instance writes 4 channels every `run` tick (~100 ms: `rateDriver.configure(1)` ms x rate-group divisor 100), i.e. 8 channel writes per cycle for the two Roboclaw instances, so telemetry is always available to a late-connecting GDS. `motorEvent` is still logged only when a motor's state changes.

## GDS Bench Checklist
**Not yet re-run after the PWM + direction refactor.** Send from GDS (names are prefixed `billee_deployment.roboclaw1Manager.` / `roboclaw2Manager.`). Use a free-spinning motor and low PWM values.
| # | Action | Expected |
|---|---|---|
| 1 | `setMotor` MOTOR1 1001 FORWARD | `VALIDATION_ERROR`; no motion, no `motorEvent` |
| 2 | `setMotor` MOTOR1 0 STOPPED | `OK` and a `motorEvent` (STOPPED) if a controller answers; `EXECUTION_ERROR` if none is attached |
| 3 | `setMotor` MOTOR1 150 FORWARD, then 150 REVERSE, then STOPPED | motor turns one way, the other, then stops; `motor1` telemetry follows each command |
| 4 | Ground the motor's limit-switch pin, then `setMotor` FORWARD | immediate stop, `motorEvent` shows STOPPED, response reflects the stop |
| 5 | Unplug the controller's serial line, send any `setMotor` | `EXECUTION_ERROR`; a further `setMotor` is rejected at once (latched) |
| 6 | Reconnect, send `clearError`, then `setMotor` again | `clearError` `OK`; commands work again |
| 7 | Repeat 1-6 on the other instance | independent behavior (ROBOCLAW-005) |
| 8 | `setMotor` MOTOR1 1000 FORWARD | `OK`, motor at full duty (ROBOCLAW-008); confirm the instant duty step (no ramp) is acceptable for the drill motor |
| 9 | Reset the board, then open/reconnect GDS **without sending any command** | `motor1`/`motor2` show `STOPPED / 0 / OFF` and both `motorNLimitSwitch` channels are present and updating, on both instances (ROBOCLAW-009) |
| 10 | Ground a motor's limit-switch pin, then release it | that motor's `motorNLimitSwitch` goes `true`, then `false` (ROBOCLAW-010) |
| 11 | Both motors running, `hardStop`; then latch a fault (step 5) and `hardStop` again | both motors stop, one `hardStopEvent(true)`, `motor1`/`motor2` STOPPED; while latched `hardStop` is still sent (`EXECUTION_ERROR` with the line unplugged, no assert) |
| 12 | Sequence: `setMotor` MOTOR1 200 FORWARD immediately followed by `hardStop` | `setMotor` answered `EXECUTION_ERROR`, `hardStop` `OK`, motor does **not** start; a following `setMotor` works normally |

## Known Limitations
Documented behaviour that has not been changed:
- **A failed stop still reports `STOPPED`.** `motorStop` always reports `STOPPED / 0 / OFF` in `motor1`/`motor2` and `motorEvent`, even if the Roboclaw did not acknowledge the command (`FORWARD`/`REVERSE` report `OFF` on failure). Only the command response (`EXECUTION_ERROR`) shows the failure, so telemetry can say a motor is stopped while it may still be running.
- **A failed limit-switch stop does not latch `checkErr`.** On the limit-switch path `motorStop` enters `doWait` directly; the `fail` signal it then sends is ignored in `doWait` by the generated state machine, so the fault is not latched (the command is still answered `EXECUTION_ERROR`) and later commands are not rejected.
- **Limit-switch read failures read as "not tripped".** A failed `limitSwGet` call is treated as `false`, so it is also reported as `false` on `motorNLimitSwitch`.
- **Initial telemetry is assumed.** `STOPPED / 0 / OFF` before the first command is not read back from the Roboclaw (see "Telemetry").
- **`hardStop` is not a latch.** Once it has run, the next `setMotor` drives the motor again. A `setMotor` sent after `hardStop` in a sequence is treated as the operator's intent.

## Change Log
| Date | Description |
|---|---|
| 2026-09-23 | Initial implementation: motor direction/speed control, per-motor limit-switch safety stop, communication-fault latch + `clearError`, multi-instance support for multiple Roboclaw boards. |
| 2026-09-23 | RoboClaw constructed via placement new (no heap). `motorCmd` responses now come from the state-machine actions and reflect the real result (signals are queued, the previous handler answered with the previous command's result). Added `speed` > 127 -> `VALIDATION_ERROR`, single command in flight, `rejectCmd` action + `init`/`checkErr` handling of `cmdRecv` so no accepted command is left unanswered. Queue depth raised to 10. |
| 2026-09-23 | Documented the command-to-Roboclaw data flow (opcodes, wire format, retries) and the speed contract: 0-127 is the Roboclaw's raw duty, passed unscaled; 128-255 is rejected, not scaled. The `motorCmd` command description now states the range so it shows in the GDS command form (once, at the top; GDS cannot show it on the `speed` field alone). Telemetry: `motor1`/`motor2` now start as `STOPPED / 0 / OFF` and, with new `motor1LimitSwitch`/`motor2LimitSwitch` (bool) channels, are republished every `run` tick so they are always available. Added ROBOCLAW-008/009/010, checklist #8-10 and a Known Limitations section. |
| 2026-10-29 | PWM + direction refactor: struct-argument `motorCmd` replaced by flat `setMotor(motor, pwm, direction)` with `pwm` 0-1000 on the project-wide scale, sent with the signed 16-bit duty commands (`DutyM1/M2`) instead of the 0-127 compatibility commands. `yellowJacket.speed` (U8) became `pwm` (U16). Added `hardStop` (`DutyM1M2(0,0)`, any state, cancels an in-flight `setMotor` via `isCancelled`/`dropCmd`), `hardStopEvent`, ROBOCLAW-011, checklist #11-12. `motorDir` moved to `Types/MotorTypes.fpp`. |
