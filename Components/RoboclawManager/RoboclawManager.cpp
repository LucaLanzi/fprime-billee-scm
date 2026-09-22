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
// Handler implementations for commands
// ----------------------------------------------------------------------

void RoboclawManager ::TODO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
