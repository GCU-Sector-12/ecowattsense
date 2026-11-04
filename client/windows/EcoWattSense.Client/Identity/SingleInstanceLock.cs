#nullable enable
using System;
using System.IO;

namespace EcoWattSense.Client.Identity;

/// <summary>Thrown when another copy of the client already monitors this computer.</summary>
public sealed class ClientAlreadyRunningException : Exception
{
    /// <summary>Exit code of the console and background exe when another copy is running.</summary>
    public const int ExitCode = 2;

    public ClientAlreadyRunningException(string lockPath, Exception innerException)
        : base("EcoWattSense is already running on this computer. This copy does not start monitoring.", innerException)
    {
        LockPath = lockPath;
    }

    public string LockPath { get; }
}

/// <summary>
/// Makes sure only one copy of the client monitors this computer.
/// The lock is an open file in the device identity folder. Windows closes the file when the
/// process ends, also after a crash, so a new copy can start again.
/// </summary>
/// <remarks>
/// The folder is per computer (%ProgramData%), like the device id. With two Windows users logged in
/// at the same time, only the first user's copy monitors.
///
/// EWS-109 (tray app): today a second copy only exits. Later the second copy can ask the running
/// copy to show its window, for example with a named event or a named pipe next to this lock.
/// Do that after Acquire throws ClientAlreadyRunningException. It is not built yet.
/// </remarks>
public sealed class SingleInstanceLock : IDisposable
{
    public const string FileName = "instance.lock";

    // ERROR_SHARING_VIOLATION as an HRESULT: another process has the file open.
    private const int SharingViolation = unchecked((int)0x80070020);

    private FileStream? stream;

    private SingleInstanceLock(FileStream stream, string path)
    {
        this.stream = stream;
        FilePath = path;
    }

    public string FilePath { get; }

    public static SingleInstanceLock Acquire(string directory)
    {
        string path = Path.Combine(directory, FileName);
        try
        {
            // The folder does not exist yet on a new computer. If it cannot be made, that is an error.
            Directory.CreateDirectory(directory);
            // Read access only: a lock file made by another Windows user is read-only for us.
            // FileShare.None makes every other open of this file fail while we hold it.
            var stream = new FileStream(path, FileMode.OpenOrCreate, FileAccess.Read, FileShare.None);
            return new SingleInstanceLock(stream, path);
        }
        catch (IOException exception) when (exception.HResult == SharingViolation)
        {
            // Only this error means "already running". Other errors must not be shown as that.
            throw new ClientAlreadyRunningException(path, exception);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            throw new InvalidOperationException(
                $"Unable to take the single-instance lock at '{path}'. " +
                "Check that this folder exists and that you can read it. Monitoring has not started.",
                exception);
        }
    }

    public void Dispose()
    {
        // The file stays on disk. Deleting it could remove a lock that another copy just took.
        stream?.Dispose();
        stream = null;
    }
}
