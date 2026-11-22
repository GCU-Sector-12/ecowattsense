// watch_run.h: the watch mode of ews-scan.exe (SPEC 1.6): prints JSON lines to standard output until standard input ends.
// Used by main.cpp. The text of the lines and the reading of the sources are in ews-core (watch_session); this part owns
// the Windows pieces: the waitable timer, the wait on the end of standard input, the monotonic clock and the output.
#pragma once

#include <cstdint>

namespace ews_scan {

/// Runs the watch until standard input reaches its end (the parent closed the pipe or died) or standard output cannot
/// be written. Writes no file. One waitable timer sets the period and one wait watches the end of input: no busy loop.
/// @param periodMs The period in milliseconds (500..10000, checked by the argument parser).
/// @return 0 when the watch ended because the input ended or the output closed; 1 when the timer or the wait failed.
int RunWatch(std::uint32_t periodMs);

}  // namespace ews_scan
