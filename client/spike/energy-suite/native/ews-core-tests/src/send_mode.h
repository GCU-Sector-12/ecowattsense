// send_mode.h: the --send mode of ews-core-tests, used by scripts/Test-Sender.ps1 to test the WinHTTP sender live.
#pragma once

namespace ews_test {

/// Sends a batch of two small JSON lines with the key `test-key` to `<baseUrl>/api/reports` and prints one line:
/// `<result word> leave=<0 or 1> rejected=<count>`. Nothing else is printed: no URL, no key, no answer text.
/// @param baseUrl The server URL, for example `http://localhost:49200`.
/// @return 0 always; the result is in the printed line. 2 when the URL is not a valid server URL.
int RunSend(const char* baseUrl);

}  // namespace ews_test
