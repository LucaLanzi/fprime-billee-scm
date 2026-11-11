// ======================================================================
// \title  BootRecord.hpp
// \brief  Assert record that survives a reboot, written by the FW_ASSERT hook and read by BootReporter
// ======================================================================
#ifndef billeeScm_BootRecord_HPP
#define billeeScm_BootRecord_HPP

#include <Fw/FPrimeBasicTypes.hpp>

namespace billeeScm {
namespace BootRecord {

//! Store an FW_ASSERT's location and arguments in RAM2 (not cleared at startup) and flush it out of the data
//! cache, so the next boot can report it. Safe to call from an assert hook, including from interrupt context:
//! it only writes memory.
void recordAssert(const char* file, U32 line, U32 numArgs, const FwAssertArgType args[6]);

}  // namespace BootRecord
}  // namespace billeeScm

#endif
