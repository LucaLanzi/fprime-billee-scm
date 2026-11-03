// ======================================================================
// \title  StirrerManager.cpp
// \author luca_lanzi
// \brief  cpp file for StirrerManager component implementation class
// ======================================================================

#include "Components/StirrerManager/StirrerManager.hpp"

namespace billeeScm {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StirrerManager ::StirrerManager(const char* const compName) : StirrerManagerComponentBase(compName) {}

StirrerManager ::~StirrerManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void StirrerManager ::run_handler(FwIndexType portNum, U32 context) {
    // Re-drive each stirrer relay's pin from its tracked state (set by stirrerToggle) on every
    // rate-group tick, same as PumpManager does for the pumps.
    const Fw::Logic stirrer1Level =
        (this->m_stirrer1.get_stirrerState() == Fw::Enabled::ENABLED) ? Fw::Logic::HIGH : Fw::Logic::LOW;
    const Fw::Logic stirrer2Level =
        (this->m_stirrer2.get_stirrerState() == Fw::Enabled::ENABLED) ? Fw::Logic::HIGH : Fw::Logic::LOW;

    this->stirrer1Set_out(0, stirrer1Level);
    this->stirrer2Set_out(0, stirrer2Level);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StirrerManager ::stirrerToggle_cmdHandler(FwOpcodeType opCode,
                                               U32 cmdSeq,
                                               Fw::On toggle,
                                               billeeScm::stirrerId stirrer) {
    // Fw::On (the command argument) -> Fw::Enabled (the type used by the struct, telemetry and event)
    const Fw::Enabled newState = (toggle == Fw::On::ON) ? Fw::Enabled::ENABLED : Fw::Enabled::DISABLED;

    switch (stirrer.e) {
        case billeeScm::stirrerId::STIRRER1:
            this->m_stirrer1.set_stirrerState(newState);
            this->tlmWrite_stirrer1State(newState);
            break;
        case billeeScm::stirrerId::STIRRER2:
            this->m_stirrer2.set_stirrerState(newState);
            this->tlmWrite_stirrer2State(newState);
            break;
        default:
            // Not one of the 2 known stirrers: reject without touching any state
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
            return;
    }

    this->log_ACTIVITY_HI_stirrerState(stirrer, newState);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
