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

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void RoboclawManager ::run_handler(FwIndexType portNum, U32 context) {
    this->motorControlSM_sendSignal_tick();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RoboclawManager ::motorCmd_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::yellowJacket motor) {
    this->motorControlSM_sendSignal_cmdRecv(motor);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Implementations for internal state machine actions
// ----------------------------------------------------------------------

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorFwd(SmId smId,
                                                                           billeeScm_MotorControlStateMachine::Signal signal) {
    // TODO: drive the Roboclaw motor forward
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorRev(SmId smId,
                                                                           billeeScm_MotorControlStateMachine::Signal signal) {
    // TODO: drive the Roboclaw motor in reverse
}

void RoboclawManager ::billeeScm_MotorControlStateMachine_action_motorStop(SmId smId,
                                                                            billeeScm_MotorControlStateMachine::Signal signal) {
    // TODO: stop the Roboclaw motor
}

// ----------------------------------------------------------------------
// Implementations for internal state machine guards
// ----------------------------------------------------------------------

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isFwdCmd(SmId smId,
                                                                          billeeScm_MotorControlStateMachine::Signal signal,
                                                                          const billeeScm::yellowJacket& value) const {
    return value.get_motorDir() == billeeScm::motorDir::FORWARD;
}

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isRevCmd(SmId smId,
                                                                          billeeScm_MotorControlStateMachine::Signal signal,
                                                                          const billeeScm::yellowJacket& value) const {
    return value.get_motorDir() == billeeScm::motorDir::REVERSE;
}

bool RoboclawManager ::billeeScm_MotorControlStateMachine_guard_isStopCmd(SmId smId,
                                                                           billeeScm_MotorControlStateMachine::Signal signal,
                                                                           const billeeScm::yellowJacket& value) const {
    return value.get_motorDir() == billeeScm::motorDir::STOPPED;
}

}  // namespace billeeScm
