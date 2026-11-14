// RealServiceOps.cs: the real service control of Setup.
// Who uses it: Program/App, when Setup runs for real. Tests use a fake instead.
// System APIs used: ServiceController for status, start and stop. advapi32 for what .NET lacks: CreateService,
// ChangeServiceConfig, ChangeServiceConfig2 (delayed automatic start, recovery actions, service SID type) and DeleteService.
// Every method returns a Windows error code and none throws.
using System.Runtime.InteropServices;
using System.ServiceProcess;

namespace EcoWattSense.Setup;

/// <summary>The advapi32 functions and structures used to register the service.</summary>
internal static class Advapi
{
    public const uint ScManagerAllAccess = 0xF003F;
    public const uint ServiceAllAccess = 0xF01FF;
    public const uint ServiceQueryStatus = 0x0004;
    public const uint Delete = 0x10000;
    public const uint OwnProcess = 0x10;
    public const uint AutoStart = 2;
    public const uint ErrorNormal = 1;
    public const uint NoChange = 0xFFFFFFFF;
    public const int ErrorServiceDoesNotExist = 1060;

    public const uint ConfigDescription = 1;
    public const uint ConfigFailureActions = 2;
    public const uint ConfigDelayedAutoStart = 3;
    public const uint ConfigServiceSid = 5;
    public const uint SidTypeUnrestricted = 1;
    public const int ActionRestart = 1;

    [StructLayout(LayoutKind.Sequential)]
    public struct DelayedAutoStartInfo { public int Delayed; }

    [StructLayout(LayoutKind.Sequential)]
    public struct ServiceSidInfo { public uint SidType; }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DescriptionInfo { [MarshalAs(UnmanagedType.LPWStr)] public string Text; }

    [StructLayout(LayoutKind.Sequential)]
    public struct ScAction { public int Type; public uint DelayMs; }

    [StructLayout(LayoutKind.Sequential)]
    public struct FailureActions
    {
        public uint ResetPeriod;
        public IntPtr RebootMessage;
        public IntPtr Command;
        public uint ActionCount;
        public IntPtr Actions;
    }

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr OpenSCManagerW(string? machine, string? database, uint access);

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr OpenServiceW(IntPtr manager, string name, uint access);

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateServiceW(IntPtr manager, string name, string display, uint access, uint type, uint start,
        uint errorControl, string binaryPath, string? group, IntPtr tag, string? dependencies, string? account, string? password);

    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ChangeServiceConfigW(IntPtr service, uint type, uint start, uint errorControl, string? binaryPath,
        string? group, IntPtr tag, string? dependencies, string? account, string? password, string? display);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ChangeServiceConfig2W(IntPtr service, uint level, IntPtr info);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DeleteService(IntPtr service);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseServiceHandle(IntPtr handle);
}

/// <summary>The service operations of Setup.</summary>
public sealed class RealServiceOps : IServiceOps
{
    private const int StopWaitSeconds = 30;

    /// <inheritdoc />
    public bool Exists()
    {
        IntPtr manager = Advapi.OpenSCManagerW(null, null, 1);
        if (manager == IntPtr.Zero) return false;
        IntPtr service = Advapi.OpenServiceW(manager, SetupLayout.ServiceName, Advapi.ServiceQueryStatus);
        bool found = service != IntPtr.Zero;
        CloseIfOpen(service);
        Advapi.CloseServiceHandle(manager);
        return found;
    }

    /// <inheritdoc />
    public bool IsRunning()
    {
        try { return ReadRunning(); }
        catch (Exception) { return false; }
    }

    private static bool ReadRunning()
    {
        using ServiceController controller = new(SetupLayout.ServiceName);
        return controller.Status is ServiceControllerStatus.Running or ServiceControllerStatus.StartPending;
    }

