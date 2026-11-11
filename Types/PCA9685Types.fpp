module billeeScm {
    @ Number of PCA9685 channels
    constant SERVO_COUNT = 16

    @ Servo on the PCA9685 board, numbered like the block diagram's PCA_S1, PCA_S2, ... The value is the board channel
    @ (S1 = channel 0). Roles per the Science Hardware Architecture 2027 diagram.
    enum ServoId: U8 {
        S1 = 0   @< PCA_S1: soil cup tilting servo (Regolith Extraction Module)
        S2 = 1   @< PCA_S2: drape lowering servo (UV Spectroscopy Module)
        S3 = 2   @< PCA_S3: cache lid servo (Sample Cache)
        S4 = 3   @< PCA_S4: funnel flap servo (Sample Cache)
        S5 = 4   @< Spare
        S6 = 5   @< Spare
        S7 = 6   @< Spare
        S8 = 7   @< Spare
        S9 = 8   @< Spare
        S10 = 9  @< Spare
        S11 = 10 @< Spare
        S12 = 11 @< Spare
        S13 = 12 @< Spare
        S14 = 13 @< Spare
        S15 = 14 @< Spare
        S16 = 15 @< Spare
    }

    @ Last commanded state of one servo
    struct ServoState {
        direction: MotorDir @< Commanded direction (STOPPED for neutral, raw pulse or released)
        speed: U8           @< Commanded speed/travel, 0-100 % (0 for neutral, raw pulse or released)
        pulse: U16          @< PCA9685 pulse actually sent, in counts of 4096 per 20 ms frame (0 = released)
    }

    @ State of every servo, indexed by ServoId. All released (pulse 0) until first commanded.
    array ServoStates = [SERVO_COUNT] ServoState
}
