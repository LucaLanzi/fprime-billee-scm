module billeeScm {
    @ Number of Roboclaw boards
    constant ROBOCLAW_COUNT = 3

    @ Number of motors driven by the Roboclaws
    constant ROBOCLAW_MOTOR_COUNT = 5

    @ Limit switch inputs: a lower and an upper one per motor (index = 2 * RoboclawMotor + 0 lower / 1 upper)
    constant ROBOCLAW_LIMIT_PORTS = 2 * ROBOCLAW_MOTOR_COUNT

    @ Roboclaw 2x7A boards (Science Hardware Architecture 2027). Serial port and packet-serial address are set in the
    @ topology (billee_deploymentTopology.cpp).
    enum RoboclawId: U8 {
        R1 = 0 @< Roboclaw 1: Serial1 (RX 0 / TX 1), address 0x80
        R2 = 1 @< Roboclaw 2: Serial2 (RX 7 / TX 8), address 0x81
        R3 = 2 @< Roboclaw 3: Serial4 (RX 16 / TX 17), address 0x82
    }

    @ Motors driven by the Roboclaws. FORWARD moves toward the motor's UPPER limit switch, REVERSE toward its LOWER one.
    @ Limit switch pins are in SciencePins.hpp.
    enum RoboclawMotor: U8 {
        R1_M1 = 0 @< Roboclaw 1 M1: soil drill actuator (YellowJacket 5203). Lower T_GPIO_23, upper T_GPIO_22
        R1_M2 = 1 @< Roboclaw 1 M2: transfer tube actuator, drill up/down (YellowJacket 5203). Lower T_GPIO_21, upper T_GPIO_15
        R2_M1 = 2 @< Roboclaw 2 M1: transfer tube lead screw (YellowJacket 5203). Lower T_GPIO_14 (lead screw), upper T_GPIO_41 (drill tube to ground contact)
        R2_M2 = 3 @< Roboclaw 2 M2: drill (YellowJacket 5203). No limit switches
        R3_M1 = 4 @< Roboclaw 3 M1: sample rotation linear actuator. Lower T_GPIO_33, upper T_GPIO_32
    }

    @ What a motor is doing
    enum MotorMode: U8 {
        IDLE = 0     @< Stopped
        MOVE = 1     @< Running until STOP, a limit switch or another command
        RUN_FOR = 2  @< Running for a set time
        TO_LIMIT = 3 @< Running until the limit switch in its direction trips
    }

    @ Why a motor stopped
    enum StopReason: U8 {
        COMMANDED = 0    @< STOP, STOP_ALL, or a new command on the motor
        LIMIT_SWITCH = 1 @< The limit switch in its direction tripped
        TIME_UP = 2      @< RUN_FOR finished
        TIMEOUT = 3      @< TO_LIMIT did not reach its switch in time
    }

    @ State of one Roboclaw motor
    struct RoboclawMotorState {
        mode: MotorMode     @< What the motor is doing
        direction: MotorDir @< Commanded direction
        speed: U8           @< Commanded speed, 0-100 % duty
        lowerLimit: bool    @< Lower limit switch tripped (false if none wired)
        upperLimit: bool    @< Upper limit switch tripped (false if none wired)
        encoder: I32        @< Encoder count (read from the Roboclaw; 0 while its board is offline)
        currentMa: I32      @< Motor current in mA (read from the Roboclaw)
    }

    @ State of every motor, indexed by RoboclawMotor
    array RoboclawMotorStates = [ROBOCLAW_MOTOR_COUNT] RoboclawMotorState

    @ State of one Roboclaw board
    struct RoboclawBoardState {
        online: bool   @< Answered its most recent transaction
        batteryDv: U16 @< Main battery voltage in 0.1 V
        errorBits: U32 @< Roboclaw error/status bits (ReadError)
    }

    @ State of every board, indexed by RoboclawId
    array RoboclawBoardStates = [ROBOCLAW_COUNT] RoboclawBoardState
}
