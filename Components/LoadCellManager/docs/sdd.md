# billeeScm::LoadCellManager

Reads the Sample Cache weight sensors: two load cells on HX711 Wheatstone-bridge ADCs (`CELL1` on DAT 24 / CLK 25,
`CELL2` on DAT 26 / CLK 27; the block diagram's "Dat/CLK Bus 1/2"). Power the HX711 boards from 3.3 V.

## Reading

Every 10 Hz tick (the HX711's default rate) each cell whose DOUT is LOW (sample ready) is read: 24 bits MSB first plus one
pulse selecting channel A at gain 128. Interrupts are off only while clocking (~50 us): CLK held HIGH for more than
60 us powers the HX711 down. DAT uses a pull-up, so a cell that is not attached reads "not ready" and shows as
disconnected (no sample for 1 s) instead of producing random values.

`grams = (raw - tareOffset) / countsPerGram`.

## Commands

| Command | Answered | Notes |
|---|---|---|
| `TARE cell samples` | after `samples` fresh readings (1-80, 10 per second) | Sets `tareOffset` to their mean. |
| `CALIBRATE cell knownGrams samples` | after the readings | With a known mass on a tared cell: sets `countsPerGram`. `Calibrated` reports the factor. |
| `SET_SCALE cell countsPerGram` | at once | Restores a recorded factor. Non-zero; negative if the cell is mounted inverted. |
| `WAIT_MASS cell ABOVE\|BELOW grams timeoutMs` | when a reading meets the threshold (OK) | Timeout or disconnect answers `EXECUTION_ERROR`. 1-300000 ms. |

`TARE`, `CALIBRATE` and `WAIT_MASS` answer `EXECUTION_ERROR` at once if the cell is disconnected, and if a newer command
on the same cell supersedes them. Tare and scale are kept in RAM: tare from the GDS before a run, and restore the scale
with `SET_SCALE` after a reboot (or set a measured default in `billee_deploymentTopology.cpp`).

## Telemetry and events

- `cellStates`: connected, raw, grams, tare offset, scale per cell.
- `Tared`, `Calibrated`, `MassReached`, `CellCommandFailed`, `CellConnected`, `CellDisconnected` (on transitions).
