// ======================================================================
// \title  Elapsed.hpp
// \brief  Elapsed-time helper shared by the Science Control Module components
// ======================================================================
#ifndef billeeScm_Common_Elapsed_HPP
#define billeeScm_Common_Elapsed_HPP

#include <Fw/Time/Time.hpp>

namespace billeeScm {

//! Milliseconds from `start` to `now` (negative if `now` is earlier). Timed commands measure their duration with
//! this rather than by counting rate-group ticks, because rate-group ports drop ticks when a queue is full.
inline I64 elapsedMs(const Fw::Time& start, const Fw::Time& now) {
    return (static_cast<I64>(now.getSeconds()) - static_cast<I64>(start.getSeconds())) * 1000 +
           (static_cast<I64>(now.getUSeconds()) - static_cast<I64>(start.getUSeconds())) / 1000;
}

}  // namespace billeeScm

#endif
