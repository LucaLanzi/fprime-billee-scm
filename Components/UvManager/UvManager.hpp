// ======================================================================
// \title  UvManager.hpp
// \author root
// \brief  hpp file for UvManager component implementation class
// ======================================================================

#ifndef billeeScm_UvManager_HPP
#define billeeScm_UvManager_HPP

#include "Components/UvManager/UvManagerComponentAc.hpp"

namespace billeeScm {

class UvManager final : public UvManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct UvManager object
    UvManager(const char* const compName  //!< The component name
    );

    //! Destroy UvManager object
    ~UvManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! input port of type Svc.Sched to invoke the component
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command uvToggle
    //!
    //! Command to toggle relay
    void uvToggle_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             Fw::On toggle) override;
};

}  // namespace billeeScm

#endif
