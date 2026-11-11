module billeeScm {
    @ Drives the PCA9685 16-channel PWM board over I2C: up to 16 servos, S1-S16 (ServoId, numbered like the block
    @ diagram's PCA_S1...). Positional and continuous-rotation servos are driven the same way: a direction and a speed
    @ (0-100 %) around the servo's calibrated neutral pulse. Each channel's min/neutral/max pulse and a `reversed` flag
    @ are a compile-time table in PCA9685Manager.cpp; no command can send a pulse outside those limits.
    active component PCA9685Manager {

        @ Rate group tick: publishes telemetry (no I2C). Drops the tick if the queue is full.
        async input port run: Svc.Sched drop

        @ Drive one servo. speed is 0-100 % of the servo's calibrated travel (positional) or speed (continuous) in
        @ the given direction, measured from its neutral pulse. STOPPED, or speed 0, holds the neutral pulse
        @ (continuous servo stops, positional servo centres). speed > 100 is rejected (VALIDATION_ERROR).
        async command SERVO_SET(
            servo: billeeScm.ServoId      @< Servo to drive
            direction: billeeScm.MotorDir @< FORWARD, REVERSE or STOPPED
            speed: U8                     @< 0-100 %
        ) opcode 0x00

        @ Stop pulsing one servo: it goes limp (no holding torque). This is "off".
        async command SERVO_OFF(
            servo: billeeScm.ServoId @< Servo to release
        ) opcode 0x01

        @ Raw pulse for bring-up and calibration. 0 releases the servo; any other value must lie inside that
        @ channel's calibrated [min,max] pulse limits, otherwise VALIDATION_ERROR.
        async command SERVO_PULSE(
            servo: billeeScm.ServoId @< Servo to drive
            counts: U16              @< 0 = release, else pulse in counts of 4096 per 20 ms frame
        ) opcode 0x02

        @ Release every servo at once (all go limp). This is the software stop: the PCA9685's OE pin is not wired.
        @ Always tries the bus, even if initialization failed.
        async command STOP_ALL() opcode 0x03

        @ Re-run PCA9685 initialization and release every servo. Use after powering the board after the Teensy.
        async command REINIT() opcode 0x04

        @ Last commanded state of every servo (direction, speed, pulse sent), indexed by ServoId
        telemetry servoStates: billeeScm.ServoStates

        @ Number of I2C transactions that reported an error since boot
        telemetry i2cErrorCount: U32

        @ Wire error code of the most recent transaction: 0 ok, 1 too long, 2 address NACK, 3 data NACK, 4 other/short read, 5 timeout
        telemetry lastI2cError: U8

        @ An I2C transaction to the PCA9685 failed. Logged only when the error code changes, so a dead bus cannot
        @ flood the event queue.
        event I2cFault(
            channel: U8 @< Board channel being written (255 = init / all channels)
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
    }
}
