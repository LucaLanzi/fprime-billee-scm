// ======================================================================
// \title  RoboclawManager.hpp
// \author luca_lanzi
// \brief  hpp file for RoboclawManager component implementation class
// ======================================================================

#ifndef billeeScm_RoboclawManager_HPP
#define billeeScm_RoboclawManager_HPP

#include "Components/RoboclawManager/RoboclawManagerComponentAc.hpp"

namespace billeeScm {

class RoboclawManager final : public RoboclawManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct RoboclawManager object
    RoboclawManager(const char* const compName  //!< The component name
    );

    //! Destroy RoboclawManager object
    ~RoboclawManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    void run_handler(FwIndexType portNum,  //!< The port number
                      U32 context          //!< The call order
                      ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command motorCmd
    void motorCmd_cmdHandler(FwOpcodeType opCode,        //!< The opcode
                              U32 cmdSeq,                 //!< The command sequence number
                              billeeScm::yellowJacket motor) override;

    // ----------------------------------------------------------------------
    // Implementations for internal state machine actions
    // ----------------------------------------------------------------------

    //! Implementation for action motorFwd of state machine billeeScm_MotorControlStateMachine
    void billeeScm_MotorControlStateMachine_action_motorFwd(
        SmId smId,
        billeeScm_MotorControlStateMachine::Signal signal) override;

    //! Implementation for action motorRev of state machine billeeScm_MotorControlStateMachine
    void billeeScm_MotorControlStateMachine_action_motorRev(
        SmId smId,
        billeeScm_MotorControlStateMachine::Signal signal) override;

    //! Implementation for action motorStop of state machine billeeScm_MotorControlStateMachine
    void billeeScm_MotorControlStateMachine_action_motorStop(
        SmId smId,
        billeeScm_MotorControlStateMachine::Signal signal) override;

    // ----------------------------------------------------------------------
    // Implementations for internal state machine guards
    // ----------------------------------------------------------------------

    //! Implementation for guard isFwdCmd of state machine billeeScm_MotorControlStateMachine
    bool billeeScm_MotorControlStateMachine_guard_isFwdCmd(
        SmId smId,
        billeeScm_MotorControlStateMachine::Signal signal,
        const billeeScm::yellowJacket& value) const override;

    //! Implementation for guard isRevCmd of state machine billeeScm_MotorControlStateMachine
    bool billeeScm_MotorControlStateMachine_guard_isRevCmd(
        SmId smId,
        billeeScm_MotorControlStateMachine::Signal signal,
        const billeeScm::yellowJacket& value) const override;

    //! Implementation for guard isStopCmd of state machine billeeScm_MotorControlStateMachine
    bool billeeScm_MotorControlStateMachine_guard_isStopCmd(
        SmId smId,
        billeeScm_MotorControlStateMachine::Signal signal,
        const billeeScm::yellowJacket& value) const override;
};

}  // namespace billeeScm

#endif
