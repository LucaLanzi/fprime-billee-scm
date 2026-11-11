// ======================================================================
// \title  LoadCellManager.cpp
// \brief  HX711 load-cell ADCs (Sample Cache weight sensors)
// ======================================================================
#include "Components/LoadCellManager/LoadCellManager.hpp"
#include "Components/Common/Elapsed.hpp"

#include <Arduino.h>
#include <cmath>

namespace billeeScm {

static_assert(billeeScm::LoadCellId::NUM_CONSTANTS == LoadCellManager::NUM_CELLS, "LoadCellId must name every cell");

LoadCellManager ::LoadCellManager(const char* const compName) : LoadCellManagerComponentBase(compName) {}

LoadCellManager ::~LoadCellManager() {}

void LoadCellManager ::configure(billeeScm::LoadCellId cell, U8 datPin, U8 clkPin, F32 countsPerGram) {
    FW_ASSERT(cell.isValid(), cell.e);
    Cell& c = this->m_cells[cell.e];
    c.dat = datPin;
    c.clk = clkPin;
    c.countsPerGram = (countsPerGram != 0.0f) ? countsPerGram : 1.0f;
    // Pull-up on DAT: with no HX711 attached the pin reads HIGH ("not ready") rather than floating, so a missing
    // cell shows as disconnected instead of producing random readings.
    pinMode(datPin, INPUT_PULLUP);
    pinMode(clkPin, OUTPUT);
    // CLK must idle LOW: held HIGH for more than 60 us the HX711 powers down
    digitalWrite(clkPin, LOW);
    c.configured = true;
}

// ----------------------------------------------------------------------
// Tick
// ----------------------------------------------------------------------

void LoadCellManager ::run_handler(FwIndexType portNum, U32 context) {
    const Fw::Time now = this->getTime();
    for (FwIndexType i = 0; i < NUM_CELLS; i++) {
        Cell& cell = this->m_cells[i];
        if (!cell.configured) {
            continue;
        }
        if (this->readSample(cell)) {
            cell.lastSample = now;
            cell.everSampled = true;
            if (!cell.connected) {
                cell.connected = true;
                this->log_ACTIVITY_HI_CellConnected(static_cast<billeeScm::LoadCellId::T>(i));
            }
            this->onSample(i);
        } else if (cell.connected && elapsedMs(cell.lastSample, now) > STALE_MS) {
            cell.connected = false;
            this->log_WARNING_HI_CellDisconnected(static_cast<billeeScm::LoadCellId::T>(i));
        }
        // A pending command fails if the cell goes quiet or its time runs out
        if (cell.op != Op::NONE && (!cell.connected || elapsedMs(cell.start, now) > static_cast<I64>(cell.limitMs))) {
            this->log_WARNING_HI_CellCommandFailed(static_cast<billeeScm::LoadCellId::T>(i), toGrams(cell, cell.raw));
            this->finishOp(i, Fw::CmdResponse::EXECUTION_ERROR);
        }
    }
    this->publishTelemetry();
}

bool LoadCellManager ::readSample(Cell& cell) {
    // DOUT goes LOW when a conversion is ready
    if (digitalRead(cell.dat) != LOW) {
        return false;
    }
    U32 value = 0;
    // 24 data bits MSB first, then one extra pulse to select channel A / gain 128 for the next conversion. Interrupts
    // are off for the ~50 us of clocking: an interrupt that kept CLK high for > 60 us would power the HX711 down.
    noInterrupts();
    for (U8 bit = 0; bit < 24; bit++) {
        digitalWrite(cell.clk, HIGH);
        delayMicroseconds(1);
        value = (value << 1) | (digitalRead(cell.dat) == HIGH ? 1u : 0u);
        digitalWrite(cell.clk, LOW);
        delayMicroseconds(1);
    }
    digitalWrite(cell.clk, HIGH);
    delayMicroseconds(1);
    digitalWrite(cell.clk, LOW);
    interrupts();
    // Sign-extend the 24-bit two's complement value
    if (value & 0x800000u) {
        value |= 0xFF000000u;
    }
    cell.raw = static_cast<I32>(value);
    return true;
}

F32 LoadCellManager ::toGrams(const Cell& cell, I32 raw) {
    return static_cast<F32>(raw - cell.tareOffset) / cell.countsPerGram;
}

void LoadCellManager ::onSample(FwIndexType c) {
    Cell& cell = this->m_cells[c];
    const billeeScm::LoadCellId id = static_cast<billeeScm::LoadCellId::T>(c);
    switch (cell.op) {
        case Op::TARE:
        case Op::CALIBRATE:
            cell.sampleSum += cell.raw;
            if (++cell.samplesTaken >= cell.samplesWanted) {
                const I32 mean = static_cast<I32>(cell.sampleSum / cell.samplesTaken);
                if (cell.op == Op::TARE) {
                    cell.tareOffset = mean;
                    this->log_ACTIVITY_HI_Tared(id, mean);
                    this->finishOp(c, Fw::CmdResponse::OK);
                } else {
                    const F32 counts = static_cast<F32>(mean - cell.tareOffset);
                    if (counts == 0.0f) {
                        // Nothing on the cell (or not tared): refuse rather than set a zero scale
                        this->log_WARNING_HI_CellCommandFailed(id, toGrams(cell, cell.raw));
                        this->finishOp(c, Fw::CmdResponse::EXECUTION_ERROR);
                    } else {
                        cell.countsPerGram = counts / cell.target;
                        this->log_ACTIVITY_HI_Calibrated(id, cell.countsPerGram);
                        this->finishOp(c, Fw::CmdResponse::OK);
                    }
                }
            }
            break;
        case Op::WAIT: {
            const F32 grams = toGrams(cell, cell.raw);
            const bool met = (cell.compare == billeeScm::MassCompare::ABOVE) ? (grams >= cell.target)
                                                                              : (grams <= cell.target);
            if (met) {
                this->log_ACTIVITY_HI_MassReached(id, grams);
                this->finishOp(c, Fw::CmdResponse::OK);
            }
            break;
        }
        default:
            break;
    }
}

// ----------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------

void LoadCellManager ::TARE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::LoadCellId cell, U8 samples) {
    if (!cell.isValid() || samples == 0 || samples > MAX_SAMPLES) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (this->beginOp(opCode, cmdSeq, cell.e, Op::TARE, static_cast<U32>(samples) * 100 + STALE_MS)) {
        this->m_cells[cell.e].samplesWanted = samples;
    }
}

