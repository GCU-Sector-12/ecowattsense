// DisplayControl.cs: switches the display off and on (DisplayPower), measures whether the display really is off (DisplayStateMonitor),
// and asks Windows not to sleep or dim during the test (KeepAwake).
// Who uses it: TestRunner uses DisplayPower in the "Screen off" phase and in the final clean-up, and waits on DisplayStateMonitor.
// SampleCollector stamps every sample with DisplayStateMonitor.Current. MainWindow attaches and detaches the monitor.
// Why the state is measured: a command that "worked" proves nothing. In the test run of 11 Nov 2026 the command was sent and the
// display stayed on. Only the notification GUID_CONSOLE_DISPLAY_STATE says what the display does.
// Why KeepAwake: on battery, Windows normally dims the display and then turns it off after a few minutes without input.
// That would change the screen state in the middle of a phase. The request is per thread and ends with the call to Off or with the program.
// DisplayPower, KeepAwake and DisplayStateMonitor.WaitForOffAsync must be called from the UI thread, because the execution-state
// request belongs to the calling thread and the power notifications arrive on the UI thread.
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace EnergyCalibrator;

/// <summary>What the display is doing, as reported by Windows.</summary>
internal enum DisplayStateKind
{
    /// <summary>No notification has arrived yet, or the registration failed.</summary>
    Unknown,
    /// <summary>The display is off.</summary>
    Off,
    /// <summary>The display is on.</summary>
    On,
    /// <summary>The display is on and dimmed.</summary>
    Dimmed
}

/// <summary>Plain-English texts for the display states. They are also the values of the display_state column in samples.csv.</summary>
internal static class DisplayStateTexts
{
    /// <summary>Gives the short text for a display state.</summary>
    /// <param name="state">The state.</param>
    /// <returns>"off", "on", "dimmed" or "unknown".</returns>
    public static string Describe(DisplayStateKind state) => state switch
    {
        DisplayStateKind.Off => "off",
        DisplayStateKind.On => "on",
        DisplayStateKind.Dimmed => "dimmed",
        _ => "unknown"
    };
}

/// <summary>
/// Keeps the latest display state. Windows reports it with a power setting notification (GUID_CONSOLE_DISPLAY_STATE) that is
/// sent to the main window: once when the registration is made, and again at every change. The state is static because one
/// window owns it and the sampling thread reads it.
/// </summary>
internal static class DisplayStateMonitor
{
    private const int WmPowerBroadcast = 0x0218;
    private const long PbtPowerSettingChange = 0x8013;
    private const uint DeviceNotifyWindowHandle = 0;
    private static readonly Guid ConsoleDisplayState = new("6FE69556-704A-47A0-8F24-C28D936FDA47");
    private static int current = (int)DisplayStateKind.Unknown;
    private static TaskCompletionSource<bool>? offWaiter;
    private static HwndSource? source;
    private static IntPtr registration = IntPtr.Zero;

    /// <summary>The latest state. It can be read from any thread.</summary>
    public static DisplayStateKind Current => (DisplayStateKind)Volatile.Read(ref current);

    /// <summary>Starts to listen: hooks the window procedure of a window and registers for the display state notification.</summary>
    /// <param name="window">The main window. It must already have a handle (call from OnSourceInitialized).</param>
    public static void Attach(Window window)
    {
        IntPtr handle = new WindowInteropHelper(window).Handle;
        source = HwndSource.FromHwnd(handle);
        source?.AddHook(OnMessage);
        Guid setting = ConsoleDisplayState;
        registration = NativeMethods.RegisterPowerSettingNotification(handle, ref setting, DeviceNotifyWindowHandle);
    }

    /// <summary>Stops listening and ends the registration. Safe to call twice.</summary>
    public static void Detach()
    {
        source?.RemoveHook(OnMessage);
        source = null;
        Unregister();
    }

    /// <summary>Waits until the display reports "off", for at most the given time.</summary>
    /// <param name="timeout">How long to wait.</param>
    /// <param name="cancel">Ends the wait early.</param>
    /// <returns>True when the display state is "off" (also when it already was); false after the time-out or the cancel.</returns>
    public static async Task<bool> WaitForOffAsync(TimeSpan timeout, CancellationToken cancel)
    {
        if (Current == DisplayStateKind.Off) { return true; }
        TaskCompletionSource<bool> waiter = new(TaskCreationOptions.RunContinuationsAsynchronously);
        offWaiter = waiter;
        bool reached = await WaitAsync(waiter.Task, timeout, cancel);
        offWaiter = null;
        return reached;
    }

    /// <summary>Waits for a task with a time-out and a cancel token, and turns both ways of giving up into false.</summary>
    /// <param name="task">The task.</param>
    /// <param name="timeout">The time-out.</param>
    /// <param name="cancel">The cancel token.</param>
    /// <returns>True when the task finished in time.</returns>
    private static async Task<bool> WaitAsync(Task task, TimeSpan timeout, CancellationToken cancel)
    {
        try { await task.WaitAsync(timeout, cancel); return true; }
        catch (TimeoutException) { return false; }
        catch (OperationCanceledException) { return false; }
    }

