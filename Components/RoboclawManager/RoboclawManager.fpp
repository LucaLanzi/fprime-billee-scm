module billeeScm {
    @ Component capable of driving two seperate motors over Serial.
    active component RoboclawManager {

        state machine instance motorControlSM: MotorControlStateMachine

        @ Input port for run handler
        async input port run: Svc.Sched 

        @ Output port to get the value of the limit switch (index 0 = Motor1, index 1 = Motor2)
        output port limitSwGet: [2] Drv.GpioRead

        @ Drive one motor. pwm is 0-1000 = 0.0-100.0% duty (0 = stop, 1000 = full duty), sent to the Roboclaw as a
        @ signed 16-bit duty (+/-32767). pwm > 1000 is rejected with VALIDATION_ERROR and nothing is sent.
        @ direction STOPPED ignores pwm.
        async command setMotor(
            motor: billeeScm.motorId      @< MOTOR1 or MOTOR2
            pwm: U16                      @< PWM magnitude, 0-1000 (0.1% steps)
            direction: billeeScm.motorDir @< FORWARD, REVERSE or STOPPED
            ) opcode 0x00

        @ Clears a latched communication-fault (checkErr) state, allowing further setMotor commands
        async command clearError() opcode 0x01

        @ Hard stop: immediately sets both motors to duty 0 in one packet, cancelling any setMotor still in flight
        @ (it is answered EXECUTION_ERROR and never runs). Accepted in every state, including a latched fault.
        async command hardStop() opcode 0x02

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

        @ A hardStop was executed. ok = false if the Roboclaw did not acknowledge the stop packet.
        event hardStopEvent(
            ok: bool @< Roboclaw acknowledged the stop
            ) \
              severity warning high \
              id 0x01 \
              format "Hard stop sent to both motors, acknowledged: {}"

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