// ======================================================================
// \title  PumpManager.hpp
// \author luca_lanzi
// \brief  hpp file for PumpManager component implementation class
// ======================================================================

#ifndef billeeScm_PumpManager_HPP
#define billeeScm_PumpManager_HPP

#include "Components/PumpManager/PumpManagerComponentAc.hpp"
#include "Types/pumpSerializableAc.hpp"

namespace billeeScm {

class PumpManager final : public PumpManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PumpManager object
    PumpManager(const char* const compName  //!< The component name
    );

    //! Destroy PumpManager object
    ~PumpManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------
    billeeScm::pump m_pump1 = billeeScm::pump(billeeScm::pumpId::PUMP1, Fw::Enabled::DISABLED);
    billeeScm::pump m_pump2 = billeeScm::pump(billeeScm::pumpId::PUMP2, Fw::Enabled::DISABLED);
    billeeScm::pump m_pump3 = billeeScm::pump(billeeScm::pumpId::PUMP3, Fw::Enabled::DISABLED);
    billeeScm::pump m_pump4 = billeeScm::pump(billeeScm::pumpId::PUMP4, Fw::Enabled::DISABLED);


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

    //! Handler implementation for command pumpToggle
    //!
    //! Command to toggle pump
    void pumpToggle_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq,           //!< The command sequence number
                               Fw::On toggle,
                               billeeScm::pumpId pump) override;
};

}  // namespace billeeScm

#endif
