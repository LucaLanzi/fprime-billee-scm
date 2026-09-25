module billeeScm {
    @ Component to drive a PCA9685 16-channel PWM board over I2C. Controls up to 12 servos.
    @ Each channel is either POSITIONAL (setServoAngle) or CONTINUOUS (setServoSpeed); which one is a
    @ compile-time table in PCA9685Manager.cpp (all POSITIONAL by default), together with each channel's
    @ min/mid/max pulse limits. A command that would send a pulse outside those limits is rejected with
    @ VALIDATION_ERROR and nothing is sent on the bus.
    active component PCA9685Manager {

        @ Input port for run handler (publishes telemetry)
        async input port run: Svc.Sched

        @ Move one POSITIONAL servo. channel is 0-11; angle is -90..+90 degrees (0 = calibrated center).
        @ Rejected with VALIDATION_ERROR if the channel is CONTINUOUS.
        async command setServoAngle(
            channel: U8 @< Servo channel, 0-11
            angle: F32  @< Angle in degrees, -90.0..+90.0
        ) opcode 0x00

        @ Drive one CONTINUOUS-rotation servo. channel is 0-11; speed is -1.0..+1.0 (0 = stop).
        @ Rejected with VALIDATION_ERROR if the channel is POSITIONAL.
        async command setServoSpeed(
            channel: U8 @< Servo channel, 0-11
            speed: F32  @< Speed, -1.0..+1.0
        ) opcode 0x01

        @ Raw PWM for bring-up and calibration. pwm 0 releases the channel; any other value must lie inside
        @ that channel's calibrated [min,max] pulse limits, otherwise VALIDATION_ERROR (4096 "full on" is never allowed).
        async command setChannelPWM(
            channel: U8 @< Channel, 0-11
            pwm: U16    @< 0 = release, else PWM counts (0-4095 scale, 50 Hz)
        ) opcode 0x02

        @ Stop pulsing one channel (the servo goes limp)
        async command releaseChannel(
            channel: U8 @< Channel, 0-11
        ) opcode 0x03

        @ Stop pulsing every channel. This is the software stop: the PCA9685's OE pin is not wired.
        async command releaseAll() opcode 0x04

        @ Re-run PCA9685 initialisation and release all channels. Use after powering the board on after the Teensy.
        async command reinit() opcode 0x05

        @ Last commanded value per channel: degrees for POSITIONAL, speed for CONTINUOUS, 0 if released
        telemetry servoAngles: billeeScm.ServoAngles

        @ Number of I2C transactions that reported an error since boot
        telemetry i2cErrorCount: U32

        @ Wire error code of the most recent transaction: 0 ok, 1 too long, 2 address NACK, 3 data NACK, 4 other/short read, 5 timeout
        telemetry lastI2cError: U8

        @ An I2C transaction to the PCA9685 failed. Deliberately no per-servo-move event: eventLogger has a
        @ shallow queue, so command floods must not each emit an event.
        event I2cFault(
            channel: U8 @< Channel being written (255 = init / all channels)
            code: U8    @< Wire error code, see lastI2cError
        ) \
          severity warning high \
          id 0x00 \
          format "PCA9685 I2C fault on channel {}: code {}"

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
