module billeeScm {
    @ UV Manager
    active component UvManager {
        import Fw.Command
        import Fw.Event
        import Fw.Channel

        time get port timeCaller

        # One async command/port is required for active components
        @  input port of type Svc.Sched to invoke the component
        async input port run: Svc.Sched

        @ Command to toggle relay
        async command uvToggle(toggle: Fw.On) opcode 0

        telemetry uvState: Fw.Enabled

        output port uvSet: Drv.GpioWrite

        event uvStateChange(toggleState: Fw.Enabled) \
        severity activity high \
        id 0 \
        format "Uv is {}"




    }
}