module billeeScm {
    @ Reports, once per boot, why the previous boot ended and whether the SD card mounted. The FW_ASSERT hook in
    @ Main.cpp records the failing file/line/arguments in RAM2 (which survives a reboot) via
    @ billeeScm::BootRecord::recordAssert(); Teensy's CrashReport covers hard faults. Without this, an assert just
    @ leaves the board in the bootloader with no trace of where it happened.
    passive component BootReporter {

        @ Rate group tick: publishes telemetry every tick and emits the boot events once, BOOT_REPORT_DELAY_TICKS
        @ after boot, when the GDS has had time to reconnect to the re-enumerated serial port.
        sync input port run: Svc.Sched

        @ Re-emit this boot's report events (previous assert / hard fault, SD card status) now
        sync command BOOT_REPORT() opcode 0x00

        @ Mount (or re-mount) the SD card, e.g. after inserting it, and report the result (SdCardStatus).
        @ Answers EXECUTION_ERROR if it does not mount.
        sync command SD_MOUNT() opcode 0x01

        @ The previous boot ended in an FW_ASSERT (the board then rebooted into the bootloader)
        event PreviousBootAsserted(
            file: string size 80 @< Source file of the failed assert (path relative to the project)
            line: U32            @< Line number
            numArgs: U8          @< Number of assert arguments
            arg1: I32            @< First assert argument (0 if none)
            arg2: I32            @< Second assert argument (0 if none)
            arg3: I32            @< Third assert argument (0 if none)
        ) \
          severity warning high \
          id 0x00 \
          format "Previous boot ended in FW_ASSERT at {}:{} ({} args: {}, {}, {})"

        @ The previous boot ended in a hard fault (Teensy CrashReport, first part)
        event PreviousBootFaulted(
            report: string size 180 @< Start of the CrashReport text
        ) \
          severity warning high \
          id 0x01 \
          format "Previous boot hard-faulted: {}"

        @ SD card mount result (SD.begin(BUILTIN_SDCARD)), at boot and on SD_MOUNT. Sequences and file uplink need it.
        @ errorCode is SdFat's sdErrorCode (SdCard/SdCardInfo.h): 0x01 = no response to CMD0 (no card / not seated),
        @ 0x17 (ACMD41) = card init timeout. errorCode 0 with fatType 0 means the card answered but has no
        @ FAT16/FAT32/exFAT filesystem (reformat it).
        event SdCardStatus(
            mounted: bool   @< True if the card initialized and its filesystem mounted
            errorCode: U8   @< SdFat sdErrorCode (0 = none)
            errorData: U8   @< SdFat sdErrorData
            fatType: U8     @< 12/16/32 = FAT, 64 = exFAT, 0 = no filesystem mounted
        ) \
          severity activity high \
          id 0x02 \
          format "SD card mounted: {} (error code 0x{x}, data 0x{x}, FAT type {})"

        @ Source file of the assert that ended the previous boot (empty if it did not end in an assert)
        telemetry lastAssertFile: string size 40

        @ Line of the assert that ended the previous boot (0 if it did not end in an assert)
        telemetry lastAssertLine: U32

        @ SD card mounted at boot
        telemetry sdCardMounted: bool

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel
    }
}
