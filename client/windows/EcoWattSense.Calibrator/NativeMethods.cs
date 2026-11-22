// NativeMethods.cs: the documented Win32 functions that the program calls (kernel32 and user32).
// Who uses it: CpuTimesReader (GetSystemTimes), PowerStatusReader (GetSystemPowerStatus), UserInputReader (GetLastInputInfo),
// KeepAwake (SetThreadExecutionState), DisplayPower (SendMessage, SendMessageTimeout, mouse_event) and
// DisplayStateMonitor (RegisterPowerSettingNotification, UnregisterPowerSettingNotification).
// The battery IOCTL calls have their own declarations in BatteryIoctlReader.cs.
// The struct fields below are filled by Windows, so the compiler cannot see a write to them (warning CS0649 is switched off).
#pragma warning disable CS0649
using System.Runtime.InteropServices;

namespace EnergyCalibrator;

/// <summary>P/Invoke declarations. Every function here is documented by Microsoft.</summary>
internal static class NativeMethods
{
    /// <summary>The FILETIME structure: a 64-bit count split in two halves.</summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct FileTime
    {
        /// <summary>Low 32 bits.</summary>
        public uint Low;

        /// <summary>High 32 bits.</summary>
        public uint High;

        /// <summary>Joins the two halves.</summary>
        /// <returns>The 64-bit value in 100 ns units.</returns>
        public readonly ulong ToUInt64() => ((ulong)High << 32) | Low;
    }

    /// <summary>The SYSTEM_POWER_STATUS structure.</summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct SystemPowerStatus
    {
        /// <summary>0 offline, 1 online, 255 unknown.</summary>
        public byte AcLineStatus;

        /// <summary>Battery flags.</summary>
        public byte BatteryFlag;

        /// <summary>Charge 0 to 100, or 255 when unknown.</summary>
        public byte BatteryLifePercent;

        /// <summary>Status flags.</summary>
        public byte SystemStatusFlag;

        /// <summary>Seconds of life left, or 0xFFFFFFFF.</summary>
        public uint BatteryLifeTime;

        /// <summary>Seconds of life on a full battery, or 0xFFFFFFFF.</summary>
        public uint BatteryFullLifeTime;
    }

    /// <summary>The LASTINPUTINFO structure.</summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct LastInputInfo
    {
        /// <summary>Size of the structure; the caller sets it.</summary>
        public uint Size;

        /// <summary>Tick count of the last input event.</summary>
        public uint Time;
    }

    /// <summary>Gives idle, kernel and user time of all processors together since boot.</summary>
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetSystemTimes(out FileTime idle, out FileTime kernel, out FileTime user);

    /// <summary>Gives the power status of the computer (charger and battery percentage).</summary>
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetSystemPowerStatus(out SystemPowerStatus status);

    /// <summary>Tells Windows that this thread needs the system or the display to stay on.</summary>
    [DllImport("kernel32.dll")]
    internal static extern uint SetThreadExecutionState(uint flags);

    /// <summary>Sends a message to a window and gives up after a time-out or when the window is hung.</summary>
    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr wParam, IntPtr lParam, uint flags, uint timeoutMs, out IntPtr result);

    /// <summary>Sends a message to a window of this thread and waits until its window procedure has handled it.</summary>
    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    /// <summary>Asks Windows to send WM_POWERBROADCAST messages to a window when one power setting changes.</summary>
    /// <returns>A handle for UnregisterPowerSettingNotification, or zero when the call failed.</returns>
    [DllImport("user32.dll", SetLastError = true)]
    internal static extern IntPtr RegisterPowerSettingNotification(IntPtr recipient, ref Guid powerSettingGuid, uint flags);

    /// <summary>Ends a registration made by RegisterPowerSettingNotification.</summary>
    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool UnregisterPowerSettingNotification(IntPtr handle);

    /// <summary>
    /// The fixed head of the POWERBROADCAST_SETTING structure. The data of the setting follows it in memory
    /// (a DWORD for the display state), so it is read separately at the offset Marshal.SizeOf gives.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    internal struct PowerBroadcastSettingHead
    {
        /// <summary>Which power setting changed.</summary>
        public Guid PowerSetting;

        /// <summary>Number of data bytes that follow the head.</summary>
        public uint DataLength;
    }

    /// <summary>Makes a synthetic mouse event; used to wake the display.</summary>
    [DllImport("user32.dll")]
    internal static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extraInfo);

    /// <summary>Gives the time of the last keyboard or mouse input of this session.</summary>
    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    internal static extern bool GetLastInputInfo(ref LastInputInfo info);
}
