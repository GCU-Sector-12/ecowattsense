using System;

namespace EcoWattSense.Client.Monitoring;

public enum ActivityState
{
    Unused = 0,
    Idle = 1,
    Active = 2
}

/// <summary>Classifies a measured inactivity duration; other monitoring signals are not inputs.</summary>
public sealed class StateClassifier
{
    public double IdleThresholdSeconds { get; }
    public double UnusedThresholdSeconds { get; }

    public StateClassifier(double idleThresholdSeconds = 600, double unusedThresholdSeconds = 3600)
    {
        if (!double.IsFinite(idleThresholdSeconds) || idleThresholdSeconds <= 0)
        {
            throw new ArgumentOutOfRangeException(nameof(idleThresholdSeconds),
                "Idle threshold must be a finite number greater than zero.");
        }

        if (!double.IsFinite(unusedThresholdSeconds) || unusedThresholdSeconds <= idleThresholdSeconds)
        {
            throw new ArgumentOutOfRangeException(nameof(unusedThresholdSeconds),
                "Unused threshold must be finite and greater than the idle threshold.");
        }

        IdleThresholdSeconds = idleThresholdSeconds;
        UnusedThresholdSeconds = unusedThresholdSeconds;
    }

    /// <returns>Null for missing, negative, or non-finite measurements; otherwise the activity state.</returns>
    public ActivityState? Classify(double? inactivitySeconds)
    {
        if (inactivitySeconds is not { } seconds || !double.IsFinite(seconds) || seconds < 0)
        {
            return null;
        }

        if (seconds >= UnusedThresholdSeconds)
        {
            return ActivityState.Unused;
        }

        return seconds >= IdleThresholdSeconds ? ActivityState.Idle : ActivityState.Active;
    }
}