    /// <inheritdoc />
    public int Stop()
    {
        try { StopAndWait(); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private static void StopAndWait()
    {
        using ServiceController controller = new(SetupLayout.ServiceName);
        if (controller.Status == ServiceControllerStatus.Stopped) return;
        controller.Stop();
        controller.WaitForStatus(ServiceControllerStatus.Stopped, TimeSpan.FromSeconds(StopWaitSeconds));
    }

    /// <inheritdoc />
    public int Start()
    {
        try { StartAndWait(); return 0; }
        catch (Exception exception) { return ErrorCodes.From(exception); }
    }

    private static void StartAndWait()
    {
        using ServiceController controller = new(SetupLayout.ServiceName);
        controller.Start();
        controller.WaitForStatus(ServiceControllerStatus.Running, TimeSpan.FromSeconds(StopWaitSeconds));
    }

    /// <inheritdoc />
    public int Register(ServiceSpec spec)
    {
        IntPtr manager = Advapi.OpenSCManagerW(null, null, Advapi.ScManagerAllAccess);
        if (manager == IntPtr.Zero) return Marshal.GetLastWin32Error();
        int code = RegisterIn(manager, spec);
        Advapi.CloseServiceHandle(manager);
        return code;
    }

    private static int RegisterIn(IntPtr manager, ServiceSpec spec)
    {
        IntPtr service = Advapi.OpenServiceW(manager, spec.Name, Advapi.ServiceAllAccess);
        int code = service != IntPtr.Zero ? ChangeExisting(service, spec) : CreateNew(manager, spec, out service);
        code = code != 0 ? code : ConfigureDetails(service, spec);
        CloseIfOpen(service);
        return code;
    }

    private static int ChangeExisting(IntPtr service, ServiceSpec spec)
    {
        bool done = Advapi.ChangeServiceConfigW(service, Advapi.NoChange, Advapi.AutoStart, Advapi.NoChange, Quote(spec.BinaryPath),
            null, IntPtr.Zero, null, spec.Account, null, spec.DisplayName);
        return done ? 0 : Marshal.GetLastWin32Error();
    }

    private static int CreateNew(IntPtr manager, ServiceSpec spec, out IntPtr service)
    {
        service = Advapi.CreateServiceW(manager, spec.Name, spec.DisplayName, Advapi.ServiceAllAccess, Advapi.OwnProcess, Advapi.AutoStart,
            Advapi.ErrorNormal, Quote(spec.BinaryPath), null, IntPtr.Zero, null, spec.Account, null);
        return service != IntPtr.Zero ? 0 : Marshal.GetLastWin32Error();
    }

    private static string Quote(string path) => "\"" + path + "\"";

    /// <summary>Delayed start, recovery (restart after 60 seconds, three times), service SID type, description.</summary>
    private static int ConfigureDetails(IntPtr service, ServiceSpec spec)
    {
        int code = SetInfo(service, Advapi.ConfigDelayedAutoStart, new Advapi.DelayedAutoStartInfo { Delayed = 1 });
        code = code != 0 ? code : SetRecovery(service);
        code = code != 0 ? code : SetInfo(service, Advapi.ConfigServiceSid, new Advapi.ServiceSidInfo { SidType = Advapi.SidTypeUnrestricted });
        return code != 0 ? code : SetInfo(service, Advapi.ConfigDescription, new Advapi.DescriptionInfo { Text = spec.Description });
    }

    private static int SetRecovery(IntPtr service)
    {
        Advapi.ScAction restart = new() { Type = Advapi.ActionRestart, DelayMs = 60000 };
        IntPtr actions = Marshal.AllocHGlobal(Marshal.SizeOf<Advapi.ScAction>() * 3);
        for (int index = 0; index < 3; index++) Marshal.StructureToPtr(restart, actions + index * Marshal.SizeOf<Advapi.ScAction>(), false);
        Advapi.FailureActions info = new() { ResetPeriod = 86400, ActionCount = 3, Actions = actions };
        int code = SetInfo(service, Advapi.ConfigFailureActions, info);
        Marshal.FreeHGlobal(actions);
        return code;
    }

    private static int SetInfo<T>(IntPtr service, uint level, T info) where T : struct
    {
        IntPtr block = Marshal.AllocHGlobal(Marshal.SizeOf<T>());
        Marshal.StructureToPtr(info, block, false);
        bool done = Advapi.ChangeServiceConfig2W(service, level, block);
        int code = done ? 0 : Marshal.GetLastWin32Error();
        Marshal.DestroyStructure<T>(block);
        Marshal.FreeHGlobal(block);
        return code;
    }

    /// <inheritdoc />
    public int Delete()
    {
        IntPtr manager = Advapi.OpenSCManagerW(null, null, Advapi.ScManagerAllAccess);
        if (manager == IntPtr.Zero) return Marshal.GetLastWin32Error();
        int code = DeleteIn(manager);
        Advapi.CloseServiceHandle(manager);
        return code;
    }

    private static int DeleteIn(IntPtr manager)
    {
        IntPtr service = Advapi.OpenServiceW(manager, SetupLayout.ServiceName, Advapi.Delete);
        if (service == IntPtr.Zero) return Marshal.GetLastWin32Error() == Advapi.ErrorServiceDoesNotExist ? 0 : Marshal.GetLastWin32Error();
        int code = Advapi.DeleteService(service) ? 0 : Marshal.GetLastWin32Error();
        Advapi.CloseServiceHandle(service);
        return code;
    }

    private static void CloseIfOpen(IntPtr handle)
    {
        if (handle != IntPtr.Zero) Advapi.CloseServiceHandle(handle);
    }
}
