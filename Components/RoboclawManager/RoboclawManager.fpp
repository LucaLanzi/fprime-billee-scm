module billeeScm {
    @ Component capable of driving two seperate motors over Serial.
    active component RoboclawManager {

        state machine instance motorControlSM: MotorControlStateMachine

        @ Input port for run handler
        async input port run: Svc.Sched 

        @ Output port to get the value of the limit switch (index 0 = Motor1, index 1 = Motor2)
        output port limitSwGet: [2] Drv.GpioRead

        @ Drive one motor. motor.speed is the Roboclaw's raw duty value, 0-127 (0 = stop, 127 = full speed),
        @ NOT 0-255 and NOT scaled: 128-255 is rejected with VALIDATION_ERROR and nothing is sent to the
        @ Roboclaw. motor.motorNum is 1 or 2; motor.motorDir is FORWARD, REVERSE or STOPPED (STOPPED ignores
        @ speed); motor.motorState is ignored on input.
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

        @ STOPPED/0/OFF (assumed, not read back from the Roboclaw) until the first command.
        telemetry motor1: billeeScm.yellowJacket

        @ STOPPED/0/OFF (assumed, not read back from the Roboclaw) until the first command.
        telemetry motor2: billeeScm.yellowJacket

        @ Motor1 limit switch: true = tripped. False if the switch is not wired (param disabled) or the GPIO read failed.
        telemetry motor1LimitSwitch: bool

        @ Motor2 limit switch: true = tripped. False if the switch is not wired (param disabled) or the GPIO read failed.
        telemetry motor2LimitSwitch: bool

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