// Sample.cs: one reading of every source (Sample), the names of its values (SampleKeys), a monotonic clock (ProcessClock),
// and the error type that readers use to say "this source is not available, and why" (SourceUnavailableException, Reasons).
// Who uses it: every reader fills a Sample; TestRunner stamps the phase; PhaseAnalyser, ResultWriter and the window read it.
// How to use: values live in Sample.Values by key. A key that is missing means "unknown". A failed read is never stored as 0.
using System.Diagnostics;
using System.Management;

namespace EnergyCalibrator;

/// <summary>Seconds on a monotonic clock that starts with the program. Wall-clock changes (time sync) do not disturb it.</summary>
internal static class ProcessClock
{
    private static readonly long Origin = Stopwatch.GetTimestamp();

    /// <summary>Gives the seconds since the program started.</summary>
    public static double Seconds => Stopwatch.GetElapsedTime(Origin).TotalSeconds;
}

/// <summary>An error that says in plain words why a source cannot be read. The message is safe to show and to save.</summary>
internal sealed class SourceUnavailableException(string reason) : Exception(reason)
{
}

/// <summary>Turns exceptions into short reasons without any text from the computer (no names, paths or ids).</summary>
internal static class Reasons
{
    /// <summary>Gives a short reason for a failed read.</summary>
    /// <param name="ex">The exception that the read threw.</param>
    /// <returns>A plain-English reason. Unknown exceptions give only the exception type name.</returns>
    public static string From(Exception ex)
    {
        if (ex is SourceUnavailableException known) { return known.Message; }
        if (ex is UnauthorizedAccessException) { return "access denied (run as administrator)"; }
        if (ex is ManagementException wmi) { return FromWmi(wmi); }
        return $"read failed ({ex.GetType().Name})";
    }

    /// <summary>Gives a short reason for a WMI error, using only the WMI status name.</summary>
    /// <param name="ex">The WMI exception.</param>
    /// <returns>A plain-English reason.</returns>
    private static string FromWmi(ManagementException ex)
    {
        if (ex.ErrorCode == ManagementStatus.AccessDenied) { return "access denied (run as administrator)"; }
        if (ex.ErrorCode == ManagementStatus.InvalidClass) { return "not supported on this computer"; }
        if (ex.ErrorCode == ManagementStatus.NotFound) { return "not supported on this computer"; }
        return $"WMI error {ex.ErrorCode}";
    }
}

/// <summary>The names of the values in a sample and of the sources. They are also the column names of samples.csv.</summary>
internal static class SampleKeys
{
    /// <summary>Raw power state flags of the battery IOCTL: 1 online, 2 discharging, 4 charging.</summary>
    public const string BatteryState = "battery_state";
    /// <summary>Battery charge from the IOCTL in mWh.</summary>
    public const string BatteryMwh = "battery_mwh";
    /// <summary>Battery voltage from the IOCTL in mV.</summary>
    public const string BatteryMv = "battery_mv";
    /// <summary>Battery power rate from the IOCTL in mW; missing when the driver does not report it.</summary>
    public const string BatteryRateMw = "battery_rate_mw";
    /// <summary>Battery charge in percent from GetSystemPowerStatus.</summary>
    public const string BatteryPercent = "battery_percent";
    /// <summary>1 when GetSystemPowerStatus says the charger is connected, 0 when not.</summary>
    public const string AcLine = "ac_line_online";
    /// <summary>Battery charge from WMI BatteryStatus in mWh.</summary>
    public const string WmiMwh = "wmi_battery_mwh";
    /// <summary>Battery voltage from WMI in mV.</summary>
    public const string WmiMv = "wmi_battery_mv";
    /// <summary>1 when WMI says the charger is connected, 0 when not.</summary>
    public const string WmiOnline = "wmi_power_online";
    /// <summary>CPU use over all cores in percent, from GetSystemTimes.</summary>
    public const string CpuUtil = "cpu_util_pct";
    /// <summary>"% Processor Performance" counter.</summary>
    public const string CpuPerformance = "cpu_performance_pct";
    /// <summary>"Processor Frequency" counter in MHz.</summary>
    public const string CpuMhz = "cpu_frequency_mhz";
    /// <summary>Highest thermal zone temperature in degrees Celsius.</summary>
    public const string ThermalMax = "thermal_zone_max_c";
    /// <summary>Start of the name of each thermal zone column; the zone number and "_c" follow.</summary>
    public const string ThermalZonePrefix = "thermal_zone_";
    /// <summary>Busiest GPU 3D engine in percent.</summary>
    public const string Gpu3D = "gpu_3d_pct";
    /// <summary>Disk throughput in bytes per second.</summary>
    public const string DiskBytes = "disk_bytes_per_s";
    /// <summary>Network throughput in bytes per second, all adapters together.</summary>
    public const string NetBytes = "network_bytes_per_s";
    /// <summary>Screen brightness in percent from WMI.</summary>
    public const string Brightness = "brightness_pct";
    /// <summary>Seconds since the last mouse or keyboard input.</summary>
    public const string UserIdle = "user_idle_s";
    /// <summary>Power of Intel DTT participant 3 (behaves like the processor package) in watts.</summary>
    public const string DttPackageWatts = "dtt_p3_power_w";
    /// <summary>Temperature of Intel DTT participant 3 in degrees Celsius.</summary>
    public const string DttPackageTemp = "dtt_p3_temp_c";
    /// <summary>Start of the name of every Intel DTT column.</summary>
    public const string DttPrefix = "dtt_p";
    /// <summary>Start of the name of every Energy Meter column.</summary>
    public const string EnergyMeterPrefix = "energymeter_";

