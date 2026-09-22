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
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TODO
    //!
    //! TODO
    void TODO_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;
};

}  // namespace billeeScm

#endif
