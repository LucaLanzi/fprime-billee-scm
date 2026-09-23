# billeeScm::RoboclawManager

Drives up to two motors on a single BasicMicro Roboclaw packet-serial motor controller, with per-motor optional limit-switch safety stop and latched communication-fault handling. Designed to be instantiated once per physical Roboclaw board — multiple independent boards are supported by running multiple instances, each bound to its own serial port and device address via `configure()`.

## Usage Examples

### Diagrams
See "Class Diagram" and "Sequence Diagrams" below.

### Typical Usage
1. In the topology, instantiate one `RoboclawManager` per physical Roboclaw board (see `instances.fpp`).
2. At boot, before the rate group starts ticking the instance, call `configure(HardwareSerial* serial, U8 address, uint32_t baud = 38400)` once to bind it to that board's serial line and packet-serial address.
3. Send the `motorCmd` command to drive a specific motor (1 or 2) forward, reverse, or stop at a given speed.
4. The component reports the actual resulting motor state via the `motor1`/`motor2` telemetry channels and the `motorEvent` log after every command and every limit-switch-triggered stop.
5. If the Roboclaw board stops acknowledging commands (wiring/power fault), the instance latches into an error state and rejects further `motorCmd`s with `EXECUTION_ERROR` until `clearError` is sent.

## Class Diagram
`RoboclawManager` owns:
- `m_roboclaw: RoboClaw*` — the vendored BasicMicro packet-serial driver (`lib/vendor-lib/RoboClaw/`), bound to this instance's `HardwareSerial`. It is constructed in `configure()` with **placement new into the static buffer `m_roboclawStorage`, not on the heap**: this deployment never registers a memory allocator with `Os::Baremetal::OverrideNewDelete`, so a plain `new` would trip `FW_ASSERT(pAllocator != nullptr)`.
- `m_address: U8` — this instance's Roboclaw device address, set in `configure()`.
- `motorControlSM: MotorControlStateMachine` — an internal F´ state machine instance (see "Component States") that sequences command dispatch, limit-switch gating, and fault latching. State-machine signals are **queued on the component's own message queue** and dispatched later, they are not run inline.
- `m_motor1SwitchTripped` / `m_motor2SwitchTripped: bool` — per-motor limit-switch state, refreshed once per `run` tick.
- `m_cmdPending` / `m_pendingOpCode` / `m_pendingCmdSeq` — the single `motorCmd` currently in flight. Because signals are queued, `motorCmd_cmdHandler` cannot know the result of the Roboclaw exchange when it returns; it records the opcode/sequence and the state-machine *action* answers via `completePendingCmd()` once the real result is known.
- `reportMotorState(const yellowJacket&)` — private helper, the single place that writes `motor1`/`motor2` telemetry and logs `motorEvent`; called from each of the three actions with the motor's *actual* resulting state, not the raw command.
- `completePendingCmd(response)` — sends the command response for the in-flight command (no-op if none). Called by `motorFwd`/`motorRev`/`motorStop`/`rejectCmd`.

## Port Descriptions
| Name | Description |
|---|---|
| `run` | `Svc.Sched` input; drives the state machine's `tick` signal and refreshes cached limit-switch state once per rate-group cycle. |
| `limitSwGet` | A 2-element array of `Drv.GpioRead` output ports (`[2] Drv.GpioRead`); index 0 reads Motor1's limit switch, index 1 reads Motor2's. Only invoked for a motor whose `hasLimitSwitch` param is `true`. |
| `motorCmd` (command) | Commands one motor (1 or 2) to a direction (`FORWARD`/`REVERSE`/`STOPPED`) and speed (0-127). |
| `clearError` (command) | Clears a latched communication-fault (`checkErr`) state. |
| `motor1` / `motor2` (telemetry) | The actual resulting state of each motor after the most recent command or limit-switch stop. |
| `motorEvent` (event) | Logged every time a motor's actual state changes (command-driven or limit-switch-triggered). |

