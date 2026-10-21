#nullable enable
using System;

namespace EcoWattSense.Client.Monitoring;

/// <summary>Immutable deltas in Windows CPU counter ticks (100 ns), summed across processors.</summary>
public sealed class CpuSample
{
    public double BusyTime { get; }
    public double TotalTime { get; }
    public double UsagePercentage => Math.Clamp(100.0 * (BusyTime / TotalTime), 0.0, 100.0);

    public CpuSample(double busyTime, double totalTime)
    {
        if (!double.IsFinite(totalTime) || totalTime <= 0)
            throw new ArgumentOutOfRangeException(nameof(totalTime));
        if (!double.IsFinite(busyTime) || busyTime < 0 || busyTime > totalTime)
            throw new ArgumentOutOfRangeException(nameof(busyTime));
        BusyTime = busyTime;
        TotalTime = totalTime;
    }
}

public enum CpuReadingStatus
{
    Invalid = 0,
    BaselineEstablished = 1,
    Valid = 2
}

public sealed class CpuReading
{
    public CpuReadingStatus Status { get; }
    public CpuSample? Sample { get; }

    internal CpuReading(CpuReadingStatus status, CpuSample? sample = null)
    {
        Status = status;
        Sample = sample;
    }
}
