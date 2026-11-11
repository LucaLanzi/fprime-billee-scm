module billeeScm {
    @ Drives the Science Control Module's logic-level relays: Chemical Management pumps 1-4, the UV light and the
    @ magnetic stirrer (RelayId). One relaySet GPIO output per relay, wired in the topology. Every relay is off at
    @ boot and the pins are re-driven from the commanded state on every tick.
    active component RelayManager {

        @ Rate group tick: re-drives every relay pin, ends ON_FOR runs whose time is up, publishes telemetry.
        @ Drops the tick if the queue is full (a long blocking call elsewhere must not assert this component).
        async input port run: Svc.Sched drop

        @ GPIO write for each relay, indexed by RelayId
        output port relaySet: [RELAY_COUNT] Drv.GpioWrite

        @ Switch one relay on or off. Cancels an ON_FOR still running on that relay (that command is answered
        @ EXECUTION_ERROR).
        async command SET(
            relay: billeeScm.RelayId @< Relay to switch
            onOff: Fw.On             @< ON or OFF
        ) opcode 0x00

        @ Switch a relay on for durationMs, then off. The command is answered only when the relay switches off
        @ (OK), so in a sequence the next command starts after the run. Resolution is one rate-group tick (100 ms
        @ at 10 Hz). durationMs must be 1-MAX_ON_FOR_MS (300000 = 5 min), else VALIDATION_ERROR; for longer runs use
        @ SET ON, a relative wait, then SET OFF. Rejected (EXECUTION_ERROR) if an ON_FOR is already running on the
        @ relay. SET or ALL_OFF on the relay cancels it (EXECUTION_ERROR).
        async command ON_FOR(
            relay: billeeScm.RelayId @< Relay to run
            durationMs: U32          @< How long to keep it on, in milliseconds
        ) opcode 0x01

        @ Switch every relay off and cancel every running ON_FOR (each answered EXECUTION_ERROR)
        async command ALL_OFF() opcode 0x02

        @ A relay changed state
        event RelaySwitched(
            relay: billeeScm.RelayId @< Relay
            newState: Fw.On          @< New state
        ) \
          severity activity high \
          id 0x00 \
          format "Relay {} switched {}"

        @ A running ON_FOR was cancelled by SET or ALL_OFF before its time was up
        event OnForCancelled(
            relay: billeeScm.RelayId @< Relay
            elapsedMs: U32           @< How long it had been on
        ) \
          severity warning low \
          id 0x01 \
          format "ON_FOR on relay {} cancelled after {} ms"

        @ Writing a relay's GPIO failed (pin not opened in the topology?)
        event GpioWriteFailed(
            relay: billeeScm.RelayId @< Relay
        ) \
          severity warning high \
          id 0x02 \
          format "GPIO write to relay {} failed"

        @ State of every relay
        telemetry relayStates: billeeScm.RelayStates

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
