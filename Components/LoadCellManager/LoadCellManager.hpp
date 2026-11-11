// ======================================================================
// \title  LoadCellManager.hpp
// \brief  HX711 load-cell ADCs (Sample Cache weight sensors)
// ======================================================================
#ifndef billeeScm_LoadCellManager_HPP
#define billeeScm_LoadCellManager_HPP

#include "Components/LoadCellManager/LoadCellManagerComponentAc.hpp"

namespace billeeScm {

class LoadCellManager final : public LoadCellManagerComponentBase {
  public:
    static constexpr FwIndexType NUM_CELLS = billeeScm::LoadCellStates::SIZE;
    static constexpr U8 MAX_SAMPLES = 80;         //!< Longest TARE / CALIBRATE average (8 s at 10 SPS)
    static constexpr U32 MAX_WAIT_MS = 300000;    //!< Longest WAIT_MASS
    static constexpr U32 STALE_MS = 1000;         //!< No sample for this long = disconnected

    LoadCellManager(const char* const compName);
    ~LoadCellManager();

    //! Bind a cell to its HX711 pins and starting scale. Call once per cell during topology setup.
    void configure(billeeScm::LoadCellId cell, U8 datPin, U8 clkPin, F32 countsPerGram);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;
    void TARE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::LoadCellId cell, U8 samples) override;
    void CALIBRATE_cmdHandler(FwOpcodeType opCode,
                              U32 cmdSeq,
                              billeeScm::LoadCellId cell,
                              F32 knownGrams,
                              U8 samples) override;
    void SET_SCALE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::LoadCellId cell, F32 countsPerGram) override;
    void WAIT_MASS_cmdHandler(FwOpcodeType opCode,
                              U32 cmdSeq,
                              billeeScm::LoadCellId cell,
                              billeeScm::MassCompare compare,
                              F32 grams,
                              U32 timeoutMs) override;

    enum class Op : U8 { NONE, TARE, CALIBRATE, WAIT };

    struct Cell {
        bool configured = false;
        U8 dat = 0;
        U8 clk = 0;
        bool connected = false;
        bool everSampled = false;
        Fw::Time lastSample;
        I32 raw = 0;
        I32 tareOffset = 0;
        F32 countsPerGram = 1.0f;
        // Pending command
        Op op = Op::NONE;
        FwOpcodeType opCode = 0;
        U32 cmdSeq = 0;
        Fw::Time start;
        U32 limitMs = 0;
        U8 samplesWanted = 0;
        U8 samplesTaken = 0;
        I64 sampleSum = 0;
        F32 target = 0;  //!< CALIBRATE: known grams; WAIT: threshold
        billeeScm::MassCompare::T compare = billeeScm::MassCompare::ABOVE;
    };

    //! Read one sample if the HX711 has one ready. Returns true if a new sample was read into `raw`.
    bool readSample(Cell& cell);

    //! Mass in grams from a raw reading
    static F32 toGrams(const Cell& cell, I32 raw);

    //! Start a pending command on a cell (answering any command it supersedes)
    bool beginOp(FwOpcodeType opCode, U32 cmdSeq, FwIndexType c, Op op, U32 limitMs);

    //! Answer and clear a cell's pending command
    void finishOp(FwIndexType c, Fw::CmdResponse response);

    //! Advance a cell's pending command with a new sample
    void onSample(FwIndexType c);

    void publishTelemetry();

    Cell m_cells[NUM_CELLS];
};

}  // namespace billeeScm

#endif
