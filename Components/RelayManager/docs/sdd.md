# billeeScm::RelayManager

Drives the Science Control Module's six logic-level relays (Science Hardware Architecture 2027):

| RelayId | Load | Pin | Supply |
|---|---|---|---|
| `PUMP1`..`PUMP4` | Chemical Management pumps 1-4 | T_GPIO_39 / 38 / 37 / 36 | +12V |
| `UV_LIGHT` | UV Spectroscopy UV light | T_GPIO_40 | +22.2V |
| `STIRRER` | Magnetic stirrer MS1 | T_GPIO_34 | +12V |

The `RelayId` value is the index of the `relaySet` output port; the topology wires each index to its `Arduino.GpioDriver`
(pins in `SciencePins.hpp`). HIGH = relay energized = load on.

## Commands

| Command | Answers | Notes |
|---|---|---|
| `SET relay ON\|OFF` | at once | Cancels an `ON_FOR` running on that relay (it is answered `EXECUTION_ERROR`). |
| `ON_FOR relay durationMs` | when the relay switches **off** again | 1-300000 ms, else `VALIDATION_ERROR`. Rejected if one is already running on the relay. Resolution: one rate-group tick (100 ms). |
| `ALL_OFF` | at once | Every relay off; every running `ON_FOR` cancelled. |

Because `ON_FOR` is answered only at the end of the run, in a sequence the next command starts after the run:

```
R00:00:00 billee_deployment.relayManager.ON_FOR PUMP1 3000   ; dose for 3 s
R00:00:00 billee_deployment.relayManager.SET STIRRER ON      ; starts after the dose
```

For runs longer than 5 minutes use `SET ON`, a relative wait, then `SET OFF`.

## Behavior

- All relays are off at boot. The topology also drives every relay pin LOW as soon as it is opened.
- Every tick (`run`, 10 Hz) re-drives each pin from the commanded state and ends `ON_FOR` runs whose time is up. Duration
  is measured with the time service, not by counting ticks.
- `run` is `drop`: a tick that arrives while the queue is full is discarded rather than asserting (the TaskRunner is
  cooperative, so another component blocking for a while must not crash this one).

## Telemetry and events

- `relayStates` (every tick): ON/OFF per relay, indexed by `RelayId`.
- `RelaySwitched` on each change, `OnForCancelled` when a timed run is cut short, `GpioWriteFailed` if a pin write fails.
