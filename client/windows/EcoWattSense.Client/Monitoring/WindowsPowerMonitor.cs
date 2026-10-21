using System.ComponentModel;
using System.Runtime.InteropServices;

namespace EcoWattSense.Client.Monitoring;

/// <summary>Reads the system power source reported by Windows.</summary>
public sealed class WindowsPowerMonitor
{
    /// <returns>
    /// True when running on battery, false when AC power is online, or null when
    /// the power source is unknown or the reported status is inconsistent.
    /// </returns>
    /// <exception cref="InvalidOperationException">The Windows power-status query failed.</exception>
    public bool? IsOnBattery()
    {
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("Power-source monitoring requires Windows.");
        }

        if (!GetSystemPowerStatus(out var status))
        {
            int error = Marshal.GetLastWin32Error();
            throw new InvalidOperationException("Unable to query Windows power status.", new Win32Exception(error));
        }

        // AC online is authoritative even on desktops with no battery, or when a
        // laptop is plugged in but not charging. Battery absence alone does not
        // establish that mains power is online. 255 is unknown, not a bitmask.
        bool noBattery = status.BatteryFlag != byte.MaxValue && (status.BatteryFlag & 128) != 0;
        return status.ACLineStatus switch
        {
            1 => false,
            0 when !noBattery => true,
            _ => null
        };
    }

    // SYSTEM_POWER_STATUS: four BYTE fields followed by two DWORD fields (12 bytes).
    [StructLayout(LayoutKind.Sequential)]
    private struct SystemPowerStatus
    {
        public byte ACLineStatus;
        public byte BatteryFlag;
        public byte BatteryLifePercent;
        public byte SystemStatusFlag;
        public uint BatteryLifeTime;
        public uint BatteryFullLifeTime;
    }

    [DllImport("kernel32.dll", ExactSpelling = true, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetSystemPowerStatus(out SystemPowerStatus status);
}
