// ======================================================================
// \title  BootReporter.hpp
// \brief  Reports why the previous boot ended (FW_ASSERT / hard fault) and the SD card mount result
// ======================================================================
#ifndef billeeScm_BootReporter_HPP
#define billeeScm_BootReporter_HPP

#include "Components/BootReporter/BootReporterComponentAc.hpp"
#include "Components/BootReporter/BootRecord.hpp"

namespace billeeScm {

class BootReporter final : public BootReporterComponentBase {
  public:
    //! Ticks (of the rate group driving `run`) after boot before the report events are emitted, so the GDS has
    //! reconnected to the re-enumerated USB serial port. 150 ticks = 15 s at 10 Hz.
    static constexpr U32 BOOT_REPORT_DELAY_TICKS = 150;

    BootReporter(const char* const compName);
    ~BootReporter();

    //! Read (and clear) the previous boot's assert record and Teensy CrashReport, and mount the SD card
    //! (SD.begin(BUILTIN_SDCARD)). Call once from topology setup.
    void configure();

  private:
    void run_handler(FwIndexType portNum, U32 context) override;
    void BOOT_REPORT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void SD_MOUNT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! SD.begin(BUILTIN_SDCARD), recording the result and SdFat's error code / filesystem type
    bool mountSdCard();

    //! Emit the report events for this boot
    void emitReport();

    bool m_sdCardMounted = false;
    U8 m_sdErrorCode = 0;
    U8 m_sdErrorData = 0;
    U8 m_sdFatType = 0;
    bool m_hadAssert = false;
    bool m_hadFault = false;
    bool m_reported = false;
    U32 m_ticks = 0;
    U32 m_assertLine = 0;
    U8 m_assertNumArgs = 0;
    I32 m_assertArgs[3] = {0, 0, 0};
    char m_assertFile[81] = {0};
    char m_faultText[181] = {0};
};

}  // namespace billeeScm

#endif
