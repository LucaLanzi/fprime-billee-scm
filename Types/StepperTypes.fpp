module billeeScm {
    @ State of the carousel stepper
    struct StepperState {
        moving: bool   @< A move is in progress
        holding: bool  @< Motor energized (holds position); false = free (MF- active)
        position: I32  @< Steps from the ZERO reference (FORWARD counts up)
        target: I32    @< Where the current / last move ends
        speedHz: U32   @< Current step rate (0 when stopped)
    }
}
