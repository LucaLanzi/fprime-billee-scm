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

    if (this->paramGet_motor1HasLimitSwitch(valid)) {
        Fw::Logic state;
        Drv::GpioStatus status = this->limitSwGet_out(0, state);
        this->m_motor1SwitchTripped = (status == Drv::GpioStatus::OP_OK) && (state == Fw::Logic::LOW);
    } else {
        this->m_motor1SwitchTripped = false;
    }

    if (this->paramGet_motor2HasLimitSwitch(valid)) {
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

void RoboclawManager ::motorCmd_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::yellowJacket motor) {
    // Out-of-range speed never reaches the serial line
    if (motor.get_speed() > MAX_MOTOR_SPEED) {
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
    this->motorControlSM_sendSignal_cmdRecv(motor);
}

void RoboclawManager ::clearError_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->motorControlSM_sendSignal_errClr();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Implementations for internal state machine actions
// ----------------------------------------------------------------------

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorFwd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    bool ok = (this->m_roboclaw != nullptr) &&
              ((value.get_motorNum() == billeeScm::motorId::MOTOR1)
                   ? this->m_roboclaw->ForwardM1(this->m_address, value.get_speed())
                   : this->m_roboclaw->ForwardM2(this->m_address, value.get_speed()));
    this->reportMotorState(billeeScm::yellowJacket(value.get_motorNum(), billeeScm::motorDir::FORWARD,
                                                     value.get_speed(),
                                                     ok ? Fw::On::ON : Fw::On::OFF));
    this->completePendingCmd(ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    ok ? this->motorControlSM_sendSignal_success() : this->motorControlSM_sendSignal_fail();
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorRev(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    bool ok = (this->m_roboclaw != nullptr) &&
              ((value.get_motorNum() == billeeScm::motorId::MOTOR1)
                   ? this->m_roboclaw->BackwardM1(this->m_address, value.get_speed())
                   : this->m_roboclaw->BackwardM2(this->m_address, value.get_speed()));
    this->reportMotorState(billeeScm::yellowJacket(value.get_motorNum(), billeeScm::motorDir::REVERSE,
                                                     value.get_speed(),
                                                     ok ? Fw::On::ON : Fw::On::OFF));
    this->completePendingCmd(ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    ok ? this->motorControlSM_sendSignal_success() : this->motorControlSM_sendSignal_fail();
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorStop(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    bool ok = (this->m_roboclaw != nullptr) &&
              ((value.get_motorNum() == billeeScm::motorId::MOTOR1)
                   ? this->m_roboclaw->ForwardM1(this->m_address, 0)
                   : this->m_roboclaw->ForwardM2(this->m_address, 0));
    this->reportMotorState(
        billeeScm::yellowJacket(value.get_motorNum(), billeeScm::motorDir::STOPPED, 0, Fw::On::OFF));
    this->completePendingCmd(ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    ok ? this->motorControlSM_sendSignal_success() : this->motorControlSM_sendSignal_fail();
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_rejectCmd(
    SmId smId,
    billeeScm_MotorControlStateMachine::Signal signal,
    const billeeScm::yellowJacket& value) {
    // Reached only when a command was accepted by the handler but the fault latched before it ran
    this->completePendingCmd(Fw::CmdResponse::EXECUTION_ERROR);
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

void RoboclawManager ::reportMotorState(const billeeScm::yellowJacket& motor) {
    // Cache the state so run_handler keeps republishing it; also write now so a command's result is
    // visible immediately rather than on the next cycle.
    if (motor.get_motorNum() == billeeScm::motorId::MOTOR1) {
        this->m_motor1State = motor;
        this->tlmWrite_motor1(motor);
    } else {
        this->m_motor2State = motor;
        this->tlmWrite_motor2(motor);
    }
    this->log_WARNING_LO_motorEvent(motor);
}

}  // namespace billeeScm
