// ======================================================================
// \title  RoboclawManager.hpp
// \author luca_lanzi
// \brief  hpp file for RoboclawManager component implementation class
// ======================================================================

#ifndef billeeScm_RoboclawManager_HPP
#define billeeScm_RoboclawManager_HPP

#include "Components/RoboclawManager/RoboclawManagerComponentAc.hpp"
#include <RoboClaw.h>
// RoboClaw.h pulls in Arduino's HardwareSerial.h, which #defines HIGH/LOW as plain macros that
// would otherwise clobber Fw::Logic::HIGH/LOW below. FprimeArduino.hpp captures them as
// Arduino::DEF_HIGH/DEF_LOW and #undefs the raw macros -- same fix GpioDriver.cpp uses.
#include <Arduino/config/FprimeArduino.hpp>
#include <new>  // for placement new, see m_roboclawStorage below

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

    //! Bind this instance to its physical Roboclaw board. Call once during topology init,
    //! before the rate group starts ticking this component.
    void configure(HardwareSerial* serial, U8 address, uint32_t baud = 38400);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Input port for run handler
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command motorCmd
    //!
    //! Command to sent to roboclaw
    void motorCmd_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             billeeScm::yellowJacket motor) override;

    //! Handler implementation for command clearError
    void clearError_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq            //!< The command sequence number
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Implementations for internal state machine actions
    // ----------------------------------------------------------------------

    //! Implementation for action motorFwd of state machine billeeScm_MotorControlStateMachine
    //!
    //! forward the motor
    void billeeScm_MotorControlStateMachine_action_motorFwd(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
        ) override;

    //! Implementation for action motorRev of state machine billeeScm_MotorControlStateMachine
    //!
    //! reverse the motor
    void billeeScm_MotorControlStateMachine_action_motorRev(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
        ) override;

    //! Implementation for action motorStop of state machine billeeScm_MotorControlStateMachine
    //!
    //! stop the motor
    void billeeScm_MotorControlStateMachine_action_motorStop(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
        ) override;

    //! Implementation for action rejectCmd of state machine billeeScm_MotorControlStateMachine
    //!
    //! answer a command that arrived while a communication fault is latched
    void billeeScm_MotorControlStateMachine_action_rejectCmd(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
        ) override;

  private:
    // ----------------------------------------------------------------------
    // Implementations for internal state machine guards
    // ----------------------------------------------------------------------

    //! Implementation for guard isFwdCmd of state machine billeeScm_MotorControlStateMachine
    //!
    //! true if the command requests FORWARD
    bool billeeScm_MotorControlStateMachine_guard_isFwdCmd(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
    ) const override;

    //! Implementation for guard isRevCmd of state machine billeeScm_MotorControlStateMachine
    //!
    //! true if the command requests REVERSE
    bool billeeScm_MotorControlStateMachine_guard_isRevCmd(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
    ) const override;

    //! Implementation for guard isStopCmd of state machine billeeScm_MotorControlStateMachine
    //!
    //! true if the command requests STOPPED
    bool billeeScm_MotorControlStateMachine_guard_isStopCmd(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
    ) const override;

    //! Implementation for guard isLimitSwTripped of state machine billeeScm_MotorControlStateMachine
    //!
    //! true if this motor's limit switch is tripped and the commanded direction should be blocked
    bool billeeScm_MotorControlStateMachine_guard_isLimitSwTripped(
        SmId smId,                                          //!< The state machine id
        billeeScm_MotorControlStateMachine::Signal signal,  //!< The signal
        const billeeScm::yellowJacket& value                //!< The value
    ) const override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Update telemetry + event for a motor's actual resulting state (called from each action)
    void reportMotorState(const billeeScm::yellowJacket& motor);

    //! Send the response for the command currently in flight (no-op if none). Called from the state
    //! machine actions: signals are queued on the component, so the Roboclaw call has not happened yet
    //! when motorCmd_cmdHandler returns and the real result is only known inside the action.
    void completePendingCmd(Fw::CmdResponse response);

    //! Roboclaw packet-serial duty range is 0-127; anything larger is rejected before any serial traffic.
    static constexpr U8 MAX_MOTOR_SPEED = 127;

    // Packet-serial driver instance, constructed in configure() via placement new into this
    // static storage. NOT heap-allocated (no plain `new RoboClaw(...)`): this deployment never
    // calls Os::Baremetal::OverrideNewDelete::registerMemAllocator(), so the global `operator
    // new` override's FW_ASSERT(pAllocator != nullptr) would fire on the very first heap
    // allocation anywhere in the firmware and halt the board before it ever produces output --
    // confirmed on real hardware: build succeeds, board boots, zero bytes ever appear on
    // serial. Placement new below constructs into pre-reserved memory without going through
    // the overridden global operator new at all, sidestepping the issue entirely.
    alignas(RoboClaw) uint8_t m_roboclawStorage[sizeof(RoboClaw)];
    RoboClaw* m_roboclaw = nullptr;  //!< Points into m_roboclawStorage once configure() runs
    U8 m_address = 0;                //!< This instance's Roboclaw device address, set in configure()

    // The one motorCmd currently in flight (accepted by motorCmd_cmdHandler, answered by a state
    // machine action). A second motorCmd while this is set is rejected as busy.
    bool m_cmdPending = false;
    FwOpcodeType m_pendingOpCode = 0;
    U32 m_pendingCmdSeq = 0;

    // Last reported state of each motor, republished as telemetry every run_handler cycle so the
    // channels exist from the first tick and stay visible to a GDS that connects later (TlmChan
    // only re-sends a channel when it is written). STOPPED/0/OFF until the first command is an
    // assumption: nothing is read back from, or sent to, the Roboclaw at boot.
    billeeScm::yellowJacket m_motor1State{billeeScm::motorId::MOTOR1, billeeScm::motorDir::STOPPED, 0, Fw::On::OFF};
    billeeScm::yellowJacket m_motor2State{billeeScm::motorId::MOTOR2, billeeScm::motorDir::STOPPED, 0, Fw::On::OFF};

    // Limit-switch state, refreshed once per tick in run_handler: guard implementations are
    // const, but the generated limitSwGet_out port-invoke helper is not, so the switches are
    // polled here and the guard just reads the cached result.
    bool m_motor1SwitchTripped = false;
    bool m_motor2SwitchTripped = false;
};

}  // namespace billeeScm

#endif
