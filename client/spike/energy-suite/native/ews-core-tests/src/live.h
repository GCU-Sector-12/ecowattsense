// live.h: the --live mode of ews-core-tests, which prints values from the real readers of this computer.
#pragma once

namespace ews_test {

/// Prints one line for each real reader: processor load over 1 second, each battery, the on-battery flag, the machine facts.
/// @return Always 0; a failed reader prints its status instead.
int RunLive();

}  // namespace ews_test
