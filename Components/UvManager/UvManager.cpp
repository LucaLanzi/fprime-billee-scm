// ======================================================================
// \title  UvManager.cpp
// \author root
// \brief  cpp file for UvManager component implementation class
// ======================================================================

#include "Components/UvManager/UvManager.hpp"

namespace billeeScm {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UvManager ::UvManager(const char* const compName) : UvManagerComponentBase(compName) {}

UvManager ::~UvManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void UvManager ::run_handler(FwIndexType portNum, U32 context) {
    // no op
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void UvManager ::uvToggle_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::On toggle) {
    Fw::Logic logicLvl = toggle == Fw::On::ON ? Fw::Logic::HIGH : Fw::Logic::LOW;
    Fw::Enabled uvEnabled = toggle == Fw::On::ON ? Fw::Enabled::ENABLED : Fw::Enabled::DISABLED;

    this->uvSet_out(0, logicLvl);
    this->tlmWrite_uvState(uvEnabled);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
