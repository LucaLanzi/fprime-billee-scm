module billeeScm {
    @ Component to toggle logic relays controlling the magnetic stirrers.
    active component StirrerManager {

        @ async Input port for the rate group driver
        async input port run: Svc.Sched

        @ Command to toggle the desired magnetic stirrer
        async command stirrerToggle(
            toggle: Fw.On @< Desired stirrer state
            stirrer: billeeScm.stirrerId
        )
        
        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

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