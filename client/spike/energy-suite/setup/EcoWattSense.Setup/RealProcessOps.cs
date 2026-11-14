// RealProcessOps.cs: the real way Setup starts other programs.
// Who uses it: Program/App, when Setup runs for real. Tests use a fake instead.
// Run: System.Diagnostics.Process with hidden window; standard input carries the server key for ews-service.exe --configure.
// RunAsLoggedOnUser: Setup is elevated, but the calibrator must run as the logged-on user without elevation. No .NET API
// does that, so the token of the desktop shell (explorer.exe) is duplicated and CreateProcessWithTokenW starts the program
// with it. This needs administrator rights (SeImpersonatePrivilege), which Setup has.
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace EcoWattSense.Setup;

/// <summary>The Windows functions used to start a program as the shell user.</summary>
internal static class ShellLaunch
{
    public const uint QueryInformation = 0x0400;
    public const uint TokenDuplicate = 0x0002;
    public const uint TokenAllAccess = 0xF01FF;
    public const int SecurityImpersonation = 2;
    public const int TokenPrimary = 1;
    public const uint Infinite = 0xFFFFFFFF;

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct StartupInfo
    {
        public int Size;
        public string? Reserved;
        public string? Desktop;
        public string? Title;
        public int X, Y, XSize, YSize, XCountChars, YCountChars, FillAttribute, Flags;
        public short ShowWindow, Reserved2;
        public IntPtr Reserved2Pointer, StdInput, StdOutput, StdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ProcessInformation
    {
        public IntPtr Process, Thread;
        public int ProcessId, ThreadId;
    }

    [DllImport("user32.dll")]
    public static extern IntPtr GetShellWindow();

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, [MarshalAs(UnmanagedType.Bool)] bool inherit, uint processId);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool OpenProcessToken(IntPtr process, uint access, out IntPtr token);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DuplicateTokenEx(IntPtr token, uint access, IntPtr attributes, int level, int type, out IntPtr newToken);

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CreateProcessWithTokenW(IntPtr token, uint logonFlags, string application, string? commandLine,
        uint creationFlags, IntPtr environment, string? directory, ref StartupInfo startup, out ProcessInformation information);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetExitCodeProcess(IntPtr process, out uint exitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr handle);
}

/// <summary>Starting programs.</summary>
public sealed class RealProcessOps : IProcessOps
{
    /// <inheritdoc />
    public int Run(string exePath, string arguments, string? standardInput)
    {
        try { return RunAndWait(exePath, arguments, standardInput); }
        catch (Exception) { return -1; }
    }

    private static int RunAndWait(string exePath, string arguments, string? standardInput)
    {
        ProcessStartInfo info = new(exePath, arguments)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardInput = standardInput != null,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        using Process process = Process.Start(info)!;
        Task output = process.StandardOutput.ReadToEndAsync();
        Task errors = process.StandardError.ReadToEndAsync();
        WriteInput(process, standardInput);
        process.WaitForExit();
        Task.WaitAll(output, errors);
        return process.ExitCode;
    }

    private static void WriteInput(Process process, string? standardInput)
    {
        if (standardInput == null) return;
        process.StandardInput.Write(standardInput);
        process.StandardInput.Close();
    }

    /// <inheritdoc />
    public int RunAsLoggedOnUser(string exePath)
    {
        List<IntPtr> handles = [];
        int code = -1;
        try { code = LaunchAndWait(exePath, handles); }
        catch (Exception) { code = -1; }
        finally { CloseAll(handles); }
        return code;
    }

    private static void CloseAll(List<IntPtr> handles)
    {
        foreach (IntPtr handle in handles) ShellLaunch.CloseHandle(handle);
    }

    private static int LaunchAndWait(string exePath, List<IntPtr> handles)
    {
        IntPtr token = DuplicateShellToken(handles);
        if (token == IntPtr.Zero) return -1;
        ShellLaunch.StartupInfo startup = new() { Size = Marshal.SizeOf<ShellLaunch.StartupInfo>() };
        string folder = Path.GetDirectoryName(exePath) ?? "";
        bool started = ShellLaunch.CreateProcessWithTokenW(token, 0, exePath, null, 0, IntPtr.Zero, folder, ref startup, out ShellLaunch.ProcessInformation information);
        if (!started) return -1;
        handles.Add(information.Process);
        handles.Add(information.Thread);
        ShellLaunch.WaitForSingleObject(information.Process, ShellLaunch.Infinite);
        return ShellLaunch.GetExitCodeProcess(information.Process, out uint exitCode) ? (int)exitCode : -1;
    }

    /// <summary>Opens the token of the shell process and duplicates it as a primary token. Returns zero on failure.</summary>
    private static IntPtr DuplicateShellToken(List<IntPtr> handles)
    {
        ShellLaunch.GetWindowThreadProcessId(ShellLaunch.GetShellWindow(), out uint shellId);
        IntPtr process = ShellLaunch.OpenProcess(ShellLaunch.QueryInformation, false, shellId);
        if (process == IntPtr.Zero) return IntPtr.Zero;
        handles.Add(process);
        if (!ShellLaunch.OpenProcessToken(process, ShellLaunch.TokenDuplicate, out IntPtr shellToken)) return IntPtr.Zero;
        handles.Add(shellToken);
        bool copied = ShellLaunch.DuplicateTokenEx(shellToken, ShellLaunch.TokenAllAccess, IntPtr.Zero, ShellLaunch.SecurityImpersonation, ShellLaunch.TokenPrimary, out IntPtr primary);
        handles.Add(primary);
        return copied ? primary : IntPtr.Zero;
    }

    /// <inheritdoc />
    public int StartDetached(string exePath, string arguments)
    {
        try { Process.Start(new ProcessStartInfo(exePath, arguments) { UseShellExecute = false, CreateNoWindow = true })?.Dispose(); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }
}
