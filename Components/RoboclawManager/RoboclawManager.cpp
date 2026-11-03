// ======================================================================
// \title  RoboclawManager.cpp
// \author luca_lanzi
// \brief  cpp file for RoboclawManager component implementation class
// ======================================================================

#include "Components/RoboclawManager/RoboclawManager.hpp"

namespace billeeScm {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RoboclawManager ::RoboclawManager(const char* const compName) : RoboclawManagerComponentBase(compName) {}

RoboclawManager ::~RoboclawManager() {}

void RoboclawManager ::configure(HardwareSerial* serial, U8 address, uint32_t baud) {
    this->m_address = address;
    // Placement new: constructs into m_roboclawStorage (declared in the header), not the heap --
    // see the comment there for why plain `new` hangs this deployment on real hardware.
    this->m_roboclaw = new (this->m_roboclawStorage) RoboClaw(serial, 10000);
    this->m_roboclaw->begin(baud);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void RoboclawManager ::run_handler(FwIndexType portNum, U32 context) {
    Fw::ParamValid valid;

    // A motor with no switch wired in the topology (limitSwGet port left unconnected) is treated the same as
    // motorNHasLimitSwitch = false: invoking an unconnected output port is an FW_ASSERT.
    if (this->isConnected_limitSwGet_OutputPort(0) && this->paramGet_motor1HasLimitSwitch(valid)) {
        Fw::Logic state;
        Drv::GpioStatus status = this->limitSwGet_out(0, state);
        this->m_motor1SwitchTripped = (status == Drv::GpioStatus::OP_OK) && (state == Fw::Logic::LOW);
    } else {
        this->m_motor1SwitchTripped = false;
    }

    if (this->isConnected_limitSwGet_OutputPort(1) && this->paramGet_motor2HasLimitSwitch(valid)) {
        Fw::Logic state;
        Drv::GpioStatus status = this->limitSwGet_out(1, state);
        this->m_motor2SwitchTripped = (status == Drv::GpioStatus::OP_OK) && (state == Fw::Logic::LOW);
    } else {
        this->m_motor2SwitchTripped = false;
    }

    // Publish every cycle (not only on change): gives the channels a value from the first tick and
    // keeps them visible to a GDS that connects mid-run.
    this->tlmWrite_motor1(this->m_motor1State);
    this->tlmWrite_motor2(this->m_motor2State);
    this->tlmWrite_motor1LimitSwitch(this->m_motor1SwitchTripped);
    this->tlmWrite_motor2LimitSwitch(this->m_motor2SwitchTripped);

    this->motorControlSM_sendSignal_tick();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RoboclawManager ::setMotor_cmdHandler(FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            billeeScm::motorId motor,
                                            U16 pwm,
                                            billeeScm::motorDir direction) {
    // Out-of-range magnitude or invalid enum never reaches the serial line
    if (!motor.isValid() || !direction.isValid() || pwm > PWM_FULL_SCALE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (this->motorControlSM_getState() == billeeScm_MotorControlStateMachine::State::checkErr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    // One command in flight at a time, so its response can't be lost or attributed to the wrong command
    if (this->m_cmdPending) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    // The signal is queued: the state machine runs later and the matching action sends the response
    // (see completePendingCmd), so the response reflects this command's real result.
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->m_cmdPending = true;
    this->motorControlSM_sendSignal_cmdRecv(billeeScm::yellowJacket(motor, direction, pwm, Fw::On::OFF));
}

void RoboclawManager ::clearError_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->motorControlSM_sendSignal_errClr();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void RoboclawManager ::hardStop_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Runs here, not through the state machine, so it is never blocked by checkErr or a command in flight.
    // Same thread as the state machine actions (active component), so there is no race with them.
    // One packet stops both motors at once.
    const bool ok = (this->m_roboclaw != nullptr) && this->m_roboclaw->DutyM1M2(this->m_address, 0, 0);
    // A setMotor whose cmdRecv is still queued would otherwise restart its motor right after this stop
    if (this->m_cmdPending) {
        this->completePendingCmd(Fw::CmdResponse::EXECUTION_ERROR);
        this->m_cancelledCmds++;
    }
    if (ok) {
        // One hardStopEvent instead of two motorEvents: eventLogger's queue is shallow and a sequence may
        // hard-stop both Roboclaws back to back.
        this->reportMotorState(
            billeeScm::yellowJacket(billeeScm::motorId::MOTOR1, billeeScm::motorDir::STOPPED, 0, Fw::On::OFF), false);
        this->reportMotorState(
            billeeScm::yellowJacket(billeeScm::motorId::MOTOR2, billeeScm::motorDir::STOPPED, 0, Fw::On::OFF), false);
    }
    this->log_WARNING_HI_hardStopEvent(ok);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

// ----------------------------------------------------------------------
// Implementations for internal state machine actions
// ----------------------------------------------------------------------

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorFwd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    const bool ok = this->sendDuty(value.get_motorNum(), toDuty(value.get_pwm()));
    this->reportMotorState(billeeScm::yellowJacket(value.get_motorNum(), billeeScm::motorDir::FORWARD,
                                                     value.get_pwm(),
                                                     ok ? Fw::On::ON : Fw::On::OFF));
    this->completePendingCmd(ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    ok ? this->motorControlSM_sendSignal_success() : this->motorControlSM_sendSignal_fail();
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorRev(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    const bool ok = this->sendDuty(value.get_motorNum(), static_cast<I16>(-toDuty(value.get_pwm())));
    this->reportMotorState(billeeScm::yellowJacket(value.get_motorNum(), billeeScm::motorDir::REVERSE,
                                                     value.get_pwm(),
                                                     ok ? Fw::On::ON : Fw::On::OFF));
    this->completePendingCmd(ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    ok ? this->motorControlSM_sendSignal_success() : this->motorControlSM_sendSignal_fail();
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorStop(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    const bool ok = this->sendDuty(value.get_motorNum(), 0);
    this->reportMotorState(
        billeeScm::yellowJacket(value.get_motorNum(), billeeScm::motorDir::STOPPED, 0, Fw::On::OFF));
    this->completePendingCmd(ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    ok ? this->motorControlSM_sendSignal_success() : this->motorControlSM_sendSignal_fail();
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_rejectCmd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    // Reached only when a command was accepted by the handler but the fault latched before it ran. If a hardStop
    // cancelled it meanwhile, it was already answered: just consume the cancellation.
    if (this->m_cancelledCmds > 0) {
        this->m_cancelledCmds--;
        return;
    }
    this->completePendingCmd(Fw::CmdResponse::EXECUTION_ERROR);
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_dropCmd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    // hardStop already answered this command (EXECUTION_ERROR); nothing is sent to the Roboclaw
    this->m_cancelledCmds--;
}

// ----------------------------------------------------------------------
// Implementations for internal state machine guards
// ----------------------------------------------------------------------

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isFwdCmd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) const {
    return value.get_motorDir() == billeeScm::motorDir::FORWARD;
}

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isRevCmd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) const {
    return value.get_motorDir() == billeeScm::motorDir::REVERSE;
}

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isStopCmd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) const {
    return value.get_motorDir() == billeeScm::motorDir::STOPPED;
}

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isCancelled(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) const {
    return this->m_cancelledCmds > 0;
}

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isLimitSwTripped(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) const {
    // An explicit STOPPED command is never blocked by the switch
    if (value.get_motorDir() == billeeScm::motorDir::STOPPED) {
        return false;
    }
    return (value.get_motorNum() == billeeScm::motorId::MOTOR1) ? this->m_motor1SwitchTripped
                                                                  : this->m_motor2SwitchTripped;
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void RoboclawManager ::completePendingCmd(Fw::CmdResponse response) {
    if (this->m_cmdPending) {
        this->m_cmdPending = false;
        this->cmdResponse_out(this->m_pendingOpCode, this->m_pendingCmdSeq, response);
    }
}

void RoboclawManager ::reportMotorState(const billeeScm::yellowJacket& motor, bool logEvent) {
    // Cache the state so run_handler keeps republishing it; also write now so a command's result is
    // visible immediately rather than on the next cycle.
    if (motor.get_motorNum() == billeeScm::motorId::MOTOR1) {
        this->m_motor1State = motor;
        this->tlmWrite_motor1(motor);
    } else {
        this->m_motor2State = motor;
        this->tlmWrite_motor2(motor);
    }
    if (logEvent) {
        this->log_WARNING_LO_motorEvent(motor);
    }
}

bool RoboclawManager ::sendDuty(billeeScm::motorId motor, I16 duty) {
    if (this->m_roboclaw == nullptr) {
        return false;
    }
    // The library takes the signed duty as its raw 16-bit word, so a negative duty is passed two's-complement
    const uint16_t word = static_cast<uint16_t>(duty);
    return (motor == billeeScm::motorId::MOTOR1) ? this->m_roboclaw->DutyM1(this->m_address, word)
                                                 : this->m_roboclaw->DutyM2(this->m_address, word);
}

I16 RoboclawManager ::toDuty(U16 pwm) {
    return static_cast<I16>((static_cast<U32>(pwm) * MAX_DUTY + PWM_FULL_SCALE / 2) / PWM_FULL_SCALE);
}

}  // namespace billeeScm
