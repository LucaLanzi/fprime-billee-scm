module billeeScm {
    @ Commanded direction, paired with a PWM magnitude in every motor command (RoboclawManager.setMotor,
    @ PCA9685Manager.setServo). That magnitude is always 0-1000 = 0.0-100.0% of the motor's calibrated full scale
    @ in this direction (PWM_FULL_SCALE in each component's .hpp). Which physical way FORWARD turns is set by
    @ wiring (Roboclaw) or by the servo's calibration `reversed` flag (PCA9685).
    enum motorDir {
        STOPPED = 1 @< Stopped / neutral (PWM magnitude ignored)
        FORWARD = 2 @< Drive forward
        REVERSE = 3 @< Drive in reverse
    }
}
