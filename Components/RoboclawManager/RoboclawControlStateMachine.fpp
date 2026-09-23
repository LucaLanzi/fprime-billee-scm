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
        guard isLimitSwTripped: billeeScm.yellowJacket @< true if this motor's limit switch is tripped and the commanded direction should be blocked

        action motorFwd: billeeScm.yellowJacket  @< forward the motor
        action motorRev:  billeeScm.yellowJacket @< reverse the motor
        action motorStop: billeeScm.yellowJacket @< stop the motor
        action rejectCmd: billeeScm.yellowJacket @< answer a command that arrived while a communication fault is latched

        @ Signals are queued on the component, so a cmdRecv can be dispatched in any state. Every state
        @ that can see one must consume it (run an action that answers the pending command), otherwise
        @ the command would never get a response. init accepts commands that arrive before the first tick.
        state init {
            on tick enter doWait
            on cmdRecv enter CHOOSE_CMD
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

        @ Gate: a tripped, enabled limit switch overrides any FORWARD/REVERSE command with an immediate stop
        choice CHOOSE_CMD {
            if isLimitSwTripped do {motorStop} enter doWait \
                else enter CHOOSE_CMD_FWD
        }

        @ Dispatch a received command to the matching motor action
        choice CHOOSE_CMD_FWD {
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
            on cmdRecv do {rejectCmd} enter checkErr
        }
    }
}