## Component States
| Name | Description |
|---|---|
| `init` | Initial state; transitions to `doWait` on the first `tick`. A command that arrives before the first tick is accepted (`cmdRecv` goes straight to command dispatch) so it is never dropped. |
| `doWait` | Idle, waiting for a `motorCmd`. Self-loops on `tick`. |
| `doCmd` | Dispatches a received command: checks the target motor's limit switch first (immediate stop + return to `doWait` if tripped and enabled), otherwise runs the matching `motorFwd`/`motorRev`/`motorStop` action and awaits `success`/`fail`. |
| `checkErr` | Latched communication-fault state, entered when a Roboclaw serial exchange fails. `motorCmd_cmdHandler` rejects new commands immediately while latched; a command that was already accepted and reaches this state (queued behind the `fail` signal) is answered `EXECUTION_ERROR` by the `rejectCmd` action. Stays latched until `clearError` sends `errClr`, returning to `doWait`. |

Every accepted `motorCmd` reaches exactly one action, and that action sends exactly one command response.

## Sequence Diagrams

**Normal command:**
`motorCmd` received → `cmdRecv` signal → limit-switch check (not tripped) → matching action runs the Roboclaw serial call → `success`/`fail` signal → telemetry/event updated with actual result → command response (`OK`/`EXECUTION_ERROR`) → back to `doWait`.

**Limit-switch-triggered stop:**
`motorCmd(FORWARD/REVERSE)` received while that motor's enabled limit switch is tripped → `motorStop` action runs directly (bypassing the commanded direction) → telemetry/event report `STOPPED` → state machine returns straight to `doWait` (not through `doCmd`'s normal `success` path).

**Communication fault and recovery:**
Roboclaw serial exchange fails inside an action → `fail` signal → `checkErr` (latched) → subsequent `motorCmd`s rejected immediately with `EXECUTION_ERROR` (state checked before forwarding to the state machine) → operator sends `clearError` → `errClr` signal → back to `doWait` → normal commands work again.

## Parameters
| Name | Description |
|---|---|
| `motor1HasLimitSwitch` | `bool`, default `true`. Whether Motor1 has a limit switch wired; if `false`, its `limitSwGet[0]` port is never invoked and its switch is ignored. |
| `motor2HasLimitSwitch` | `bool`, default `true`. Same, for Motor2 / `limitSwGet[1]`. |

## Commands
| Name | Description |
|---|---|
| `motorCmd` | Drive the specified motor (`motorNum`: 1 or 2) in the given direction (`motorDir`: `FORWARD`/`REVERSE`/`STOPPED`) at the given `speed` (0-127). Answered `VALIDATION_ERROR` if `speed` > 127 (nothing is sent on the serial line), `EXECUTION_ERROR` if the instance is latched in `checkErr` or another `motorCmd` is still in flight, otherwise the response reflects the real result of the Roboclaw exchange (`OK` / `EXECUTION_ERROR`). |
| `clearError` | Clears a latched `checkErr` communication-fault state. Safe to send at any time — a no-op if not currently latched. |

## Events
| Name | Description |
|---|---|
| `motorEvent` | Warning (low), logged with the motor's actual resulting state (`yellowJacket`) every time a command runs or a limit switch triggers a stop. |

## Telemetry
| Name | Description |
|---|---|
| `motor1` | Actual current state (`yellowJacket`: motor number, direction, speed, on/off) of Motor1, updated after every command/stop affecting it. |
| `motor2` | Same, for Motor2. |

## Unit Tests
No automated unit tests exist yet — `register_fprime_ut` in `CMakeLists.txt` is present but commented out. Verification is currently manual bench testing (see Step 7 of the implementation plan for the full procedure: normal commands, limit-switch trip/param-disable, communication-fault latch/`clearError`, multi-instance isolation).
| Name | Description | Output | Coverage |
|---|---|---|---|
| *(none yet)* | | | |

