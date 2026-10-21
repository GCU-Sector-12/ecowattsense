#nullable enable
using System;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client.Reporting;

public sealed record ReportIntervalResult(DeviceReport? Report = null, string? SkipReason = null);

/// <summary>Consumes sequential sample boundaries; the caller supplies monotonic time, never wall-clock elapsed time.</summary>
public sealed class ReportInterval(DeviceIdentity identity, ReportingSettings settings)
{
    private readonly CpuIntervalAccumulator cpu = new();
    private TimeSpan? start;

    public ReportIntervalResult Observe(TimeSpan boundary, DateTime utcEnd, CpuReading? reading,
        double? inactivity, ActivityState? state, bool? locked, bool? onBattery)
    {
        if (reading is null || reading.Status == CpuReadingStatus.Invalid)
        {
            cpu.MarkMissing();
            return new(SkipReason: "CPU measurement missing or invalid; interval discarded pending a fresh boundary.");
        }

        if (reading.Status == CpuReadingStatus.BaselineEstablished || start is null || !cpu.IsComplete)
        {
            bool discarded = start is not null;
            ResetAt(boundary); // Recovery sample is not reused in the new interval.
            return new(SkipReason: discarded ? "CPU coverage incomplete; restarted interval at established boundary." : null);
        }

        cpu.Add(reading.Sample); // Include the closing sample before reading the average.
        TimeSpan elapsed = boundary - start.Value;
        if (elapsed < TimeSpan.Zero)
        {
            cpu.MarkMissing();
            return new(SkipReason: "Monotonic boundary moved backwards; interval discarded.");
        }
        if (elapsed.TotalSeconds < settings.IntervalSeconds)
            return new();

        try
        {
            double? average = cpu.GetAveragePercentage();
            if (average is null)
                return new(SkipReason: "CPU interval coverage is empty or incomplete.");
            if (inactivity is not { } idle || !double.IsFinite(idle) || idle < 0)
                return new(SkipReason: "Current inactivity is unknown or invalid.");
            if (state is not (ActivityState.Active or ActivityState.Idle or ActivityState.Unused))
                return new(SkipReason: "Current activity state is unknown or invalid.");
            if (locked is null)
                return new(SkipReason: "Current screen-lock status is unknown.");
            if (onBattery is null)
                return new(SkipReason: "Current power-source status is unknown.");
            if (utcEnd.Kind != DateTimeKind.Utc)
                return new(SkipReason: "Interval-end timestamp is not UTC.");

            int seconds = checked((int)(elapsed.Ticks / TimeSpan.TicksPerSecond));
            int idleSeconds = checked((int)Math.Floor(idle));
            return new(new DeviceReport(identity.Id, utcEnd, seconds, state.Value,
                idleSeconds, average.Value, locked.Value, onBattery.Value, settings.DeviceType));
        }
        catch (OverflowException)
        {
            return new(SkipReason: "Interval or inactivity seconds exceed the report integer range.");
        }
        finally
        {
            ResetAt(boundary); // Completed and skipped intervals both release their samples.
        }
    }

    private void ResetAt(TimeSpan boundary)
    {
        cpu.Reset();
        start = boundary;
    }
}