void LoadCellManager ::CALIBRATE_cmdHandler(FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            billeeScm::LoadCellId cell,
                                            F32 knownGrams,
                                            U8 samples) {
    if (!cell.isValid() || samples == 0 || samples > MAX_SAMPLES || !(knownGrams > 0.0f) || !std::isfinite(knownGrams)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (this->beginOp(opCode, cmdSeq, cell.e, Op::CALIBRATE, static_cast<U32>(samples) * 100 + STALE_MS)) {
        this->m_cells[cell.e].samplesWanted = samples;
        this->m_cells[cell.e].target = knownGrams;
    }
}

void LoadCellManager ::SET_SCALE_cmdHandler(FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            billeeScm::LoadCellId cell,
                                            F32 countsPerGram) {
    if (!cell.isValid() || countsPerGram == 0.0f || !std::isfinite(countsPerGram)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->m_cells[cell.e].countsPerGram = countsPerGram;
    this->log_ACTIVITY_HI_Calibrated(cell, countsPerGram);
    this->publishTelemetry();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void LoadCellManager ::WAIT_MASS_cmdHandler(FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            billeeScm::LoadCellId cell,
                                            billeeScm::MassCompare compare,
                                            F32 grams,
                                            U32 timeoutMs) {
    if (!cell.isValid() || !compare.isValid() || !std::isfinite(grams) || timeoutMs == 0 || timeoutMs > MAX_WAIT_MS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (this->beginOp(opCode, cmdSeq, cell.e, Op::WAIT, timeoutMs)) {
        this->m_cells[cell.e].target = grams;
        this->m_cells[cell.e].compare = compare.e;
    }
}

bool LoadCellManager ::beginOp(FwOpcodeType opCode, U32 cmdSeq, FwIndexType c, Op op, U32 limitMs) {
    Cell& cell = this->m_cells[c];
    if (!cell.configured || !cell.connected) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return false;
    }
    // A new command on the cell supersedes the one still running on it
    if (cell.op != Op::NONE) {
        this->log_WARNING_HI_CellCommandFailed(static_cast<billeeScm::LoadCellId::T>(c), toGrams(cell, cell.raw));
        this->finishOp(c, Fw::CmdResponse::EXECUTION_ERROR);
    }
    cell.op = op;
    cell.opCode = opCode;
    cell.cmdSeq = cmdSeq;
    cell.start = this->getTime();
    cell.limitMs = limitMs;
    cell.samplesTaken = 0;
    cell.sampleSum = 0;
    return true;
}

void LoadCellManager ::finishOp(FwIndexType c, Fw::CmdResponse response) {
    Cell& cell = this->m_cells[c];
    if (cell.op != Op::NONE) {
        cell.op = Op::NONE;
        this->cmdResponse_out(cell.opCode, cell.cmdSeq, response);
    }
}

void LoadCellManager ::publishTelemetry() {
    billeeScm::LoadCellStates states;
    for (FwIndexType i = 0; i < NUM_CELLS; i++) {
        const Cell& cell = this->m_cells[i];
        states[i] = billeeScm::LoadCellState(cell.connected, cell.raw, toGrams(cell, cell.raw), cell.tareOffset,
                                             cell.countsPerGram);
    }
    this->tlmWrite_cellStates(states);
}

}  // namespace billeeScm
