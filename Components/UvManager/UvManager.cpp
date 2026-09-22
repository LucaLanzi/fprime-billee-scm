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
    Fw::Enabled relayState;
    Fw::Logic logicLvl = toggle == Fw::On::ON ? Fw::Logic::HIGH : Fw::Logic::LOW;

    Drv::GpioStatus writeOk = this->uvSet_out(0, logicLvl);

    if(writeOk == Drv::GpioStatus::OP_OK){
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        relayState = toggle == Fw::On::ON ? Fw::Enabled::ENABLED : Fw::Enabled::DISABLED;
    }
    else{
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        relayState = toggle == Fw::On::ON ? Fw::Enabled::DISABLED : Fw::Enabled::ENABLED;
    }
    this->tlmWrite_uvState(relayState);
    this->log_ACTIVITY_HI_uvStateChange(relayState);


}

}  // namespace billeeScm
