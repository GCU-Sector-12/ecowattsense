using System.ComponentModel;
using System.Runtime.InteropServices;

namespace EcoWattSense.Client.Monitoring;

/// <summary>
/// Samples Windows CPU time without a background thread or performance-counter dependency.
/// Call sequentially from the worker; each result covers the interval since the previous sample.
/// </summary>
public sealed class CpuMonitor
{
    private (ulong Idle, ulong Kernel, ulong User)? previous;

    /// <returns>
    /// CPU utilisation from 0 to 100, or null while establishing a baseline or when
    /// the counters have not advanced or have reset.
    /// </returns>
    public double? GetUsagePercentage()
    {
        if (!OperatingSystem.IsWindows())
        {
            throw new PlatformNotSupportedException("CPU monitoring requires Windows.");
        }

        // GetSystemTimes only covers the calling processor group on multi-group machines.
        // Do not present a partial-machine reading as overall CPU utilisation.
        if (GetActiveProcessorGroupCount() > 1)
        {
            throw new InvalidOperationException("CPU monitoring currently supports Windows machines with one processor group.");
        }

        if (!GetSystemTimes(out var idle, out var kernel, out var user))
        {
            int error = Marshal.GetLastWin32Error();
            previous = null;
            throw new InvalidOperationException("GetSystemTimes failed to retrieve CPU usage.", new Win32Exception(error));
        }

        var current = (Idle: idle.ToUInt64(), Kernel: kernel.ToUInt64(), User: user.ToUInt64());
        var baseline = previous;
        previous = current;

        if (baseline is not { } last || current.Idle < last.Idle ||
            current.Kernel < last.Kernel || current.User < last.User)
        {
            return null;
        }

        ulong idleDelta = current.Idle - last.Idle;
        // Kernel time includes idle time, so subtract idle only from the busy numerator.
        double totalDelta = (double)(current.Kernel - last.Kernel) + (current.User - last.User);
        if (totalDelta <= 0 || idleDelta > totalDelta)
        {
            return null;
        }

        return Math.Clamp(100.0 * (totalDelta - idleDelta) / totalDelta, 0.0, 100.0);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct FileTime
    {
        public uint Low;
        public uint High;

        public readonly ulong ToUInt64() => ((ulong)High << 32) | Low;
    }

    [DllImport("kernel32.dll", ExactSpelling = true, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetSystemTimes(out FileTime idle, out FileTime kernel, out FileTime user);

    [DllImport("kernel32.dll", ExactSpelling = true)]
    private static extern ushort GetActiveProcessorGroupCount();
}
