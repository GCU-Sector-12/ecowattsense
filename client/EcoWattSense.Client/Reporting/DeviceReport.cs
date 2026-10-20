using System;
using System.Text.Json.Serialization;
using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client.Reporting;

/// <summary>
/// Immutable outbound report containing only the ten required report-schema fields.
/// All measurements must be supplied; unknown measurements cannot form a report.
/// </summary>
public sealed class DeviceReport
{
    [JsonPropertyName("device_id")]
    public Guid DeviceId { get; }

    [JsonPropertyName("timestamp")]
    public DateTime Timestamp { get; }

    [JsonPropertyName("interval_seconds")]
    public int IntervalSeconds { get; }

    [JsonPropertyName("state")]
    public ActivityState State { get; }

    [JsonPropertyName("idle_seconds")]
    public int IdleSeconds { get; }

    /// <summary>Average CPU utilisation over the reported interval, calculated and supplied by the caller.</summary>
    [JsonPropertyName("cpu_percent")]
    public double CpuPercent { get; }

    [JsonPropertyName("screen_locked")]
    public bool ScreenLocked { get; }

    [JsonPropertyName("on_battery")]
    public bool OnBattery { get; }

    [JsonPropertyName("os")]
    public string Os => "windows";

    [JsonPropertyName("device_type")]
    public string DeviceType { get; }

    public DeviceReport(Guid deviceId, DateTime timestamp, int intervalSeconds,
        ActivityState state, int idleSeconds, double cpuPercent, bool screenLocked,
        bool onBattery, string deviceType)
    {
        if (deviceId == Guid.Empty)
            throw new ArgumentException("Device UUID must not be empty.", nameof(deviceId));
        if (timestamp.Kind == DateTimeKind.Unspecified)
            throw new ArgumentException("Timestamp must have UTC or Local kind; unspecified timezones are not assumed.", nameof(timestamp));
        if (intervalSeconds <= 0)
            throw new ArgumentOutOfRangeException(nameof(intervalSeconds), "Report interval must be positive.");
        if (state is not (ActivityState.Active or ActivityState.Idle or ActivityState.Unused))
            throw new ArgumentOutOfRangeException(nameof(state), "State must be Active (2), Idle (1), or Unused (0).");
        if (idleSeconds < 0)
            throw new ArgumentOutOfRangeException(nameof(idleSeconds), "Idle duration must be nonnegative.");
        if (!double.IsFinite(cpuPercent) || cpuPercent < 0 || cpuPercent > 100)
            throw new ArgumentOutOfRangeException(nameof(cpuPercent), "CPU percentage must be finite and between 0 and 100.");
        if (!IsValidDeviceType(deviceType))
            throw new ArgumentException("Device type must match ^[a-z][a-z0-9_]*$ and contain at most 40 characters.", nameof(deviceType));

        DeviceId = deviceId;
        Timestamp = timestamp.ToUniversalTime();
        IntervalSeconds = intervalSeconds;
        State = state;
        IdleSeconds = idleSeconds;
        CpuPercent = cpuPercent;
        ScreenLocked = screenLocked;
        OnBattery = onBattery;
        DeviceType = deviceType;
    }

    private static bool IsValidDeviceType(string value)
    {
        if (string.IsNullOrEmpty(value) || value.Length > 40 || value[0] is < 'a' or > 'z')
            return false;

        foreach (char character in value)
        {
            if (character is not (>= 'a' and <= 'z') and not (>= '0' and <= '9') and not '_')
                return false;
        }

        return true;
    }
}
