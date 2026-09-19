// ======================================================================
// \title  RelayManager.cpp
// \author luca_lanzi
// \brief  cpp file for RelayManager component implementation class
// ======================================================================

#include "Components/RelayManager/RelayManager.hpp"

namespace billeeScm {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RelayManager ::RelayManager(const char* const compName) : RelayManagerComponentBase(compName) {}

RelayManager ::~RelayManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void RelayManager ::run_handler(FwIndexType portNum, U32 context) {
    // List of all 4 relay ids, so we can loop over every relay each time this handler runs
    // (it's called on every tick of the Svc.Sched rate group this component is hooked up to).
    static const billeeScm::relayId::T relayIds[] = {
        billeeScm::relayId::RELAY1,
        billeeScm::relayId::RELAY2,
        billeeScm::relayId::RELAY3,
        billeeScm::relayId::RELAY4,
    };

    // Check every relay, one at a time, on every call.
    for (const billeeScm::relayId::T id : relayIds) {
        // Outer switch: pick which relay we're looking at this iteration — each case reads
        // that relay's own tracked state and drives that relay's own GPIO output port
        // (each relay's port is a distinct, separately-named port, so this can't be
        // factored into a single shared call the way the state lookup could be).
        switch (id) {
            case billeeScm::relayId::RELAY1:
                // Inner switch: look at this relay's currently tracked state (set by
                // relayToggle) and drive its pin to match.
                switch (this->m_relay1.get_relayState()) {
                    case Fw::Enabled::ENABLED:
                        this->relay1Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->relay1Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            case billeeScm::relayId::RELAY2:
                switch (this->m_relay2.get_relayState()) {
                    case Fw::Enabled::ENABLED:
                        this->relay2Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->relay2Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            case billeeScm::relayId::RELAY3:
                switch (this->m_relay3.get_relayState()) {
                    case Fw::Enabled::ENABLED:
                        this->relay3Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->relay3Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            case billeeScm::relayId::RELAY4:
                switch (this->m_relay4.get_relayState()) {
                    case Fw::Enabled::ENABLED:
                        this->relay4Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->relay4Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            default:
                // Shouldn't happen (relayIds only contains real enum values).
                break;
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void RelayManager ::relayToggle_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::On toggle, billeeScm::relayId relay) {
    // Convert the command's Fw::On (ON/OFF) argument into Fw::Enabled (ENABLED/DISABLED),
    // since that's the type used everywhere else in this component (the relay struct's
    // state field, the telemetry channels, and the event below).
    const Fw::Enabled newState = (toggle == Fw::On::ON) ? Fw::Enabled::ENABLED : Fw::Enabled::DISABLED;
    // ^ if toggle is on, set newState to ENABLED, if off set newSTATE to DISABLED

    // Pick which relay the command targets, update its tracked state, and publish that
    // update as telemetry on that relay's own channel.
    switch (relay.e) {
        case billeeScm::relayId::RELAY1:
            this->m_relay1.set_relayState(newState);
            this->tlmWrite_relay1State(newState);
            break;
        case billeeScm::relayId::RELAY2:
            this->m_relay2.set_relayState(newState);
            this->tlmWrite_relay2State(newState);
            break;
        case billeeScm::relayId::RELAY3:
            this->m_relay3.set_relayState(newState);
            this->tlmWrite_relay3State(newState);
            break;
        case billeeScm::relayId::RELAY4:
            this->m_relay4.set_relayState(newState);
            this->tlmWrite_relay4State(newState);
            break;
        default:
            // relay wasn't one of the 4 known ids: reject the command and stop here,
            // without emitting the event or touching any relay state below.
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
            return;
    }

    // Only reached for a valid relay: announce which relay changed and what it changed to.
    this->log_ACTIVITY_HI_relayState(relay, newState);
    // Tell the command dispatcher the command completed successfully.
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
