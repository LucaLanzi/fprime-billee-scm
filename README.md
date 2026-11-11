# fprime-billee-scm

F´ components for the BILLEE Science Control Module (Teensy 4.1), used by the
[fprime-arduino-billee-scm](https://github.com/LucaLanzi/fprime-arduino-billee-scm) deployment.

| Component | Hardware |
|---|---|
| `RelayManager` | Logic-level relays: pumps 1-4, UV light, magnetic stirrer |
| `PCA9685Manager` | PCA9685 servo board, servos S1-S16 |
| `RoboclawManager` | Three Roboclaw boards, five motors, lower/upper limit switches |
| `LoadCellManager` | Two HX711 load-cell ADCs (Sample Cache weight sensors) |
| `StepperManager` | Sample Cache carousel stepper (step/direction/motor-free driver) |
| `BootReporter` | Reports the previous boot's FW_ASSERT / hard fault and the SD card mount |

`Types/` holds the shared FPP types (`MotorDir`, `RelayId`, `ServoId`, `RoboclawMotor`, `LoadCellId`, ...).
Each component has a `docs/sdd.md`. The deployment README ("Commanding the Science Control Module") covers the shared
conventions: direction and speed, commands answered when the hardware finishes, and writing sequences.
