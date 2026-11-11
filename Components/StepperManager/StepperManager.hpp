// ======================================================================
// \title  StepperManager.hpp
// \brief  Sample Cache carousel stepper (step / direction / motor-free driver)
// ======================================================================
#ifndef billeeScm_StepperManager_HPP
#define billeeScm_StepperManager_HPP

#include "Components/StepperManager/StepperManagerComponentAc.hpp"

namespace billeeScm {

class StepperManager final : public StepperManagerComponentBase {
  public:
    static constexpr U32 MAX_SPEED_HZ = 5000;    //!< Fastest step rate accepted
    static constexpr U32 MAX_STEPS = 1000000;    //!< Longest single move
    static constexpr F32 START_SPEED_HZ = 200;   //!< Ramps start and end at this rate (or the cruise rate if lower)

    StepperManager(const char* const compName);
    ~StepperManager();

    //! Bind to the driver's PU- (step), DR- (direction) and MF- (motor free) pins, set the ramp acceleration
    //! (steps/s^2) and whether FORWARD needs DR- on (invertDirection). Energizes the motor. Call once during topology
    //! setup. Only one StepperManager can exist (the step timer interrupt is static).
    void configure(U8 stepPin, U8 dirPin, U8 freePin, F32 accelStepsPerS2, bool invertDirection);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;
    void MOVE_STEPS_cmdHandler(FwOpcodeType opCode,
                               U32 cmdSeq,
                               billeeScm::MotorDir direction,
                               U32 steps,
                               U32 speedHz) override;
    void MOVE_TO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, I32 position, U32 speedHz) override;
    void STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void HOLD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::On onOff) override;
    void ZERO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Start a move of `steps` (> 0) in a direction. Answers the command itself on refusal; otherwise it is answered
    //! when the move ends.
    void startMove(FwOpcodeType opCode, U32 cmdSeq, bool forward, U32 steps, U32 speedHz);

    //! Stop the pulse timer at once. Returns true if a move was running.
    bool haltTimer();

    //! Answer the pending move command (no-op if none)
    void finishMove(Fw::CmdResponse response);

    //! Energize (hold) or free the motor
    void setHold(bool hold);

    void publishTelemetry();

    bool m_configured = false;
    U8 m_stepPin = 0;
    U8 m_dirPin = 0;
    U8 m_freePin = 0;
    bool m_invertDirection = false;
    bool m_holding = false;
    I32 m_target = 0;
    bool m_pending = false;
    FwOpcodeType m_opCode = 0;
    U32 m_cmdSeq = 0;
};

}  // namespace billeeScm

#endif
