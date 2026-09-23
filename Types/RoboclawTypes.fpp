module billeeScm {
    enum motorId: U8 {
        MOTOR1 = 1 @< Drill Module Motor 1
        MOTOR2 = 2 @< Drill Module Motor 2
    }

    enum switchId: U8{
        SWITCH1 = 1 @< Limit switch corresponding to Motor 1
        SWITCH2 = 2 @< Limit switch corresponding to Motor 2
    }

    enum motorDir {
        STOPPED = 1 @< Drill Direction Forward
        FORWARD = 2 @< Drill Direction Forward
        REVERSE = 3 @< Drill Direction Forward
    }

    struct yellowJacket {
        motorNum: motorId        @< Define motor you are controlling (1 or 2)
        motorDir: motorDir       @< Motor direction (arbitrarily set by initial wiring)
        speed: U8                @< Motor speed magnitude, 0-127 (Roboclaw PWM duty range)
        motorState: Fw.On        @< Motor on/off state 
    } 
}