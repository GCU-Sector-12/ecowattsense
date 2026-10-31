namespace EcoWattSense.Client.Monitoring;

public sealed record MonitoringReading(DateTime Timestamp, double? InactivitySeconds,
    double? CpuPercentage, ActivityState? State, bool? IsLocked, bool? IsOnBattery);
