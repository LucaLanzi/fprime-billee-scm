module billeeScm {
    @ Drives the Sample Cache carousel: a NEMA 17 stepper with an integrated driver taking PU- (step pulse), DR-
    @ (direction) and MF- (motor free) on optocoupled inputs, common XCOM to +5 V. The Teensy pins are open-drain:
    @ an input is "on" when its pin sinks to GND, "off" when the pin is high-impedance.
    @
    @ Step pulses come from a hardware timer with a trapezoidal profile (ramp up, cruise, ramp down). Moves are
    @ answered when the motor arrives, so in a sequence the next command starts after the move. Position is counted
    @ in steps from the ZERO reference (no home switch on the carousel).
    active component StepperManager {

        @ Rate group tick: completes finished moves, publishes telemetry. Drops the tick if the queue is full.
        async input port run: Svc.Sched drop

        @ Move a number of steps. Answered OK on arrival; EXECUTION_ERROR if STOP or another move cuts it short.
        @ steps 1-1000000, speedHz 1-MAX_SPEED_HZ (5000) steps per second. Energizes the motor if it was free.
        async command MOVE_STEPS(
            direction: billeeScm.MotorDir @< FORWARD (position counts up) or REVERSE
            steps: U32                    @< Steps to move
            speedHz: U32                  @< Cruise speed, steps per second
        ) opcode 0x00

        @ Move to an absolute position (steps from ZERO). Answered OK on arrival (at once if already there).
        async command MOVE_TO(
            position: I32 @< Target, steps from the ZERO reference
            speedHz: U32  @< Cruise speed, steps per second
        ) opcode 0x01

        @ Stop at once (no ramp down). A move in progress is answered EXECUTION_ERROR.
        async command STOP() opcode 0x02

        @ ON: energize the motor so it holds position (default at boot). OFF: free it (MF- active), e.g. to turn the
        @ carousel by hand. Refused while moving.
        async command HOLD(
            onOff: Fw.On @< ON = hold, OFF = free
        ) opcode 0x03

        @ Make the current position the 0 reference. Refused while moving.
        async command ZERO() opcode 0x04

        @ A move started
        event MoveStarted(
            fromPosition: I32 @< Start, steps
            toPosition: I32   @< Target, steps
            speedHz: U32      @< Cruise speed
        ) \
          severity activity high \
          id 0x00 \
          format "Carousel moving {} -> {} at {} steps/s"

        @ A move arrived
        event MoveDone(
            position: I32 @< Final position, steps
        ) \
          severity activity high \
          id 0x01 \
          format "Carousel at {}"

        @ A move was cut short (STOP or a new move)
        event MoveStopped(
            position: I32 @< Where it stopped, steps
        ) \
          severity warning low \
          id 0x02 \
          format "Carousel move stopped at {}"

        @ State of the carousel stepper
        telemetry carousel: billeeScm.StepperState

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
