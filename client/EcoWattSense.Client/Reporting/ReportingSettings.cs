using System;

namespace EcoWattSense.Client.Reporting;

public sealed class ReportingSettings
{
    public int IntervalSeconds { get; }
    public string DeviceType { get; }

    public ReportingSettings(int intervalSeconds, string deviceType)
    {
        if (intervalSeconds <= 0)
            throw new ArgumentOutOfRangeException(nameof(intervalSeconds), "Reporting:IntervalSeconds must be a positive integer.");
        if (!DeviceReport.IsValidDeviceType(deviceType))
            throw new ArgumentException("Reporting:DeviceType is required and must match ^[a-z][a-z0-9_]*$ (maximum 40 characters).", nameof(deviceType));
        IntervalSeconds = intervalSeconds;
        DeviceType = deviceType;
    }
}
