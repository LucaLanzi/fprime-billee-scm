module billeeScm {
    @ Component to control Science Control Module relays
    active component RelayManager {

        # One async command/port is required for active components
        # This should be overridden by the developers with a useful command/port
        @  input port of type Svc.Sched to invoke the component
        async input port run: Svc.Sched

        @ Command to toggle relay
        async command relayToggle (
            toggle: Fw.On             @< desired relay state
            relay: billeeScm.relayId  @< which relay to control
            ) opcode 0

        @ Telemetry to keep relay 1 state
        telemetry relay1State: Fw.Enabled

        @ Telemetry to keep relay 2 state
        telemetry relay2State: Fw.Enabled

        @ Telemetry to keep relay 3 state
        telemetry relay3State: Fw.Enabled

        @ Telemetry to keep relay 4 state
        telemetry relay4State: Fw.Enabled

        @ Output ports driving each relay's physical GPIO pin
        output port relay1Set: Drv.GpioWrite
        output port relay2Set: Drv.GpioWrite
        output port relay3Set: Drv.GpioWrite
        output port relay4Set: Drv.GpioWrite

        @ Event to signal relay toggle
        event relayState (
            relay: billeeScm.relayId  @< which relay changed
            toggleState: Fw.Enabled   @< the relay's new state
        ) \
        severity activity high \
        id 0 \
        format "Relay {} is {}"

    

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"

        # @ Example parameter
        # param PARAMETER_NAME: U32

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}