module billeeScm {
    @ Component to toggle logic relays controlling the magnetic stirrers.
    active component StirrerManager {

        @ async Input port for the rate group driver
        async input port run: Svc.Sched

        @ Command to toggle the desired magnetic stirrer
        async command stirrerToggle(
            toggle: Fw.On @< Desired stirrer state
            stirrer: billeeScm.stirrerId @< which stirrer to control
        ) opcode 0

        @ Telemetry to keep stirrer 1 state
        telemetry stirrer1State: Fw.Enabled

        @ Telemetry to keep stirrer 2 state
        telemetry stirrer2State: Fw.Enabled

        @ Output ports driving each stirrer relay's physical GPIO pin
        output port stirrer1Set: Drv.GpioWrite
        output port stirrer2Set: Drv.GpioWrite

        @ Event to signal stirrer toggle
        event stirrerState (
            stirrer: billeeScm.stirrerId @< which stirrer changed
            toggleState: Fw.Enabled      @< the stirrer's new state
        ) \
        severity activity high \
        id 0 \
        format "Stirrer {} is {}"

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