    /// <summary>Source name: battery IOCTL.</summary>
    public const string SourceBatteryIoctl = "Battery (IOCTL)";
    /// <summary>Source name: GetSystemPowerStatus.</summary>
    public const string SourcePowerStatus = "Battery percentage (GetSystemPowerStatus)";
    /// <summary>Source name: WMI BatteryStatus.</summary>
    public const string SourceWmiBattery = "Battery (WMI)";
    /// <summary>Source name: Intel Dynamic Tuning.</summary>
    public const string SourceDtt = "Intel DTT";
    /// <summary>Source name: GetSystemTimes.</summary>
    public const string SourceCpuTimes = "CPU use (GetSystemTimes)";
    /// <summary>Source name: processor performance counter.</summary>
    public const string SourceCpuPerformance = "CPU performance counter";
    /// <summary>Source name: processor frequency counter.</summary>
    public const string SourceCpuFrequency = "CPU frequency counter";
    /// <summary>Source name: thermal zone counters.</summary>
    public const string SourceThermal = "Thermal zone counters";
    /// <summary>Source name: GPU engine counters.</summary>
    public const string SourceGpu = "GPU engine counters";
    /// <summary>Source name: disk counter.</summary>
    public const string SourceDisk = "Disk counter";
    /// <summary>Source name: network counters.</summary>
    public const string SourceNetwork = "Network counters";
    /// <summary>Source name: screen brightness WMI.</summary>
    public const string SourceBrightness = "Screen brightness (WMI)";
    /// <summary>Source name: Energy Meter counter set.</summary>
    public const string SourceEnergyMeter = "Energy Meter counters";
    /// <summary>Source name: last user input.</summary>
    public const string SourceUserInput = "Last user input";
    /// <summary>Source name: display state from the power setting notification. It is never marked unavailable; "unknown" is a state.</summary>
    public const string SourceDisplayState = "Display state (power notification)";

    /// <summary>Columns of samples.csv that are always present, in this order. Other keys follow in alphabetical order.</summary>
    public static readonly string[] CsvOrder =
    [
        BatteryState, BatteryMwh, BatteryMv, BatteryRateMw, BatteryPercent, AcLine, WmiMwh, WmiMv, WmiOnline,
        CpuUtil, CpuPerformance, CpuMhz, ThermalMax, Gpu3D, DiskBytes, NetBytes, Brightness, UserIdle,
        DttPackageWatts, DttPackageTemp
    ];
}

/// <summary>One reading of all sources taken at one moment.</summary>
internal sealed class Sample
{
    /// <summary>Wall-clock time when the reading started (local time, for people to read).</summary>
    public DateTime Time { get; } = DateTime.Now;

    /// <summary>Monotonic seconds when the reading started; all timing in the analysis uses this value.</summary>
    public double MonotonicSeconds { get; } = ProcessClock.Seconds;

    /// <summary>Number of the phase this sample belongs to (1 to 7), or 0 when no phase was running.</summary>
    public int PhaseNumber { get; set; }

    /// <summary>Seconds since the phase started; negative when the reading began before the phase clock started.</summary>
    public double SecondsIntoPhase { get; set; }

    /// <summary>What the display was doing when the reading started, from the power notification. Unknown when no notification arrived.</summary>
    public DisplayStateKind DisplayState { get; set; } = DisplayStateKind.Unknown;

    /// <summary>Every value that could be read, by key. A missing key means "unknown".</summary>
    public Dictionary<string, double> Values { get; } = new();

    /// <summary>The sources that could not be read in this sample, with the reason.</summary>
    public Dictionary<string, string> Unavailable { get; } = new();

    /// <summary>Tells whether a value was read.</summary>
    /// <param name="key">The value name from SampleKeys or a dynamic column name.</param>
    /// <returns>True when the value is known.</returns>
    public bool Has(string key) => Values.ContainsKey(key);

    /// <summary>Gives a value or null when it is unknown.</summary>
    /// <param name="key">The value name.</param>
    /// <returns>The value, or null.</returns>
    public double? Get(string key) => Values.TryGetValue(key, out double value) ? value : null;

    /// <summary>Stores a value that needs no range check (flags and raw counters).</summary>
    /// <param name="key">The value name.</param>
    /// <param name="value">The value.</param>
    public void Set(string key, double value) => Values[key] = value;

    /// <summary>Stores a value only when it is a real number inside the allowed range. Odd values stay unknown.</summary>
    /// <param name="key">The value name.</param>
    /// <param name="value">The value, or null when the read gave nothing.</param>
    /// <param name="min">Smallest believable value.</param>
    /// <param name="max">Largest believable value.</param>
    public void SetChecked(string key, double? value, double min, double max)
    {
        if (value is not double number) { return; }
        if (double.IsNaN(number) || number < min || number > max) { return; }
        Values[key] = number;
    }

    /// <summary>Records that a source could not be read in this sample.</summary>
    /// <param name="source">Name of the source.</param>
    /// <param name="reason">Plain-English reason.</param>
    public void MarkUnavailable(string source, string reason) => Unavailable[source] = reason;

    /// <summary>Battery percentage from GetSystemPowerStatus, or null when unknown.</summary>
    public double? BatteryPercent => Get(SampleKeys.BatteryPercent);

    /// <summary>
    /// Tells whether the charger is plugged in. Uses the battery IOCTL first, then WMI, then GetSystemPowerStatus.
    /// </summary>
    /// <returns>True on the charger, false on battery, null when no source knows.</returns>
    public bool? OnCharger()
    {
        if (Values.TryGetValue(SampleKeys.BatteryState, out double state)) { return (((uint)state) & 0x1) != 0; }
        if (Values.TryGetValue(SampleKeys.WmiOnline, out double online)) { return online != 0; }
        if (Values.TryGetValue(SampleKeys.AcLine, out double line)) { return line != 0; }
        return null;
    }
}
