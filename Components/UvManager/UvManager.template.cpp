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
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void UvManager ::uvToggle_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::On toggle) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
