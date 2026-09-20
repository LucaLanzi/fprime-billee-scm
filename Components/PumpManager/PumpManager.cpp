// ======================================================================
// \title  PumpManager.cpp
// \author luca_lanzi
// \brief  cpp file for PumpManager component implementation class
// ======================================================================

#include "Components/PumpManager/PumpManager.hpp"

namespace billeeScm {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PumpManager ::PumpManager(const char* const compName) : PumpManagerComponentBase(compName) {}

PumpManager ::~PumpManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PumpManager ::run_handler(FwIndexType portNum, U32 context) {
    // List of all 4 pump ids, so we can loop over every pump each time this handler runs
    // (it's called on every tick of the Svc.Sched rate group this component is hooked up to).
    static const billeeScm::pumpId::T pumpIds[] = {
        billeeScm::pumpId::PUMP1,
        billeeScm::pumpId::PUMP2,
        billeeScm::pumpId::PUMP3,
        billeeScm::pumpId::PUMP4,
    };

    // Check every pump, one at a time, on every call.
    for (const billeeScm::pumpId::T id : pumpIds) {
        // Outer switch: pick which pump we're looking at this iteration — each case reads
        // that pump's own tracked state and drives that pump's own GPIO output port
        // (each pump's port is a distinct, separately-named port, so this can't be
        // factored into a single shared call the way the state lookup could be).
        switch (id) {
            case billeeScm::pumpId::PUMP1:
                // Inner switch: look at this pump's currently tracked state (set by
                // pumpToggle) and drive its pin to match.
                switch (this->m_pump1.get_pumpState()) {
                    case Fw::Enabled::ENABLED:
                        this->pump1Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->pump1Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            case billeeScm::pumpId::PUMP2:
                switch (this->m_pump2.get_pumpState()) {
                    case Fw::Enabled::ENABLED:
                        this->pump2Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->pump2Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            case billeeScm::pumpId::PUMP3:
                switch (this->m_pump3.get_pumpState()) {
                    case Fw::Enabled::ENABLED:
                        this->pump3Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->pump3Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            case billeeScm::pumpId::PUMP4:
                switch (this->m_pump4.get_pumpState()) {
                    case Fw::Enabled::ENABLED:
                        this->pump4Set_out(0, Fw::Logic::HIGH);
                        break;
                    case Fw::Enabled::DISABLED:
                        this->pump4Set_out(0, Fw::Logic::LOW);
                        break;
                }
                break;
            default:
                // Shouldn't happen (pumpIds only contains real enum values).
                break;
        }
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PumpManager ::pumpToggle_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::On toggle, billeeScm::pumpId pump) {
    // Convert the command's Fw::On (ON/OFF) argument into Fw::Enabled (ENABLED/DISABLED),
    // since that's the type used everywhere else in this component (the pump struct's
    // state field, the telemetry channels, and the event below).
    const Fw::Enabled newState = (toggle == Fw::On::ON) ? Fw::Enabled::ENABLED : Fw::Enabled::DISABLED;
    // ^ if toggle is on, set newState to ENABLED, if off set newSTATE to DISABLED

    // Pick which pump the command targets, update its tracked state, and publish that
    // update as telemetry on that pump's own channel.
    switch (pump.e) {
        case billeeScm::pumpId::PUMP1:
            this->m_pump1.set_pumpState(newState);
            this->tlmWrite_pump1State(newState);
            break;
        case billeeScm::pumpId::PUMP2:
            this->m_pump2.set_pumpState(newState);
            this->tlmWrite_pump2State(newState);
            break;
        case billeeScm::pumpId::PUMP3:
            this->m_pump3.set_pumpState(newState);
            this->tlmWrite_pump3State(newState);
            break;
        case billeeScm::pumpId::PUMP4:
            this->m_pump4.set_pumpState(newState);
            this->tlmWrite_pump4State(newState);
            break;
        default:
            // pump wasn't one of the 4 known ids: reject the command and stop here,
            // without emitting the event or touching any pump state below.
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
            return;
    }

    // Only reached for a valid pump: announce which pump changed and what it changed to.
    this->log_ACTIVITY_HI_pumpState(pump, newState);
    // Tell the command dispatcher the command completed successfully.
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace billeeScm
