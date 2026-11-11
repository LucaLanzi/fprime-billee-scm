// ======================================================================
// \title  RelayManager.cpp
// \brief  Logic-level relays: pumps 1-4, UV light, magnetic stirrer
// ======================================================================
#include "Components/RelayManager/RelayManager.hpp"
#include "Components/Common/Elapsed.hpp"

namespace billeeScm {

static_assert(billeeScm::RelayId::NUM_CONSTANTS == RelayManager::NUM_RELAYS, "RelayId must name every relay (RelayTypes.fpp)");

RelayManager ::RelayManager(const char* const compName) : RelayManagerComponentBase(compName) {
    for (FwIndexType i = 0; i < NUM_RELAYS; i++) {
        this->m_states[i] = Fw::On::OFF;
    }
}

RelayManager ::~RelayManager() {}

void RelayManager ::run_handler(FwIndexType portNum, U32 context) {
    const Fw::Time now = this->getTime();
    for (FwIndexType i = 0; i < NUM_RELAYS; i++) {
        OnFor& run = this->m_onFor[i];
        if (run.active && elapsedMs(run.start, now) >= static_cast<I64>(run.durationMs)) {
            const bool ok = this->apply(i, Fw::On::OFF);
            this->finishOnFor(i, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
        } else if (this->isConnected_relaySet_OutputPort(i)) {
            // Re-drive from the commanded state every tick so a pin can never drift from what telemetry reports
            (void)this->relaySet_out(i, (this->m_states[i] == Fw::On::ON) ? Fw::Logic::HIGH : Fw::Logic::LOW);
        }
    }
    this->tlmWrite_relayStates(this->m_states);
}

void RelayManager ::SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RelayId relay, Fw::On onOff) {
    if (!relay.isValid() || !onOff.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    const FwIndexType i = static_cast<FwIndexType>(relay.e);
    if (this->m_onFor[i].active) {
        this->log_WARNING_LO_OnForCancelled(relay, static_cast<U32>(elapsedMs(this->m_onFor[i].start, this->getTime())));
        this->finishOnFor(i, Fw::CmdResponse::EXECUTION_ERROR);
    }
    const bool ok = this->apply(i, onOff);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void RelayManager ::ON_FOR_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RelayId relay, U32 durationMs) {
    if (!relay.isValid() || durationMs == 0 || durationMs > MAX_ON_FOR_MS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    const FwIndexType i = static_cast<FwIndexType>(relay.e);
    if (this->m_onFor[i].active) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (!this->apply(i, Fw::On::ON)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    OnFor& run = this->m_onFor[i];
    run.active = true;
    run.opCode = opCode;
    run.cmdSeq = cmdSeq;
    run.start = this->getTime();
    run.durationMs = durationMs;
    // Answered by run_handler when the time is up
}

void RelayManager ::ALL_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    bool ok = true;
    const Fw::Time now = this->getTime();
    for (FwIndexType i = 0; i < NUM_RELAYS; i++) {
        if (this->m_onFor[i].active) {
            this->log_WARNING_LO_OnForCancelled(static_cast<billeeScm::RelayId::T>(i),
                                                static_cast<U32>(elapsedMs(this->m_onFor[i].start, now)));
            this->finishOnFor(i, Fw::CmdResponse::EXECUTION_ERROR);
        }
        ok = this->apply(i, Fw::On::OFF) && ok;
    }
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

bool RelayManager ::apply(FwIndexType relay, Fw::On state) {
    const bool changed = (this->m_states[relay] != state);
    this->m_states[relay] = state;
    bool ok = false;
    if (this->isConnected_relaySet_OutputPort(relay)) {
        ok = (this->relaySet_out(relay, (state == Fw::On::ON) ? Fw::Logic::HIGH : Fw::Logic::LOW) ==
              Drv::GpioStatus::OP_OK);
    }
    if (!ok) {
        this->log_WARNING_HI_GpioWriteFailed(static_cast<billeeScm::RelayId::T>(relay));
    } else if (changed) {
        this->log_ACTIVITY_HI_RelaySwitched(static_cast<billeeScm::RelayId::T>(relay), state);
    }
    this->tlmWrite_relayStates(this->m_states);
    return ok;
}

void RelayManager ::finishOnFor(FwIndexType relay, Fw::CmdResponse response) {
    OnFor& run = this->m_onFor[relay];
    if (run.active) {
        run.active = false;
        this->cmdResponse_out(run.opCode, run.cmdSeq, response);
    }
}

}  // namespace billeeScm
