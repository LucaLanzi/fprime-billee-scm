module billeeScm {
    @ Direction of a motor or servo command, paired with a speed of 0-100 (% of the actuator's calibrated full
    @ scale in that direction). Roboclaw motors: FORWARD always moves toward the motor's UPPER limit switch, REVERSE
    @ toward its LOWER one (per-motor wiring is corrected in the topology). Servos: FORWARD/REVERSE move away from
    @ the servo's neutral pulse toward its max/min pulse (a per-channel `reversed` flag swaps them).
    enum MotorDir: U8 {
        STOPPED = 0 @< Stopped / neutral (speed ignored)
        FORWARD = 1 @< Forward (Roboclaw: toward the UPPER limit switch)
        REVERSE = 2 @< Reverse (Roboclaw: toward the LOWER limit switch)
    }
}
