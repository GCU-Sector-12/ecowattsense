// HistoryRow.cs: one parsed minute of the service history (SPEC section 7).
// Who uses it: HistoryReader makes the rows; HistorySummary and ChartGeometry read them.
// An empty field in the file is null here, which means "unknown", never zero.
#nullable enable
using System;

namespace EcoWattSense.Client.Gui.History
{
    /// <summary>
    /// One history row. Every value has been range-checked by HistoryReader.
    /// </summary>
    /// <param name="EndUtc">End of the minute, UTC (column ts_utc).</param>
    /// <param name="Seconds">Length of the interval in seconds, 1 to 86400.</param>
    /// <param name="OnBattery">True when the computer ran on battery.</param>
    /// <param name="State">0 Unused, 1 Idle, 2 Active (the numbers of ActivityState), or null.</param>
    /// <param name="IdleSeconds">Idle time in seconds, or null.</param>
    /// <param name="CpuPercent">CPU load, 0 to 100, or null.</param>
    /// <param name="DeviceW">Power of the whole computer in watts, or null.</param>
    /// <param name="MeasuredW">Part of the power read from hardware, or null.</param>
    /// <param name="BatteryW">Battery flow in watts (negative when discharging), or null.</param>
    /// <param name="Level">Measurement level such as "L2", or null.</param>
    public sealed record HistoryRow(
        DateTime EndUtc,
        int Seconds,
        bool OnBattery,
        int? State,
        double? IdleSeconds,
        double? CpuPercent,
        double? DeviceW,
        double? MeasuredW,
        double? BatteryW,
        string? Level);
}
