module billeeScm {
    @ Reads the Sample Cache weight sensors: load cells on HX711 Wheatstone-bridge ADCs (LoadCellId), bit-banged on
    @ two GPIOs each (pins given in configure()). Converts to grams with a per-cell tare offset and scale. Tare and
    @ calibration live in RAM: tare from the GDS (or a sequence) before a run, and restore a known scale with SET_SCALE.
    @
    @ TARE, CALIBRATE and WAIT_MASS are answered when they finish, so in a sequence the next command starts after.
    active component LoadCellManager {

        @ Rate group tick (10 Hz = the HX711's default sample rate): reads every cell that has a sample ready, runs
        @ pending TARE / CALIBRATE / WAIT_MASS, publishes telemetry. Drops the tick if the queue is full.
        async input port run: Svc.Sched drop

        @ Set the cell's zero: average `samples` fresh readings (1-80; 10 per second) and use the mean as 0 g.
        @ EXECUTION_ERROR if the cell is not connected or stops sending samples.
        async command TARE(
            cell: billeeScm.LoadCellId @< Load cell
            samples: U8                @< Readings to average, 1-80
        ) opcode 0x00

        @ With a known mass on the (tared) cell: average `samples` readings and set countsPerGram so that it reads
        @ knownGrams. Emits Calibrated with the factor (record it, and restore it later with SET_SCALE).
        async command CALIBRATE(
            cell: billeeScm.LoadCellId @< Load cell
            knownGrams: F32            @< Mass on the cell, grams (> 0)
            samples: U8                @< Readings to average, 1-80
        ) opcode 0x01

        @ Set the scale directly, e.g. a factor recorded from CALIBRATE. Must be non-zero.
        async command SET_SCALE(
            cell: billeeScm.LoadCellId @< Load cell
            countsPerGram: F32         @< Raw counts per gram (negative if the cell is mounted inverted)
        ) opcode 0x02

        @ Wait until the cell reads ABOVE (>=) or BELOW (<=) `grams` (answered OK), or timeoutMs passes / the cell
        @ disconnects (EXECUTION_ERROR). E.g. pump with relayManager.SET, WAIT_MASS ... ABOVE 50, then SET OFF.
        @ 1-300000 ms.
        async command WAIT_MASS(
            cell: billeeScm.LoadCellId     @< Load cell
            compare: billeeScm.MassCompare @< ABOVE or BELOW
            grams: F32                     @< Threshold, grams
            timeoutMs: U32                 @< Give up after this many milliseconds
        ) opcode 0x03

        @ TARE finished
        event Tared(
            cell: billeeScm.LoadCellId @< Load cell
            tareOffset: I32            @< New zero, raw counts
        ) \
          severity activity high \
          id 0x00 \
          format "{} tared: offset {} counts"

        @ CALIBRATE finished. Record the factor to restore it with SET_SCALE after a reboot.
        event Calibrated(
            cell: billeeScm.LoadCellId @< Load cell
            countsPerGram: F32         @< New scale
        ) \
          severity activity high \
          id 0x01 \
          format "{} calibrated: {} counts per gram"

        @ WAIT_MASS reached its threshold
        event MassReached(
            cell: billeeScm.LoadCellId @< Load cell
            grams: F32                 @< Reading that met the threshold
        ) \
          severity activity high \
          id 0x02 \
          format "{} reached {} g"

        @ TARE / CALIBRATE / WAIT_MASS gave up (timeout, cell disconnected, or superseded by another command on the cell)
        event CellCommandFailed(
            cell: billeeScm.LoadCellId @< Load cell
            grams: F32                 @< Last reading
        ) \
          severity warning high \
          id 0x03 \
          format "{} command failed (last reading {} g)"

        @ A cell started sending samples
        event CellConnected(
            cell: billeeScm.LoadCellId @< Load cell
        ) \
          severity activity high \
          id 0x04 \
          format "{} connected"

        @ A cell stopped sending samples (no sample for 1 s)
        event CellDisconnected(
            cell: billeeScm.LoadCellId @< Load cell
        ) \
          severity warning high \
          id 0x05 \
          format "{} not sending samples"

        @ State of every load cell
        telemetry cellStates: billeeScm.LoadCellStates

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel
    }
}
