#nullable enable
using System;

namespace EcoWattSense.Client.Monitoring;

/// <summary>
/// Accumulates whole, consecutive CPU samples. Call sequentially; reset only at a
/// sample boundary. A missing sample invalidates the interval until explicit reset.
/// </summary>
public sealed class CpuIntervalAccumulator
{
    private double busyTime;
    private double totalTime;
    public bool IsComplete { get; private set; } = true;

    public void Add(CpuSample? sample)
    {
        if (sample is null)
        {
            MarkMissing();
            return;
        }
        if (!IsComplete)
            return;

        double nextBusy = busyTime + sample.BusyTime;
        double nextTotal = totalTime + sample.TotalTime;
        if (!double.IsFinite(nextBusy) || !double.IsFinite(nextTotal))
        {
            MarkMissing();
            return;
        }
        busyTime = nextBusy;
        totalTime = nextTotal;
    }

    public void MarkMissing() => IsComplete = false;

    public double? GetAveragePercentage() => IsComplete && totalTime > 0
        ? Math.Clamp(100.0 * (busyTime / totalTime), 0.0, 100.0)
        : null;

    public void Reset()
    {
        busyTime = 0;
        totalTime = 0;
        IsComplete = true;
    }
}
