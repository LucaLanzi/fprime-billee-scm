module billeeScm {
    enum motorId: U8 {
        MOTOR1 = 1 @< Drill Module Motor 1
        MOTOR2 = 2 @< Drill Module Motor 2
    }

    enum switchId: U8{
        SWITCH1 = 1 @< Limit switch corresponding to Motor 1
        SWITCH2 = 2 @< Limit switch corresponding to Motor 2
    }

    struct yellowJacket {
        motorNum: motorId        @< Define motor you are controlling (1 or 2)
        motorDir: motorDir       @< Motor direction (arbitrarily set by initial wiring)
        pwm: U16                 @< PWM magnitude, 0-1000 (0.1% steps of full duty)
        motorState: Fw.On        @< Motor on/off state 
    } 
}
