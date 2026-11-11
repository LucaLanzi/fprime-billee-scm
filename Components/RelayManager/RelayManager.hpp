// ======================================================================
// \title  RelayManager.hpp
// \brief  Logic-level relays: pumps 1-4, UV light, magnetic stirrer
// ======================================================================
#ifndef billeeScm_RelayManager_HPP
#define billeeScm_RelayManager_HPP

#include "Components/RelayManager/RelayManagerComponentAc.hpp"

namespace billeeScm {

class RelayManager final : public RelayManagerComponentBase {
  public:
    //! Number of relays (RELAY_COUNT in Types/RelayTypes.fpp)
    static constexpr FwIndexType NUM_RELAYS = billeeScm::RelayStates::SIZE;

    //! Longest ON_FOR, in ms. Kept well under cmdSeq's 600 s per-command timeout.
    static constexpr U32 MAX_ON_FOR_MS = 300000;

    RelayManager(const char* const compName);
    ~RelayManager();

  private:
    void run_handler(FwIndexType portNum, U32 context) override;
    void SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RelayId relay, Fw::On onOff) override;
    void ON_FOR_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RelayId relay, U32 durationMs) override;
    void ALL_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Set a relay's commanded state and write its pin. Emits RelaySwitched on a change. Returns false if the GPIO
    //! write failed.
    bool apply(FwIndexType relay, Fw::On state);

    //! Answer and forget a running ON_FOR (no-op if none on this relay)
    void finishOnFor(FwIndexType relay, Fw::CmdResponse response);

    //! An ON_FOR in progress: answered when the relay switches off
    struct OnFor {
        bool active = false;
        FwOpcodeType opCode = 0;
        U32 cmdSeq = 0;
        Fw::Time start;
        U32 durationMs = 0;
    };

    billeeScm::RelayStates m_states;  //!< Commanded state per relay (all OFF at boot)
    OnFor m_onFor[NUM_RELAYS];
};

}  // namespace billeeScm

#endif
