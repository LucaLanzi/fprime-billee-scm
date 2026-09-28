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
// Handler implementations for commands
// ----------------------------------------------------------------------

void StirrerManager ::TODO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // TODO
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
