// Interfaces.cs: the small interfaces behind every operation Setup does on the computer, and their data types.
// Who uses it: the install and maintenance engines call only these interfaces. The Real* classes implement them for
// Windows; the console tests (setup\tests\SetupTests) implement them with fakes, so no test touches the real machine.
// Every operation returns 0 on success or a Windows error code; none throws, so the engines stay free of try blocks.
namespace EcoWattSense.Setup;

/// <summary>The right a folder entry grants (SPEC 2.2).</summary>
public enum AccessRight
{
    /// <summary>List and traverse only, no inheritance.</summary>
    Traverse,
    /// <summary>Read, inherited by sub-folders and files.</summary>
    Read,
    /// <summary>Modify, inherited by sub-folders and files.</summary>
    Modify,
    /// <summary>Full control, inherited by sub-folders and files.</summary>
    Full,
}

/// <summary>How an ACL plan is applied to a folder.</summary>
public enum AclMode
{
    /// <summary>Remove inheritance and every old entry, then add exactly the given ones.</summary>
    Reset,
    /// <summary>Keep the old entries and add the given ones.</summary>
    Add,
}

/// <summary>One access entry: a SID (text form) and a right.</summary>
/// <param name="Sid">The SID, for example S-1-5-18.</param>
/// <param name="Right">The right granted.</param>
public sealed record AclEntry(string Sid, AccessRight Right);

/// <summary>What the service registration needs (SPEC 5.1).</summary>
/// <param name="Name">Service name.</param>
/// <param name="DisplayName">Name shown in the Services list.</param>
/// <param name="BinaryPath">Full path of ews-service.exe.</param>
/// <param name="Account">Logon account, for example NT AUTHORITY\LocalService.</param>
/// <param name="Description">Text for the description field.</param>
public sealed record ServiceSpec(string Name, string DisplayName, string BinaryPath, string Account, string Description);

/// <summary>File and folder operations.</summary>
public interface IFileOps
{
    /// <summary>Tells whether a folder exists.</summary>
    bool DirectoryExists(string path);

    /// <summary>Tells whether a file exists.</summary>
    bool FileExists(string path);

    /// <summary>Creates a folder with its parents; an existing folder is fine. Returns 0 or an error code.</summary>
    int CreateDirectory(string path);

    /// <summary>Copies a whole folder tree over another one, replacing files. Returns 0 or an error code.</summary>
    int CopyDirectory(string source, string target);

    /// <summary>Deletes a folder tree; a missing folder is fine. Returns 0 or an error code.</summary>
    int DeleteDirectory(string path);

    /// <summary>Reads a text file, or returns null when it cannot be read.</summary>
    string? ReadText(string path);

    /// <summary>Writes a text file, replacing it. Returns 0 or an error code.</summary>
    int WriteText(string path, string text);

    /// <summary>Deletes a file; a missing file is fine. Returns 0 or an error code.</summary>
    int DeleteFile(string path);

    /// <summary>Appends one line to a text file and creates the folder when missing. Returns 0 or an error code.</summary>
    int AppendLine(string path, string line);
}

/// <summary>Folder rights through the ACL classes.</summary>
public interface IAclOps
{
    /// <summary>Applies the entries to a folder in the given mode. Returns 0 or an error code.</summary>
    int Apply(string path, AclMode mode, IReadOnlyList<AclEntry> entries);
}

/// <summary>The Windows service.</summary>
public interface IServiceOps
{
    /// <summary>Tells whether the service is registered.</summary>
    bool Exists();

    /// <summary>Tells whether the service is running or starting.</summary>
    bool IsRunning();

    /// <summary>Stops the service and waits up to 30 seconds. Returns 0 or an error code.</summary>
    int Stop();

    /// <summary>Starts the service. Returns 0 or an error code.</summary>
    int Start();

    /// <summary>
    /// Creates the service, or updates it when it exists: delayed automatic start, recovery restart after 60 seconds
    /// three times, service SID type unrestricted. Returns 0 or an error code.
    /// </summary>
    int Register(ServiceSpec spec);

    /// <summary>Deletes the service; a missing service is fine. Returns 0 or an error code.</summary>
    int Delete();
}

/// <summary>The Task Scheduler.</summary>
public interface ITaskOps
{
    /// <summary>Registers (creates or replaces) a task from its XML. Returns 0 or an error code.</summary>
    int Register(string name, string xml);

    /// <summary>Deletes a task; a missing task is fine. Returns 0 or an error code.</summary>
    int Delete(string name);

    /// <summary>Tells whether a task with this name exists.</summary>
    bool Exists(string name);
}

/// <summary>Starting programs.</summary>
public interface IProcessOps
{
    /// <summary>
    /// Runs a program hidden and waits. The standard input text, when given, is written to its standard input.
    /// Returns the exit code, or -1 when the program could not start.
    /// </summary>
    int Run(string exePath, string arguments, string? standardInput);

    /// <summary>Runs a program as the logged-on user, not elevated, and waits. Returns its exit code or -1.</summary>
    int RunAsLoggedOnUser(string exePath);

    /// <summary>Starts a program and does not wait. Returns 0 or an error code.</summary>
    int StartDetached(string exePath, string arguments);
}

/// <summary>Facts about the computer.</summary>
public interface ISystemInfo
{
    /// <summary>Tells whether this is Windows 11 on x64.</summary>
    bool IsWindows11X64();

    /// <summary>Tells whether the process runs with administrator rights.</summary>
    bool IsAdministrator();

    /// <summary>The current UTC time as text for the log, for example 2026-11-14T10:00:00Z.</summary>
    string NowUtcText();
}

/// <summary>All the operation interfaces in one place.</summary>
/// <param name="Files">File operations.</param>
/// <param name="Acls">Folder rights.</param>
/// <param name="Service">The service.</param>
/// <param name="Tasks">The scheduled task.</param>
/// <param name="Processes">Starting programs.</param>
/// <param name="System">Facts about the computer.</param>
public sealed record SetupServices(
    IFileOps Files, IAclOps Acls, IServiceOps Service, ITaskOps Tasks, IProcessOps Processes, ISystemInfo System);
