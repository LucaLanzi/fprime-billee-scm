module billeeScm {
    @ Component to control Science Control Module pumps
    active component PumpManager {

        # One async command/port is required for active components
        # This should be overridden by the developers with a useful command/port
        @  input port of type Svc.Sched to invoke the component
        async input port run: Svc.Sched

        @ Command to toggle pump
        async command pumpToggle (
            toggle: Fw.On           @< desired pump state
            pump: billeeScm.pumpId  @< which pump to control
            ) opcode 0

        @ Telemetry to keep pump 1 state
        telemetry pump1State: Fw.Enabled

        @ Telemetry to keep pump 2 state
        telemetry pump2State: Fw.Enabled

        @ Telemetry to keep pump 3 state
        telemetry pump3State: Fw.Enabled

        @ Telemetry to keep pump 4 state
        telemetry pump4State: Fw.Enabled

        @ Output ports driving each pump's physical GPIO pin
        output port pump1Set: Drv.GpioWrite
        output port pump2Set: Drv.GpioWrite
        output port pump3Set: Drv.GpioWrite
        output port pump4Set: Drv.GpioWrite

        @ Event to signal pump toggle
        event pumpState (
            pump: billeeScm.pumpId   @< which pump changed
            toggleState: Fw.Enabled  @< the pump's new state
        ) \
        severity activity high \
        id 0 \
        format "Pump {} is {}"



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
