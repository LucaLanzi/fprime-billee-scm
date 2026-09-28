// ======================================================================
// \title  StirrerManager.hpp
// \author luca_lanzi
// \brief  hpp file for StirrerManager component implementation class
// ======================================================================

#ifndef billeeScm_StirrerManager_HPP
#define billeeScm_StirrerManager_HPP

#include "Components/StirrerManager/StirrerManagerComponentAc.hpp"

namespace billeeScm {

class StirrerManager final : public StirrerManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StirrerManager object
    StirrerManager(const char* const compName  //!< The component name
    );

    //! Destroy StirrerManager object
    ~StirrerManager();

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