    /// <summary>The window hook: picks the display state out of WM_POWERBROADCAST messages. It never marks a message as handled.</summary>
    /// <param name="hwnd">The window.</param>
    /// <param name="msg">The message number.</param>
    /// <param name="wParam">For WM_POWERBROADCAST: the event, PBT_POWERSETTINGCHANGE for a setting.</param>
    /// <param name="lParam">For PBT_POWERSETTINGCHANGE: a pointer to a POWERBROADCAST_SETTING.</param>
    /// <param name="handled">Not changed, so the other handlers still see the message.</param>
    /// <returns>Always zero.</returns>
    private static IntPtr OnMessage(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg != WmPowerBroadcast) { return IntPtr.Zero; }
        if (wParam.ToInt64() != PbtPowerSettingChange) { return IntPtr.Zero; }
        ReadSetting(lParam);
        return IntPtr.Zero;
    }

    /// <summary>Reads a POWERBROADCAST_SETTING and keeps the value when it is the display state.</summary>
    /// <param name="lParam">Pointer to the structure, owned by Windows for the length of the message.</param>
    private static void ReadSetting(IntPtr lParam)
    {
        NativeMethods.PowerBroadcastSettingHead head = Marshal.PtrToStructure<NativeMethods.PowerBroadcastSettingHead>(lParam);
        if (head.PowerSetting != ConsoleDisplayState) { return; }
        if (head.DataLength < sizeof(int)) { return; }
        int value = Marshal.ReadInt32(lParam, Marshal.SizeOf<NativeMethods.PowerBroadcastSettingHead>());
        Store(FromValue(value));
    }

    /// <summary>Turns the number of the notification into a state.</summary>
    /// <param name="value">0 for off, 1 for on, 2 for dimmed.</param>
    /// <returns>The state; Unknown for any other number.</returns>
    private static DisplayStateKind FromValue(int value) => value switch
    {
        0 => DisplayStateKind.Off,
        1 => DisplayStateKind.On,
        2 => DisplayStateKind.Dimmed,
        _ => DisplayStateKind.Unknown
    };

    /// <summary>Keeps the new state and wakes up a waiter when the display went off.</summary>
    /// <param name="state">The new state.</param>
    private static void Store(DisplayStateKind state)
    {
        Volatile.Write(ref current, (int)state);
        if (state != DisplayStateKind.Off) { return; }
        offWaiter?.TrySetResult(true);
    }

    /// <summary>Ends the registration when there is one.</summary>
    private static void Unregister()
    {
        if (registration == IntPtr.Zero) { return; }
        NativeMethods.UnregisterPowerSettingNotification(registration);
        registration = IntPtr.Zero;
    }
}

/// <summary>Turns the display off and on with the documented SC_MONITORPOWER system command.</summary>
internal static class DisplayPower
{
    private const uint WmSysCommand = 0x0112;
    private const int ScMonitorPower = 0xF170;
    private const int MonitorOff = 2;
    private const int MonitorOn = -1;
    private const uint SmtoAbortIfHung = 0x0002;
    private const uint MouseEventMove = 0x0001;
    private static readonly IntPtr HwndBroadcast = new(0xFFFF);

    /// <summary>
    /// Turns the display off. The command goes to the main window of this program with SendMessage: its default window procedure
    /// (DefWindowProc) handles SC_MONITORPOWER. The program does not touch the mouse or keyboard afterwards.
    /// </summary>
    /// <param name="ownWindow">Handle of the main window of this program.</param>
    public static void TurnOff(IntPtr ownWindow)
    {
        NativeMethods.SendMessage(ownWindow, WmSysCommand, new IntPtr(ScMonitorPower), new IntPtr(MonitorOff));
    }

    /// <summary>Turns the display off by sending the command to all top-level windows. It is only the fall-back for TurnOff.</summary>
    public static void TurnOffBroadcast()
    {
        Broadcast(MonitorOff);
    }

    /// <summary>Turns the display on and nudges the mouse by one pixel and back, so Windows sees user activity and keeps it on.</summary>
    public static void TurnOn()
    {
        Broadcast(MonitorOn);
        NudgeMouse();
    }

    /// <summary>
    /// Sends the monitor-power command to all top-level windows. SendMessageTimeout is used instead of SendMessage,
    /// so one window that does not answer cannot freeze the program with the display off.
    /// </summary>
    /// <param name="state">2 for off, -1 for on.</param>
    private static void Broadcast(int state)
    {
        NativeMethods.SendMessageTimeout(HwndBroadcast, WmSysCommand, new IntPtr(ScMonitorPower), new IntPtr(state), SmtoAbortIfHung, TestSettings.DisplayMessageTimeoutMs, out _);
    }

    /// <summary>Moves the mouse one pixel right and one pixel back.</summary>
    private static void NudgeMouse()
    {
        NativeMethods.mouse_event(MouseEventMove, 1, 0, 0, UIntPtr.Zero);
        NativeMethods.mouse_event(MouseEventMove, -1, 0, 0, UIntPtr.Zero);
    }
}

/// <summary>Asks Windows to keep the system (and the display) awake while the test runs.</summary>
internal static class KeepAwake
{
    private const uint Continuous = 0x80000000;
    private const uint SystemRequired = 0x00000001;
    private const uint DisplayRequired = 0x00000002;

    /// <summary>Keeps the system and the display on until Off is called.</summary>
    public static void On()
    {
        NativeMethods.SetThreadExecutionState(Continuous | SystemRequired | DisplayRequired);
    }

    /// <summary>Keeps only the system awake. Used while the display is off on purpose, so Windows has no reason to switch it on.</summary>
    public static void SystemOnly()
    {
        NativeMethods.SetThreadExecutionState(Continuous | SystemRequired);
    }

    /// <summary>Ends the request, so Windows follows the normal power settings again.</summary>
    public static void Off()
    {
        NativeMethods.SetThreadExecutionState(Continuous);
    }
}
