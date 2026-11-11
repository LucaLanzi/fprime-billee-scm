module billeeScm {
    @ Drives every Roboclaw motor on the Science Control Module: three Roboclaw 2x7A boards in packet-serial mode
    @ (RoboclawId) and the five motors they drive (RoboclawMotor). Each motor has an optional lower and upper limit
    @ switch; FORWARD always moves toward the upper one, REVERSE toward the lower one (a motor wired the other way is
    @ fixed with configureMotor(..., invert)). A motor moving toward a tripped switch is stopped on the next tick,
    @ whatever command started it.
    @
    @ Commands that wait on the hardware (RUN_FOR, TO_LIMIT) are answered only when the motion ends, so in a sequence
    @ the next command starts after the motion.
    active component RoboclawManager {

        @ Rate group tick (50 Hz): checks limit switches and timers, polls one Roboclaw telemetry item, publishes
        @ telemetry every 5th tick. Drops the tick if the queue is full.
        async input port run: Svc.Sched drop

        @ Limit switch reads, index = 2 * RoboclawMotor + 0 (lower) / 1 (upper). A switch is tripped when it reads LOW
        @ (closes to GND). Leave a port unconnected where a motor has no switch.
        output port limitSwGet: [ROBOCLAW_LIMIT_PORTS] Drv.GpioRead

        @ Run a motor until STOP, its limit switch in that direction, or another command on it. Answered once the
        @ Roboclaw acknowledges. direction STOPPED stops the motor. Refused (EXECUTION_ERROR, LimitBlocked) if the
        @ switch in that direction is already tripped.
        async command MOVE(
            motor: billeeScm.RoboclawMotor @< Motor
            direction: billeeScm.MotorDir  @< FORWARD (toward upper switch), REVERSE (toward lower) or STOPPED
            speed: U8                      @< 0-100 % duty
        ) opcode 0x00

        @ Run a motor for durationMs, then stop it. Answered OK when the time is up; EXECUTION_ERROR if a limit
        @ switch stops it first, the command is superseded, or the board does not answer. 1-300000 ms.
        async command RUN_FOR(
            motor: billeeScm.RoboclawMotor @< Motor
            direction: billeeScm.MotorDir  @< FORWARD or REVERSE
            speed: U8                      @< 0-100 % duty
            durationMs: U32                @< Run time in milliseconds
        ) opcode 0x01

        @ Run a motor until the limit switch in its direction trips (answered OK), or timeoutMs passes (stopped,
        @ EXECUTION_ERROR). Answered OK at once if that switch is already tripped. VALIDATION_ERROR if the motor has
        @ no switch in that direction. 1-300000 ms.
        async command TO_LIMIT(
            motor: billeeScm.RoboclawMotor @< Motor
            direction: billeeScm.MotorDir  @< FORWARD (to upper switch) or REVERSE (to lower switch)
            speed: U8                      @< 0-100 % duty
            timeoutMs: U32                 @< Give up and stop after this many milliseconds
        ) opcode 0x02

        @ Stop one motor. A RUN_FOR / TO_LIMIT running on it is answered EXECUTION_ERROR.
        async command STOP(
            motor: billeeScm.RoboclawMotor @< Motor
        ) opcode 0x03

        @ Stop every motor on every board (one packet per board). Every running RUN_FOR / TO_LIMIT is answered
        @ EXECUTION_ERROR. Fails only if a board that may have a motor running does not acknowledge.
        async command STOP_ALL() opcode 0x04

        @ Zero both encoder counts of one board
        async command RESET_ENCODERS(
            roboclaw: billeeScm.RoboclawId @< Board
        ) opcode 0x05

        @ A motor started
        event MotorStarted(
            motor: billeeScm.RoboclawMotor @< Motor
            mode: billeeScm.MotorMode      @< MOVE, RUN_FOR or TO_LIMIT
            direction: billeeScm.MotorDir  @< Direction
            speed: U8                      @< % duty
        ) \
          severity activity high \
          id 0x00 \
          format "{} started: {} {} at {}%"

        @ A motor stopped
        event MotorStopped(
            motor: billeeScm.RoboclawMotor @< Motor
            reason: billeeScm.StopReason   @< Why
        ) \
          severity activity high \
          id 0x01 \
          format "{} stopped: {}"

        @ A command was refused because the limit switch in its direction is already tripped
        event LimitBlocked(
            motor: billeeScm.RoboclawMotor @< Motor
            direction: billeeScm.MotorDir  @< Refused direction
        ) \
          severity warning low \
          id 0x02 \
          format "{} {} refused: limit switch in that direction is tripped"

        @ A board answered after being offline (or for the first time)
        event BoardOnline(
            roboclaw: billeeScm.RoboclawId @< Board
        ) \
          severity activity high \
          id 0x03 \
          format "Roboclaw {} online"

        @ A board stopped answering. It is probed every 2 s until it answers again.
        event BoardOffline(
            roboclaw: billeeScm.RoboclawId @< Board
        ) \
          severity warning high \
          id 0x04 \
          format "Roboclaw {} not answering"

        @ State of every motor (mode, command, limit switches, encoder, current)
        telemetry motorStates: billeeScm.RoboclawMotorStates

        @ State of every board (online, battery voltage, error bits)
        telemetry boardStates: billeeScm.RoboclawBoardStates

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
