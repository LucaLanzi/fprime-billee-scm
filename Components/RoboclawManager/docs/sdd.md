# billeeScm::RoboclawManager

One component for all three Roboclaw 2x7A boards (packet serial, 38400 baud) and the five motors they drive.

| Motor | Board / channel | Role | Lower / upper limit |
|---|---|---|---|
| `R1_M1` | R1 (Serial1, 0x80) M1 | Soil drill actuator | T_GPIO_23 / 22 |
| `R1_M2` | R1 M2 | Transfer tube actuator (drill up/down) | 21 / 15 |
| `R2_M1` | R2 (Serial2, 0x81) M1 | Transfer tube lead screw | 14 / 41 (drill tube to ground contact) |
| `R2_M2` | R2 M2 | Drill | none |
| `R3_M1` | R3 (Serial4, 0x82) M1 | Sample rotation linear actuator | 33 / 32 |

Limit switches close to GND (tripped = `LOW`, pins use the internal pull-up). `limitSwGet[2 * motor]` is the lower
switch, `[2 * motor + 1]` the upper; an unconnected port means no switch.

## Direction rule

`FORWARD` always moves a motor toward its **upper** switch, `REVERSE` toward its **lower** one. If a motor turns the
wrong way on the bench, set `configureMotor(motor, true)` in `billee_deploymentTopology.cpp`; that negates its duty and
its encoder count.

## Commands

| Command | Answered | Notes |
|---|---|---|
| `MOVE motor dir speed` | when the Roboclaw acknowledges | Runs until `STOP`, a limit switch or another command. `STOPPED` or 0 stops. |
| `RUN_FOR motor dir speed ms` | when the time is up (OK) | A limit switch stopping it first answers `EXECUTION_ERROR`. 1-300000 ms. |
| `TO_LIMIT motor dir speed timeoutMs` | when the switch in `dir` trips (OK) | Timeout stops it and answers `EXECUTION_ERROR`. OK at once if already there. `VALIDATION_ERROR` if no switch in that direction. |
| `STOP motor` | at once | Answers a running `RUN_FOR`/`TO_LIMIT` with `EXECUTION_ERROR`. |
| `STOP_ALL` | at once | One stop packet per board. Fails only if a board that may have a motor running does not acknowledge. |
| `RESET_ENCODERS board` | at once | |

`speed` is 0-100 % duty. A command refused because its switch is already tripped emits `LimitBlocked`. A new command on
a motor supersedes the one running on it (`EXECUTION_ERROR`).

## Tick (50 Hz, `rateGroup2`)

1. Read every limit switch.
2. Stop any motor moving toward a tripped switch, and end `RUN_FOR` / `TO_LIMIT` whose time is up.
3. One serial transaction: rotate through encoders, currents, battery voltage and error bits of the online boards, or
   probe an offline board (every 2 s). A board that does not answer costs 30 ms (the library's 3 attempts of 10 ms),
   so this budget keeps the cooperative scheduler responsive with boards missing.
4. Publish telemetry every 5th tick (10 Hz).

## Telemetry and events

- `motorStates`: mode, direction, speed, lower/upper switch, encoder, current (mA) per motor.
- `boardStates`: online, main battery (0.1 V), error bits per board.
- `MotorStarted`, `MotorStopped` (reason: commanded, limit switch, time up, timeout), `LimitBlocked`,
  `BoardOnline`, `BoardOffline` (on transitions only).
