module billeeScm {
    state machine MotorControlStateMachine {

        initial enter init

        signal tick    @< Tick signal driven by rate group
        signal success @< success signal
        signal fail    @< failure signal
        signal errClr  @< clears a latched fault
        signal cmdRecv: billeeScm.yellowJacket @< command received

        guard isFwdCmd: billeeScm.yellowJacket  @< true if the command requests FORWARD
        guard isRevCmd: billeeScm.yellowJacket  @< true if the command requests REVERSE
        guard isStopCmd: billeeScm.yellowJacket @< true if the command requests STOPPED

        action motorFwd  @< forward the motor
        action motorRev  @< reverse the motor
        action motorStop @< stop the motor

        state init {
            on tick enter doWait
        }

        state doWait {
            on tick enter doWait
            on cmdRecv enter CHOOSE_CMD
        }

        state doCmd {
            on cmdRecv enter CHOOSE_CMD
            on success enter doWait
            on fail enter checkErr
        }

        @ Dispatch a received command to the matching motor action
        choice CHOOSE_CMD {
            if isFwdCmd do {motorFwd} enter doCmd \
                else enter CHOOSE_CMD_REV
        }

        choice CHOOSE_CMD_REV {
            if isRevCmd do {motorRev} enter doCmd \
                else enter CHOOSE_CMD_STOP
        }

        choice CHOOSE_CMD_STOP {
            if isStopCmd do {motorStop} enter doCmd \
                else enter doCmd
        }

        state checkErr {
            on fail enter checkErr
            on errClr enter doWait
        }
    }
}
