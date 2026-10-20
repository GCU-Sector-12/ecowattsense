using System.Runtime.InteropServices;

namespace EcoWattSense.Client.Monitoring;

public sealed class WindowsActivityMonitor
{
    /// <summary>
    /// Returns seconds since the last keyboard or mouse input in the calling session.
    /// Durations are modulo the 32-bit tick period (approximately 49.7 days).
    /// </summary>
    public double GetInactivitySeconds()
    {
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("User inactivity monitoring requires Windows.");
        }

        var lastInput = new LastInputInfo
        {
            Size = (uint)Marshal.SizeOf<LastInputInfo>()
        };

        if (!GetLastInputInfo(ref lastInput))
        {
            // This API does not document a GetLastError value on failure.
            throw new InvalidOperationException("GetLastInputInfo failed to retrieve user activity.");
        }

        // Unsigned subtraction handles both signed TickCount rollover and uint wraparound.
        uint elapsedMilliseconds = unchecked((uint)Environment.TickCount - lastInput.Time);
        return elapsedMilliseconds / 1000.0;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LastInputInfo
    {
        public uint Size;
        public uint Time;
    }

    [DllImport("user32.dll", ExactSpelling = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetLastInputInfo(ref LastInputInfo lastInput);
}
