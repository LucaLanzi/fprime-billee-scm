// ======================================================================
// \title  RelayManager.hpp
// \author luca_lanzi
// \brief  hpp file for RelayManager component implementation class
// ======================================================================

#ifndef billeeScm_RelayManager_HPP
#define billeeScm_RelayManager_HPP

#include "Components/RelayManager/RelayManagerComponentAc.hpp"
#include "Types/relaySerializableAc.hpp"

namespace billeeScm {

class RelayManager final : public RelayManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct RelayManager object
    RelayManager(const char* const compName  //!< The component name
    );

    //! Destroy RelayManager object
    ~RelayManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------
    billeeScm::relay m_relay1 = billeeScm::relay(billeeScm::relayId::RELAY1, Fw::Enabled::DISABLED);
    billeeScm::relay m_relay2 = billeeScm::relay(billeeScm::relayId::RELAY2, Fw::Enabled::DISABLED);
    billeeScm::relay m_relay3 = billeeScm::relay(billeeScm::relayId::RELAY3, Fw::Enabled::DISABLED);
    billeeScm::relay m_relay4 = billeeScm::relay(billeeScm::relayId::RELAY4, Fw::Enabled::DISABLED);


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

    //! Handler implementation for command relayToggle
    //!
    //! Command to toggle relay
    void relayToggle_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq,           //!< The command sequence number
                                Fw::On toggle,
                                billeeScm::relayId relay) override;
};

}  // namespace billeeScm

#endif
