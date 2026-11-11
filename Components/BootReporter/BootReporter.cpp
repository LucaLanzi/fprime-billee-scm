// ======================================================================
// \title  BootReporter.cpp
// \brief  Reports why the previous boot ended (FW_ASSERT / hard fault) and the SD card mount result
// ======================================================================
#include "Components/BootReporter/BootReporter.hpp"

#include <Arduino.h>  // DMAMEM, arm_dcache_flush, CrashReport
#include <SD.h>
#include <Fw/Types/StringUtils.hpp>

namespace billeeScm {

namespace {

constexpr U32 ASSERT_MAGIC = 0xA55E2705;

//! Lives in RAM2 (DMAMEM = .bss.dma, NOLOAD): Teensy's startup code only clears .bss, so this survives a
//! reboot (assert -> bootloader -> reflash or reset) as long as the board stays powered.
struct AssertRecordData {
    U32 magic;
    U32 line;
    U32 numArgs;
    I32 args[6];
    char file[80];
    U32 check;  //!< magic ^ line ^ numArgs: rejects random power-on RAM contents
};
DMAMEM AssertRecordData s_assertRecord;

//! Print sink that captures CrashReport text into a fixed buffer (no heap)
class BufferPrint : public Print {
  public:
    BufferPrint(char* buffer, size_t size) : m_buffer(buffer), m_size(size) { m_buffer[0] = 0; }
    size_t write(uint8_t c) override {
        if (m_len + 1 >= m_size) {
            return 0;
        }
        // Collapse newlines so the text reads as one event line
        m_buffer[m_len++] = (c == '\n' || c == '\r') ? ' ' : static_cast<char>(c);
        m_buffer[m_len] = 0;
        return 1;
    }

  private:
    char* m_buffer;
    size_t m_size;
    size_t m_len = 0;
};

}  // namespace

namespace BootRecord {
void recordAssert(const char* file, U32 line, U32 numArgs, const FwAssertArgType args[6]) {
    s_assertRecord.magic = ASSERT_MAGIC;
    s_assertRecord.line = line;
    s_assertRecord.numArgs = numArgs;
    for (U32 i = 0; i < 6; i++) {
        s_assertRecord.args[i] = static_cast<I32>(args[i]);
    }
    U32 i = 0;
    for (; file != nullptr && file[i] != 0 && i < sizeof(s_assertRecord.file) - 1; i++) {
        s_assertRecord.file[i] = file[i];
    }
    s_assertRecord.file[i] = 0;
    s_assertRecord.check = ASSERT_MAGIC ^ line ^ numArgs;
    // RAM2 is write-back cached: without the flush the record could still be in the D-cache when the
    // assert hook resets the chip, and it would be lost.
    arm_dcache_flush(&s_assertRecord, sizeof(s_assertRecord));
}
}  // namespace BootRecord

BootReporter ::BootReporter(const char* const compName) : BootReporterComponentBase(compName) {}

BootReporter ::~BootReporter() {}

bool BootReporter ::mountSdCard() {
    this->m_sdCardMounted = SD.begin(BUILTIN_SDCARD);
    this->m_sdErrorCode = SD.sdfs.sdErrorCode();
    this->m_sdErrorData = SD.sdfs.sdErrorData();
    this->m_sdFatType = this->m_sdCardMounted ? SD.sdfs.fatType() : 0;
    return this->m_sdCardMounted;
}

void BootReporter ::configure() {
    (void)this->mountSdCard();

    const AssertRecordData& rec = s_assertRecord;
    if (rec.magic == ASSERT_MAGIC && rec.check == (ASSERT_MAGIC ^ rec.line ^ rec.numArgs)) {
        this->m_hadAssert = true;
        this->m_assertLine = rec.line;
        this->m_assertNumArgs = static_cast<U8>(rec.numArgs);
        for (U32 i = 0; i < 3; i++) {
            this->m_assertArgs[i] = rec.args[i];
        }
        (void)Fw::StringUtils::string_copy(this->m_assertFile, rec.file, sizeof(this->m_assertFile));
    }
    // Clear it so it is reported for exactly one boot
    s_assertRecord.magic = 0;
    arm_dcache_flush(&s_assertRecord, sizeof(s_assertRecord));

    if (CrashReport) {
        this->m_hadFault = true;
        BufferPrint sink(this->m_faultText, sizeof(this->m_faultText));
        CrashReport.printTo(sink);
        CrashReport.clear();
    }
}

void BootReporter ::run_handler(FwIndexType portNum, U32 context) {
    if (!this->m_reported && ++this->m_ticks >= BOOT_REPORT_DELAY_TICKS) {
        this->m_reported = true;
        this->emitReport();
    }
    // Every tick, so a GDS that connects late still sees them
    this->tlmWrite_lastAssertFile(Fw::String(this->m_assertFile));
    this->tlmWrite_lastAssertLine(this->m_assertLine);
    this->tlmWrite_sdCardMounted(this->m_sdCardMounted);
}

void BootReporter ::BOOT_REPORT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->emitReport();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void BootReporter ::SD_MOUNT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->mountSdCard();
    this->log_ACTIVITY_HI_SdCardStatus(this->m_sdCardMounted, this->m_sdErrorCode, this->m_sdErrorData,
                                       this->m_sdFatType);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void BootReporter ::emitReport() {
    if (this->m_hadAssert) {
        this->log_WARNING_HI_PreviousBootAsserted(Fw::String(this->m_assertFile), this->m_assertLine,
                                                  this->m_assertNumArgs, this->m_assertArgs[0],
                                                  this->m_assertArgs[1], this->m_assertArgs[2]);
    }
    if (this->m_hadFault) {
        this->log_WARNING_HI_PreviousBootFaulted(Fw::String(this->m_faultText));
    }
    this->log_ACTIVITY_HI_SdCardStatus(this->m_sdCardMounted, this->m_sdErrorCode, this->m_sdErrorData,
                                       this->m_sdFatType);
}

}  // namespace billeeScm
