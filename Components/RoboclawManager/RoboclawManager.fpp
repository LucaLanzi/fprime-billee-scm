module billeeScm {
    @ Component capable of driving two seperate motors over Serial.
    active component RoboclawManager {

        state machine instance billeeScm.MotorControlStateMachine: RoboclawStateMachine

        @ Input port for run handler
        async input port run: Svc.Sched 

        @ Output port to get the value of the limit switch
        output port limitSwGet: Drv.GpioRead

      @ Command to sent to roboclaw
        async command motorCmd (
            motor: billeeScm.yellowjacket
            ) opcode 0x00

        event motorEvent(
            motor: yellowJacket
            ) \
              severity warning low
              id 0x00 \
              format "{}" 

        @ telemetry for motor state
        telemetry billeeScm.yellowJacket : motor1

        telemetry billeeScm.yellowJacket : motor2

        telemetry billeeScm.yellowJacket : motor3

        ################################################

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