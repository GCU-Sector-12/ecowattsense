// ProgramSwap.cs: the two safe-copy helpers of Setup (SPEC section 10).
// Who uses it: InstallEngine (install and /upgrade) and MaintenanceEngine (/settings, /uninstall).
// ServiceStop: stops the service and waits until its process has really exited. The service control manager reports
// STOPPED before the process has released ews-service.exe, and a copy right after that fails with a sharing violation.
// ProgramSwap: replaces the program folder as a whole. The package is copied to "<folder>.new"; only a complete copy is
// swapped in, so a failed copy never leaves a folder that mixes old and new files.
namespace EcoWattSense.Setup;

/// <summary>Stops the service and waits for its process to exit.</summary>
public static class ServiceStop
{
    private const int ExitWaitMilliseconds = 30000;
    private const int ErrorTimeout = 258;

    /// <summary>
    /// Reads the service process id first (QueryServiceStatusEx), stops the service, then waits up to 30 seconds for that
    /// process to exit. When the process stays, the service is started again (best effort) and the step fails.
    /// </summary>
    /// <param name="services">The operations.</param>
    /// <param name="log">The install log; gets one line "wait_service_exit".</param>
    /// <returns>0, the error code of the stop, or 258 (timeout) when the process did not exit.</returns>
    public static int Run(SetupServices services, SetupLog log)
    {
        int processId = services.Service.ProcessId();
        int code = services.Service.Stop();
        if (code != 0) return code;
        bool gone = processId == 0 || services.Processes.WaitForExit(processId, ExitWaitMilliseconds);
        log.Line("wait_service_exit", gone ? "ok" : "timeout", gone ? 0 : ErrorTimeout);
        if (!gone) services.Service.Start();
        return gone ? 0 : ErrorTimeout;
    }
}

/// <summary>Copies the package next to the program folder and swaps the folders by renaming.</summary>
public sealed class ProgramSwap
{
    private const int SharingViolation = 32;
    private const int MoveRetries = 5;
    private const int DeleteRetries = 3;

    private readonly SetupServices _services;
    private readonly SetupLayout _layout;
    private readonly SetupLog _log;

    /// <summary>Creates the helper.</summary>
    /// <param name="services">The operations.</param>
    /// <param name="layout">The folders.</param>
    /// <param name="log">The install log; gets copy_new, swap and remove_old lines.</param>
    public ProgramSwap(SetupServices services, SetupLayout layout, SetupLog log)
    {
        _services = services;
        _layout = layout;
        _log = log;
    }

    private string NewDir => _layout.ProgramDir + ".new";

    private string OldDir => _layout.ProgramDir + ".old";

    /// <summary>
    /// Copies the package to ".new", renames the current folder to ".old" (retrying a sharing violation 5 times, 1 second
    /// apart), renames ".new" to the program folder and deletes ".old". A fresh install only renames ".new". A failure
    /// before the swap leaves the current folder as it was and removes ".new".
    /// </summary>
    /// <param name="packageDir">The folder to copy.</param>
    /// <param name="copied">Gets the number of files copied so far; may be null.</param>
    /// <returns>0 or an error code.</returns>
    public int Run(string packageDir, IProgress<int>? copied)
    {
        DeleteIfThere(NewDir);
        int code = _services.Files.CopyDirectory(packageDir, NewDir, copied);
        _log.Line("copy_new", code == 0 ? "ok" : "failed", code);
        if (code != 0) DeleteIfThere(NewDir);
        if (code != 0) return code;
        code = Swap();
        if (code != 0) DeleteIfThere(NewDir);
        if (code == 0) RemoveOld();
        return code;
    }

    /// <summary>
    /// Undoes the swap. A fresh install: deletes the program folder. Otherwise, when ".old" still exists, the current
    /// (new) folder is renamed away and ".old" is renamed back. ".new" is deleted in every case.
    /// </summary>
    /// <param name="existedBefore">True when the program folder existed before the copy.</param>
    public void Undo(bool existedBefore)
    {
        if (!existedBefore) _services.Files.DeleteDirectory(_layout.ProgramDir);
        if (existedBefore && _services.Files.DirectoryExists(OldDir)) RestoreOld();
        DeleteIfThere(NewDir);
    }

    private int Swap()
    {
        bool hasCurrent = _services.Files.DirectoryExists(_layout.ProgramDir);
        if (!hasCurrent) return LogSwap(_services.Files.MoveDirectory(NewDir, _layout.ProgramDir));
        DeleteIfThere(OldDir);
        int code = MoveWithRetry(_layout.ProgramDir, OldDir);
        if (code != 0) return LogSwap(code);
        code = _services.Files.MoveDirectory(NewDir, _layout.ProgramDir);
        if (code != 0) _services.Files.MoveDirectory(OldDir, _layout.ProgramDir);
        return LogSwap(code);
    }

    private int LogSwap(int code)
    {
        _log.Line("swap", code == 0 ? "ok" : "failed", code);
        return code;
    }

    private int MoveWithRetry(string source, string target)
    {
        int code = _services.Files.MoveDirectory(source, target);
        for (int retry = 0; retry < MoveRetries && code == SharingViolation; retry++)
        {
            _services.Processes.Wait(1000);
            code = _services.Files.MoveDirectory(source, target);
        }
        return code;
    }

    /// <summary>Deletes ".old" (3 retries on a sharing violation); a folder that stays is marked for deletion at the next restart. Never fails the step.</summary>
    private void RemoveOld()
    {
        if (!_services.Files.DirectoryExists(OldDir)) return;
        int code = _services.Files.DeleteDirectory(OldDir);
        for (int retry = 0; retry < DeleteRetries && code == SharingViolation; retry++)
        {
            _services.Processes.Wait(1000);
            code = _services.Files.DeleteDirectory(OldDir);
        }
        if (code == 0) _log.Line("remove_old", "ok", 0);
        if (code == 0) return;
        int marked = _services.Files.DeleteAtReboot(OldDir);
        _log.Line("remove_old", marked == 0 ? "at_reboot" : "failed", marked == 0 ? 0 : code);
    }

    private void RestoreOld()
    {
        int moved = _services.Files.MoveDirectory(_layout.ProgramDir, NewDir);
        int back = moved == 0 ? _services.Files.MoveDirectory(OldDir, _layout.ProgramDir) : moved;
        _log.Line("swap_back", back == 0 ? "ok" : "failed", back);
    }

    private void DeleteIfThere(string path)
    {
        if (_services.Files.DirectoryExists(path)) _services.Files.DeleteDirectory(path);
    }
}
