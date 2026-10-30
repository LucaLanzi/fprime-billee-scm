module billeeScm {
    @ Servo selection on the PCA9685 board: the value is the board channel. Rename entries to their physical role
    @ once servos are assigned (keep the numeric values).
    enum ServoId: U8 {
        SERVO0 = 0   @< Channel 0
        SERVO1 = 1   @< Channel 1
        SERVO2 = 2   @< Channel 2
        SERVO3 = 3   @< Channel 3
        SERVO4 = 4   @< Channel 4
        SERVO5 = 5   @< Channel 5
        SERVO6 = 6   @< Channel 6
        SERVO7 = 7   @< Channel 7
        SERVO8 = 8   @< Channel 8
        SERVO9 = 9   @< Channel 9
        SERVO10 = 10 @< Channel 10
        SERVO11 = 11 @< Channel 11
    }

    @ Last commanded state of one servo channel
    struct ServoState {
        direction: motorDir @< Commanded direction (STOPPED for neutral, raw PWM or released)
        pwm: U16            @< Commanded magnitude, 0-1000 (0 for neutral, raw PWM or released)
        pulse: U16          @< PCA9685 pulse actually sent, in counts of 4096 (0 = released)
    }

    @ Per-channel servo state, indexed by ServoId. All released (pulse 0) until first commanded.
    array ServoStates = [12] ServoState
}
