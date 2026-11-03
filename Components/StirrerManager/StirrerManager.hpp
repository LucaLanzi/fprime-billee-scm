// ======================================================================
// \title  StirrerManager.hpp
// \author luca_lanzi
// \brief  hpp file for StirrerManager component implementation class
// ======================================================================

#ifndef billeeScm_StirrerManager_HPP
#define billeeScm_StirrerManager_HPP

#include "Components/StirrerManager/StirrerManagerComponentAc.hpp"
#include "Types/stirrerSerializableAc.hpp"

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
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    billeeScm::stirrer m_stirrer1 = billeeScm::stirrer(billeeScm::stirrerId::STIRRER1, Fw::Enabled::DISABLED);
    billeeScm::stirrer m_stirrer2 = billeeScm::stirrer(billeeScm::stirrerId::STIRRER2, Fw::Enabled::DISABLED);

    //! Handler implementation for run
    //!
    //! async Input port for the rate group driver
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command stirrerToggle
    //!
    //! Command to toggle the desired magnetic stirrer
    void stirrerToggle_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq,           //!< The command sequence number
                                  Fw::On toggle,
                                  billeeScm::stirrerId stirrer) override;
};

}  // namespace billeeScm

#endif
