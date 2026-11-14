// RealFilesAclTask.cs: the real Windows implementations of the file, folder-rights, task and system-info interfaces.
// Who uses it: Program/App, when Setup runs for real. Tests never use these classes.
// System APIs used: System.IO for files, System.Security.AccessControl (ACL classes) for rights, the Task Scheduler 2.0
// COM object (Schedule.Service) for the task, and WindowsPrincipal for the administrator check.
// Every method catches its own exceptions and returns a Windows error code, so callers need no try blocks.
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;

namespace EcoWattSense.Setup;

/// <summary>Turns an exception into the error code the interfaces return.</summary>
internal static class ErrorCodes
{
    private const int ErrorFileNotFound = 2;
    private const int ErrorPathNotFound = 3;

    /// <summary>The low 16 bits of the HRESULT, or 1 when that is zero.</summary>
    public static int From(Exception exception)
    {
        int code = exception.HResult & 0xFFFF;
        return code == 0 ? 1 : code;
    }

    /// <summary>True for "file not found" and "path not found".</summary>
    public static bool IsMissing(Exception exception) => From(exception) is ErrorFileNotFound or ErrorPathNotFound;
}

/// <summary>File operations with System.IO.</summary>
public sealed class RealFileOps : IFileOps
{
    /// <inheritdoc />
    public bool DirectoryExists(string path) => Directory.Exists(path);

    /// <inheritdoc />
    public bool FileExists(string path) => File.Exists(path);

    /// <inheritdoc />
    public int CreateDirectory(string path)
    {
        try { Directory.CreateDirectory(path); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    /// <inheritdoc />
    public int CountFiles(string path)
    {
        try { return Directory.GetFiles(path, "*", SearchOption.AllDirectories).Length; }
        catch (Exception) { return 0; }
    }

    /// <inheritdoc />
    public int CopyDirectory(string source, string target, IProgress<int>? copied = null)
    {
        int count = 0;
        try { CopyTree(source, target, copied, ref count); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private static void CopyTree(string source, string target, IProgress<int>? copied, ref int count)
    {
        Directory.CreateDirectory(target);
        foreach (string file in Directory.GetFiles(source))
        {
            File.Copy(file, Path.Combine(target, Path.GetFileName(file)), true);
            count++;
            copied?.Report(count);
        }
        foreach (string folder in Directory.GetDirectories(source)) CopyTree(folder, Path.Combine(target, Path.GetFileName(folder)), copied, ref count);
    }

    /// <inheritdoc />
    public int DeleteDirectory(string path)
    {
        if (!Directory.Exists(path)) return 0;
        try { Directory.Delete(path, true); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    /// <inheritdoc />
    public int MoveDirectory(string source, string target)
    {
        try { Directory.Move(source, target); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private const int MoveFileDelayUntilReboot = 0x4;

    [System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
    private static extern bool MoveFileEx(string existing, string? target, int flags);

    /// <inheritdoc />
    public int DeleteAtReboot(string path)
    {
        bool marked = MoveFileEx(path, null, MoveFileDelayUntilReboot);
        return marked ? 0 : System.Runtime.InteropServices.Marshal.GetLastWin32Error();
    }

    /// <inheritdoc />
    public int DeleteFile(string path)
    {
        try { File.Delete(path); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    /// <inheritdoc />
    public string? ReadText(string path)
    {
        if (!File.Exists(path)) return null;
        try { return File.ReadAllText(path); }
        catch (Exception) { return null; }
    }

    /// <inheritdoc />
    public int WriteText(string path, string text)
    {
        try { WriteReplacing(path, text); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private static void WriteReplacing(string path, string text)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        string temporary = path + ".tmp";
        File.WriteAllText(temporary, text);
        File.Move(temporary, path, true);
    }

    /// <inheritdoc />
    public int AppendLine(string path, string line)
    {
        try { Directory.CreateDirectory(Path.GetDirectoryName(path)!); File.AppendAllText(path, line + "\n"); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }
}

/// <summary>Folder rights with DirectorySecurity.</summary>
public sealed class RealAclOps : IAclOps
{
    /// <inheritdoc />
    public int Apply(string path, AclMode mode, IReadOnlyList<AclEntry> entries)
    {
        try { Edit(path, mode, entries); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private static void Edit(string path, AclMode mode, IReadOnlyList<AclEntry> entries)
    {
        DirectoryInfo folder = new(path);
        DirectorySecurity security = folder.GetAccessControl();
        if (mode == AclMode.Reset) ClearRules(security);
        foreach (AclEntry entry in entries) security.AddAccessRule(ToRule(entry));
        folder.SetAccessControl(security);
    }

    /// <summary>Stops inheritance without copying the old rules, then removes the explicit rules.</summary>
    private static void ClearRules(DirectorySecurity security)
    {
        security.SetAccessRuleProtection(true, false);
        foreach (FileSystemAccessRule rule in security.GetAccessRules(true, false, typeof(SecurityIdentifier))) security.RemoveAccessRuleSpecific(rule);
    }

    private static FileSystemAccessRule ToRule(AclEntry entry)
    {
        SecurityIdentifier sid = new(entry.Sid);
        bool inherit = entry.Right != AccessRight.Traverse;
        InheritanceFlags flags = inherit ? InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit : InheritanceFlags.None;
        return new FileSystemAccessRule(sid, ToRights(entry.Right), flags, PropagationFlags.None, AccessControlType.Allow);
    }

    private static FileSystemRights ToRights(AccessRight right) => right switch
    {
        AccessRight.Full => FileSystemRights.FullControl,
        AccessRight.Modify => FileSystemRights.Modify,
        _ => FileSystemRights.ReadAndExecute,
    };
}

/// <summary>The Task Scheduler through the Schedule.Service COM object.</summary>
public sealed class RealTaskOps : ITaskOps
{
    private const int TaskCreateOrUpdate = 6;
    private const int TaskLogonNone = 0;

    /// <inheritdoc />
    public int Register(string name, string xml)
    {
        try { RegisterCore(name, xml); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private static void RegisterCore(string name, string xml)
    {
        dynamic folder = OpenRootFolder();
        folder.RegisterTask(name, xml, TaskCreateOrUpdate, null, null, TaskLogonNone, null);
    }

    /// <inheritdoc />
    public int Delete(string name)
    {
        try { OpenRootFolder().DeleteTask(name, 0); return 0; }
        catch (Exception exception) { return ErrorCodes.IsMissing(exception) ? 0 : ErrorCodes.From(exception); }
    }

    /// <inheritdoc />
    public bool Exists(string name)
    {
        try { OpenRootFolder().GetTask(name); return true; }
        catch (Exception) { return false; }
    }

    private static dynamic OpenRootFolder()
    {
        Type type = Type.GetTypeFromProgID("Schedule.Service")!;
        dynamic service = Activator.CreateInstance(type)!;
        service.Connect();
        return service.GetFolder("\\");
    }
}

/// <summary>Facts about the computer.</summary>
public sealed class RealSystemInfo : ISystemInfo
{
    private const int FirstWindows11Build = 22000;

    /// <inheritdoc />
    public bool IsWindows11X64() =>
        Environment.OSVersion.Version.Build >= FirstWindows11Build && RuntimeInformation.OSArchitecture == Architecture.X64;

    /// <inheritdoc />
    public bool IsAdministrator()
    {
        using WindowsIdentity identity = WindowsIdentity.GetCurrent();
        return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
    }

    /// <inheritdoc />
    public string NowUtcText() => DateTime.UtcNow.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'");
}
