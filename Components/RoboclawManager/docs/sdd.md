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
- `m_roboclaw: RoboClaw*` — the vendored BasicMicro packet-serial driver (`lib/vendor-lib/RoboClaw/`), allocated in `configure()`, bound to this instance's `HardwareSerial`.
- `m_address: U8` — this instance's Roboclaw device address, set in `configure()`.
- `motorControlSM: MotorControlStateMachine` — an internal F´ state machine instance (see "Component States") that sequences command dispatch, limit-switch gating, and fault latching.
- `m_motor1SwitchTripped` / `m_motor2SwitchTripped: bool` — per-motor limit-switch state, refreshed once per `run` tick.
- `m_lastCmdOk: bool` — result of the most recent Roboclaw serial exchange, used to build command responses.
- `reportMotorState(const yellowJacket&)` — private helper, the single place that writes `motor1`/`motor2` telemetry and logs `motorEvent`; called from each of the three actions with the motor's *actual* resulting state, not the raw command.

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
| `init` | Initial state; transitions to `doWait` on the first `tick`. |
| `doWait` | Idle, waiting for a `motorCmd`. Self-loops on `tick`. |
| `doCmd` | Dispatches a received command: checks the target motor's limit switch first (immediate stop + return to `doWait` if tripped and enabled), otherwise runs the matching `motorFwd`/`motorRev`/`motorStop` action and awaits `success`/`fail`. |
| `checkErr` | Latched communication-fault state, entered when a Roboclaw serial exchange fails. Rejects further commands (checked directly in `motorCmd_cmdHandler`, not via a state-machine transition) until `clearError` sends `errClr`, returning to `doWait`. |

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
| `motorCmd` | Drive the specified motor (`motorNum`: 1 or 2) in the given direction (`motorDir`: `FORWARD`/`REVERSE`/`STOPPED`) at the given `speed` (0-127). Rejected with `EXECUTION_ERROR` if the instance is currently latched in `checkErr`. |
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

## Change Log
| Date | Description |
|---|---|
| 2026-09-23 | Initial implementation: motor direction/speed control, per-motor limit-switch safety stop, communication-fault latch + `clearError`, multi-instance support for multiple Roboclaw boards. |
