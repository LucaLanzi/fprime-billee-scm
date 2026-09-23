module billeeScm {
    @ Component capable of driving two seperate motors over Serial.
    active component RoboclawManager {

        state machine instance motorControlSM: MotorControlStateMachine

        @ Input port for run handler
        async input port run: Svc.Sched 

        @ Output port to get the value of the limit switch (index 0 = Motor1, index 1 = Motor2)
        output port limitSwGet: [2] Drv.GpioRead

      @ Command to sent to roboclaw
        async command motorCmd (
            motor: billeeScm.yellowJacket
            ) opcode 0x00

        @ Clears a latched communication-fault (checkErr) state, allowing further motorCmds
        async command clearError() opcode 0x01

        @ Does Motor1 have a limit switch wired for safety-stop? If false, its switch is never read.
        param motor1HasLimitSwitch: bool default true

        @ Does Motor2 have a limit switch wired for safety-stop? If false, its switch is never read.
        param motor2HasLimitSwitch: bool default true

        event motorEvent(
            motor: yellowJacket
            ) \
              severity warning low \
              id 0x00 \
              format "{}" 

        @ telemetry for motor state
        telemetry motor1: billeeScm.yellowJacket

        telemetry motor2: billeeScm.yellowJacket

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