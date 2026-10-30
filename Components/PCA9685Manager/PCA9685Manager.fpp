module billeeScm {
    @ Component to drive a PCA9685 16-channel PWM board over I2C. Controls up to 12 servos.
    @ Every servo, positional or continuous-rotation, is driven the same way: a PWM magnitude and a direction
    @ around its calibrated neutral pulse (setServo). Each channel's min/mid/max pulse limits and a `reversed`
    @ flag are a compile-time table in PCA9685Manager.cpp; no command can send a pulse outside those limits.
    active component PCA9685Manager {

        @ Input port for run handler (publishes telemetry)
        async input port run: Svc.Sched

        @ Drive one servo. pwm is 0-1000 = 0.0-100.0% of the servo's calibrated travel (positional) or speed
        @ (continuous) in the given direction, measured from its neutral pulse. STOPPED, or pwm 0, holds the neutral
        @ pulse (continuous servo stops, positional servo centres). pwm > 1000 is rejected with VALIDATION_ERROR.
        async command setServo(
            servo: billeeScm.ServoId      @< Servo to drive
            pwm: U16                      @< PWM magnitude, 0-1000 (0.1% steps)
            direction: billeeScm.motorDir @< FORWARD, REVERSE or STOPPED
        ) opcode 0x00

        @ Raw PWM for bring-up and calibration. pwm 0 releases the channel; any other value must lie inside
        @ that channel's calibrated [min,max] pulse limits, otherwise VALIDATION_ERROR (4096 "full on" is never allowed).
        async command setChannelPWM(
            servo: billeeScm.ServoId @< Servo to drive
            pwm: U16                 @< 0 = release, else PWM counts (0-4095 scale, 50 Hz)
        ) opcode 0x01

        @ Stop pulsing one channel (the servo goes limp)
        async command releaseChannel(
            servo: billeeScm.ServoId @< Servo to release
        ) opcode 0x02

        @ Hard stop: stop pulsing every channel at once (all servos go limp). This is the software stop: the
        @ PCA9685's OE pin is not wired. Always tries the bus, even if init failed.
        async command hardStop() opcode 0x03

        @ Re-run PCA9685 initialisation and release all channels. Use after powering the board on after the Teensy.
        async command reinit() opcode 0x04

        @ Last commanded state per channel (direction, PWM magnitude, pulse sent), indexed by ServoId
        telemetry servoStates: billeeScm.ServoStates

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