## Requirements
| Name | Description | Validation |
|---|---|---|
| ROBOCLAW-001 | Shall drive the commanded motor (1 or 2) forward or reverse at the commanded speed via Roboclaw packet serial. | Bench test: Step 7.4 |
| ROBOCLAW-002 | Shall immediately stop a motor, overriding any commanded direction, if that motor's limit switch is enabled and tripped. | Bench test: Step 7.5 |
| ROBOCLAW-003 | Shall not apply the limit-switch override to an explicitly commanded `STOPPED` direction. | Bench test: Step 7.5 |
| ROBOCLAW-004 | Shall latch a communication fault and reject further motor commands until explicitly cleared, after a failed Roboclaw serial exchange. | Bench test: Step 7.6 |
| ROBOCLAW-005 | Shall support multiple independently-configured instances (distinct serial port + device address) without shared state between them. | Bench test: Step 7.4/7.6 (cross-instance isolation) |
| ROBOCLAW-006 | Shall reject a `motorCmd` whose `speed` exceeds 127 with `VALIDATION_ERROR`, without sending anything to the Roboclaw. | GDS: `motorCmd` FORWARD speed 200 -> `VALIDATION_ERROR` |
| ROBOCLAW-007 | Shall answer each accepted `motorCmd` exactly once, with a response that reflects that command's actual Roboclaw result (not a previous command's). | GDS: `motorCmd` STOPPED with a controller attached -> `OK`; with none attached -> `EXECUTION_ERROR` |

## Deployment Notes
- **Queue depth.** Because the state machine's own signals (`tick`, `cmdRecv`, `success`/`fail`) share the component queue with `run` and the commands, `instances.fpp` gives each Roboclaw instance `Default.ROBOCLAW_QUEUE_SIZE` (10) rather than the default 3. Overflow is an `FW_ASSERT`.
- **Limit-switch wiring.** A limit switch must close to **GND** when tripped: the component treats a `LOW` read as "tripped". `Arduino::GpioDriver` only offers plain `INPUT`, so `setupTopology()` enables the Teensy's internal pull-up (`pinMode(pin, Arduino::DEF_INPUT_PULLUP)`) on the limit-switch pins 6-9; an open switch reads `HIGH`. Both `motorNHasLimitSwitch` parameters default to `true`, so an unwired pin with the pull-up reads `HIGH` (not tripped) and is harmless.
- **Addresses / ports.** `roboclaw1Manager` is address `0x80` on `Serial3` (pins 14 TX / 15 RX); `roboclaw2Manager` is address `0x81` on `Serial4` (pins 17 TX / 16 RX), 38400 baud.
- **Task priority.** All active components must be given priorities that are non-increasing in alphabetical start order (see the comment in `instances.fpp`); a bug in `Os::Baremetal::TaskRunner::addTask()` otherwise drops a task and its queue overflows.
- **Blocking.** A Roboclaw serial call with no controller attached blocks the cooperative main loop for its serial timeout (10 ms per try, with retries) before the command fails and latches `checkErr`.

## GDS Bench Checklist
Send from GDS (names are prefixed `billee_deployment.roboclaw1Manager.` / `roboclaw2Manager.`). Use a free-spinning motor and low speeds.
| # | Action | Expected |
|---|---|---|
| 1 | `motorCmd` MOTOR1 FORWARD speed 200 | `VALIDATION_ERROR`; no motion, no `motorEvent` |
| 2 | `motorCmd` MOTOR1 STOPPED speed 0 | `OK` and a `motorEvent` (STOPPED) if a controller answers; `EXECUTION_ERROR` if none is attached |
| 3 | `motorCmd` MOTOR1 FORWARD speed 20, then STOPPED | motor turns then stops; `motor1` telemetry follows each command |
| 4 | Ground the motor's limit-switch pin, then `motorCmd` FORWARD | immediate stop, `motorEvent` shows STOPPED, response reflects the stop |
| 5 | Unplug the controller's serial line, send any `motorCmd` | `EXECUTION_ERROR`; a further `motorCmd` is rejected at once (latched) |
| 6 | Reconnect, send `clearError`, then `motorCmd` again | `clearError` `OK`; commands work again |
| 7 | Repeat 1-6 on the other instance | independent behavior (ROBOCLAW-005) |

## Change Log
| Date | Description |
|---|---|
| 2026-09-23 | Initial implementation: motor direction/speed control, per-motor limit-switch safety stop, communication-fault latch + `clearError`, multi-instance support for multiple Roboclaw boards. |
| 2026-09-23 | RoboClaw constructed via placement new (no heap). `motorCmd` responses now come from the state-machine actions and reflect the real result (signals are queued, the previous handler answered with the previous command's result). Added `speed` > 127 -> `VALIDATION_ERROR`, single command in flight, `rejectCmd` action + `init`/`checkErr` handling of `cmdRecv` so no accepted command is left unanswered. Queue depth raised to 10. |
