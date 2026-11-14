// ViewerMode.cs: decides at start whether the client is a viewer of the EcoWattSense service (SPEC section 9).
// Who uses it: App.xaml.cs at start, the viewer window for the "Service" card, and Test-ViewerMode.ps1.
// How it works: the service control manager (SCM) is asked for the service state with the minimum right a standard
// user has (connect to the SCM, query the status of one service). The answer is mapped to a ServiceState.
// A service that is installed (running or stopped) means viewer mode. Not installed means the client works as before.
#nullable enable
using System;
using System.Runtime.InteropServices;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>The state of the EcoWattSense service as the service control manager reports it.</summary>
public enum ServiceState
{
    /// <summary>The state could not be read (for example, the service control manager refused the query).</summary>
    Unknown,
    /// <summary>The service is not installed on this computer.</summary>
    NotInstalled,
    /// <summary>The service is installed and stopped.</summary>
    Stopped,
    /// <summary>The service is starting.</summary>
    Starting,
    /// <summary>The service is running.</summary>
    Running,
    /// <summary>The service is stopping.</summary>
    Stopping,
    /// <summary>The service is paused or about to change to or from paused.</summary>
    Paused
}

/// <summary>Chooses between the classic mode (own sampling and reports) and the viewer mode.</summary>
public static class ViewerMode
{
    /// <summary>Tells whether the client must work as a viewer of the service.</summary>
    /// <param name="lookup">Gives the service state. A fake in tests, <see cref="ScmServiceLookup.Query()"/> in the app.</param>
    /// <returns>
    /// True when the service is installed (in any state). False when it is not installed, and also when the state is
    /// unknown: a failed query must not switch off the client's own reports, so the classic mode stays the safe default.
    /// </returns>
    public static bool IsViewer(Func<ServiceState> lookup)
    {
        ServiceState state = lookup();
        return state != ServiceState.NotInstalled && state != ServiceState.Unknown;
    }

    /// <summary>Gives the word shown on the Service card.</summary>
    /// <param name="state">The service state.</param>
    /// <returns>A short English word or phrase.</returns>
    public static string Word(ServiceState state) => state switch
    {
        ServiceState.NotInstalled => "Not installed",
        ServiceState.Stopped => "Stopped",
        ServiceState.Starting => "Starting",
        ServiceState.Running => "Running",
        ServiceState.Stopping => "Stopping",
        ServiceState.Paused => "Paused",
        _ => "Unknown"
    };
}

/// <summary>Reads the state of a Windows service through the service control manager (advapi32).</summary>
public static class ScmServiceLookup
{
    /// <summary>The registered name of the EcoWattSense service.</summary>
    public const string ServiceName = "EcoWattSenseService";

    private const uint ScManagerConnect = 0x0001;
    private const uint ServiceQueryStatus = 0x0004;
    private const int ErrorServiceDoesNotExist = 1060;

    [StructLayout(LayoutKind.Sequential)]
    private struct ServiceStatus
    {
        public uint ServiceType;
        public uint CurrentState;
        public uint ControlsAccepted;
        public uint Win32ExitCode;
        public uint ServiceSpecificExitCode;
        public uint CheckPoint;
        public uint WaitHint;
    }

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr OpenSCManagerW(string? machineName, string? databaseName, uint access);

    [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr OpenServiceW(IntPtr manager, string serviceName, uint access);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool QueryServiceStatus(IntPtr service, out ServiceStatus status);

    [DllImport("advapi32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseServiceHandle(IntPtr handle);

    /// <summary>Reads the state of the EcoWattSense service.</summary>
    /// <returns>The state. Never throws.</returns>
    public static ServiceState Query() => Query(ServiceName);

    /// <summary>Reads the state of a service by name.</summary>
    /// <param name="name">The registered service name.</param>
    /// <returns>
    /// The state; NotInstalled only when Windows says the service does not exist; Unknown for every other failure.
    /// Never throws.
    /// </returns>
    public static ServiceState Query(string name)
    {
        IntPtr manager = OpenSCManagerW(null, null, ScManagerConnect);
        if (manager == IntPtr.Zero) return ServiceState.Unknown;
        try { return QueryOpened(manager, name); }
        finally { CloseServiceHandle(manager); }
    }

    /// <summary>Opens one service on an open manager and reads its state.</summary>
    /// <param name="manager">A handle from OpenSCManagerW.</param>
    /// <param name="name">The registered service name.</param>
    /// <returns>The state, as described for <see cref="Query(string)"/>.</returns>
    private static ServiceState QueryOpened(IntPtr manager, string name)
    {
        IntPtr service = OpenServiceW(manager, name, ServiceQueryStatus);
        if (service == IntPtr.Zero) return OpenFailure(Marshal.GetLastWin32Error());
        try { return ReadState(service); }
        finally { CloseServiceHandle(service); }
    }

    /// <summary>Maps the error of a failed OpenServiceW call to a state.</summary>
    /// <param name="error">The Win32 error code.</param>
    /// <returns>NotInstalled for "service does not exist", otherwise Unknown (for example, access denied).</returns>
    private static ServiceState OpenFailure(int error)
        => error == ErrorServiceDoesNotExist ? ServiceState.NotInstalled : ServiceState.Unknown;

    /// <summary>Reads the current state of an open service.</summary>
    /// <param name="service">A handle from OpenServiceW.</param>
    /// <returns>The state, or Unknown when the query fails.</returns>
    private static ServiceState ReadState(IntPtr service)
    {
        if (!QueryServiceStatus(service, out ServiceStatus status)) return ServiceState.Unknown;
        return MapState(status.CurrentState);
    }

    /// <summary>Maps a SERVICE_STATUS current-state number to a ServiceState.</summary>
    /// <param name="current">The dwCurrentState value (1 to 7).</param>
    /// <returns>The matching state; Unknown for a number outside 1 to 7.</returns>
    public static ServiceState MapState(uint current) => current switch
    {
        1 => ServiceState.Stopped,
        2 => ServiceState.Starting,
        3 => ServiceState.Stopping,
        4 => ServiceState.Running,
        5 => ServiceState.Starting,
        6 => ServiceState.Paused,
        7 => ServiceState.Paused,
        _ => ServiceState.Unknown
    };
}
