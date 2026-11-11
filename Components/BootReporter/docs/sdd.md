# billeeScm::BootReporter

Reports why the previous boot ended and whether the SD card mounted. Without it, a failed `FW_ASSERT` only shows up as
the board sitting in the Teensy bootloader (USB `16c0:0478`) with no trace of where it happened.

## How the assert is captured

`Main.cpp`'s assert hook calls `billeeScm::BootRecord::recordAssert()` before rebooting into the bootloader. That
writes the file, line and arguments into a struct in RAM2 (`DMAMEM`, linker section `.bss.dma`, which Teensy's startup
code does not clear) and flushes it out of the data cache. The record survives the reboot and a reflash as long as the
board stays powered. On the next boot `configure()` reads it once and clears it. Teensy's own `CrashReport` covers hard
faults the same way.

## What it reports

| When | What |
|---|---|
| Every tick | Telemetry `lastAssertFile` (first 40 characters), `lastAssertLine`, `sdCardMounted` |
| 15 s after boot (`BOOT_REPORT_DELAY_TICKS`), once | Events `PreviousBootAsserted` (file, line, first 3 args), `PreviousBootFaulted` (start of the CrashReport text), `SdCardStatus` |
| `BOOT_REPORT` command | The same events again |

The delay gives the GDS time to reconnect to the re-enumerated USB serial port; the telemetry covers a GDS that
connects later.

## SD card

`configure()` mounts the card (`SD.begin(BUILTIN_SDCARD)`, the Teensy 4.1's own socket). `SD_MOUNT` retries, e.g. after
inserting a card. `SdCardStatus` carries SdFat's error code: `0x17` (ACMD41) = card never finished initializing, which
over SDIO is also what an empty socket looks like; error code 0 with FAT type 0 = card answered but has no
FAT16/FAT32/exFAT filesystem.

`SD.begin()` blocks for SdFat's 2 s init timeout when no card answers. The TaskRunner is cooperative, so during that
time every component with an asserting rate-group port fills its queue; see `SCHED_QUEUE_SIZE` in `instances.fpp`.
