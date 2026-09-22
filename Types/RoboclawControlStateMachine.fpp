module billeScm {
    state machine MotorControlStateMachine {
        
        @ Enter init
        initial enter init

        ############################

        signal tick    <@ Tick signal driven by rate group
        signal success <@ success signal
        signal fail    <@ failure signal

        signal cmdRecv <@ command received


        action motorFwd  @< forward the motor
        action motorRev  @< reverse the motor
        action motorStop @< Stop the motor

        ############################

        state init {
            on tick enter doWait
        }

        state doWait {
            on tick enter doWait
            on cmdRecv enter doCmd
        }

        state doCmd {
            on cmdFwd do {motorFwd}
            on cmdRev do {motorRev}
            on cmdStop do {motorStop}
            on success enter doWait
            on fail enter checkErr        
        }

        state checkErr {
            on fail enter checkErr
            on errClr enter doWait
        }

}