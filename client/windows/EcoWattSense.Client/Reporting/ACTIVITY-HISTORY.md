# Every-check JSON history

Worker appends each observation immediately (normally every five seconds) to one
valid JSON array: `Reports/activity-history.json` beside the running executable/DLL.
The console prints the absolute path at startup. With Rider's normal Debug build,
this is `bin/Debug/net10.0/Reports/activity-history.json`. With a published EXE, it is
the Reports folder beside that EXE. The file remains after shutdown; a restart appends
to it. It does not wait for the 300-second server reporting interval.

Use `--History:Directory="C:\absolute\folder"` to choose another visible writable
folder. A read-only installation directory requires this override. Write failures
are logged as ACTIVITY HISTORY NOT SAVED. No permission changes or fallback folders
are used. Existing malformed JSON is preserved and reported.

Each array item includes UTC timestamp, persistent device UUID, device type, OS,
fractional inactivity, CPU percentage, numeric state and English state name, screen
lock and battery status. Unknown readings are JSON null. CPU here describes the
latest sample, not the longer server reporting interval. This local observation
history has its own format; it is not an API report or an upload queue.

The file is saved by replacing it with a flushed complete JSON array. A lock file
prevents concurrent updates; don't run two clients against the same history path.
For this small tutor demo the full array is rewritten each observation; there is no
retention limit. Archive large files manually with the application stopped.
No observations before this feature are invented or reconstructed.

Open the JSON in Notepad after stopping the client, or reopen it to see updates.
An old published EXE must be republished before it can record this new history.
