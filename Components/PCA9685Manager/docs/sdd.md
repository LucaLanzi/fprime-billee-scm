# billeeScm::PCA9685Manager

Drives the PCA9685 16-channel PWM board over I2C (`Wire`, SDA 18 / SCL 19, 100 kHz, address 0x40): up to 16 servos,
`S1`-`S16`, numbered like the block diagram's `PCA_S1`... (`S1` = board channel 0).

| Servo | Role (Science Hardware Architecture 2027) |
|---|---|
| `S1` | Soil cup tilting servo |
| `S2` | Drape lowering servo |
| `S3` | Cache lid servo |
| `S4` | Funnel flap servo |
| `S5`-`S16` | Spare |

## Commands

| Command | Notes |
|---|---|
| `SERVO_SET servo dir speed` | `speed` 0-100 % of the servo's travel (positional) or speed (continuous) from its neutral pulse, in direction `FORWARD`/`REVERSE`. `STOPPED` or 0 holds neutral. |
| `SERVO_OFF servo` | Stops pulsing that channel: the servo goes limp. |
| `SERVO_PULSE servo counts` | Raw pulse for calibration, in counts of 4096 per 20 ms frame. Must be inside the channel's limits (0 = release). |
| `STOP_ALL` | Releases every channel in one transaction. The OE pin is not wired, so this is the software stop. |
| `REINIT` | Re-runs initialization (e.g. after powering the board after the Teensy). |

Drive commands are refused (`EXECUTION_ERROR`) until initialization succeeded; `SERVO_OFF` and `STOP_ALL` always try the
bus.

## Calibration

`kServoCal` in `PCA9685Manager.cpp` holds, per channel, the min / neutral / max pulse and a `reversed` flag. No command
can send a pulse outside [min, max], so a positional servo cannot be driven into its end stops. Defaults are the nominal
0.5 / 1.5 / 2.5 ms (102 / 307 / 512 counts). Bench-tune each servo with `SERVO_PULSE` and copy the values into the table.

## Telemetry and events

- `servoStates` (every tick): direction, speed and pulse sent per servo.
- `i2cErrorCount`, `lastI2cError` (Wire code: 2 = address NACK, i.e. no board answering).
- `I2cFault`, logged only when the error code changes, so a dead bus cannot flood the event queue.

`run` is `drop` and does no I2C. Without a board attached, initialization fails with code 2 and drive commands answer
`EXECUTION_ERROR`.
