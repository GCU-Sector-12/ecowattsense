// Fmt.cs: number and text formatting for the window, summary.txt and the logs.
// Who uses it: MainWindow rows (ViewModels.cs), Analysis.cs and ResultWriter.cs. It holds no WPF code, so the console tests compile it.
using System.Globalization;

namespace EnergyCalibrator;

/// <summary>Number and text formatting. Numbers always use the invariant culture (a dot as the decimal mark).</summary>
internal static class Fmt
{
    private static readonly CultureInfo Inv = CultureInfo.InvariantCulture;

    /// <summary>Formats a number for a table cell.</summary>
    /// <param name="value">The number, or null.</param>
    /// <param name="format">A .NET number format such as "0.00".</param>
    /// <returns>The number, or "-" when it is unknown.</returns>
    public static string Number(double? value, string format) => value.HasValue ? value.Value.ToString(format, Inv) : "-";

    /// <summary>Formats a number with its unit.</summary>
    /// <param name="value">The number, or null.</param>
    /// <param name="format">A .NET number format.</param>
    /// <param name="unit">The unit text.</param>
    /// <returns>For example "4.10 W", or "not available" when the number is unknown.</returns>
    public static string WithUnit(double? value, string format, string unit) => value.HasValue ? $"{value.Value.ToString(format, Inv)} {unit}" : "not available";

    /// <summary>Formats one value of a sample with its unit.</summary>
    /// <param name="s">The sample.</param>
    /// <param name="key">The value name.</param>
    /// <param name="format">A .NET number format.</param>
    /// <param name="unit">The unit text.</param>
    /// <returns>The text, or "unknown".</returns>
    private static string Show(Sample s, string key, string format, string unit)
    {
        double? value = s.Get(key);
        return value.HasValue ? $"{value.Value.ToString(format, Inv)} {unit}" : "unknown";
    }

    /// <summary>Tells whether the laptop runs on battery or on the charger.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string PowerSource(Sample s)
    {
        bool? onCharger = s.OnCharger();
        if (onCharger == null) { return "unknown"; }
        return onCharger.Value ? "On charger" : "On battery";
    }

    /// <summary>Battery charge from the IOCTL with the percentage.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string ChargeIoctl(Sample s)
    {
        string charge = Show(s, SampleKeys.BatteryMwh, "N0", "mWh");
        string percent = s.BatteryPercent.HasValue ? $", {s.BatteryPercent.Value.ToString("0", Inv)}%" : "";
        return charge + percent;
    }

    /// <summary>Battery voltage from the IOCTL.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string VoltageIoctl(Sample s) => Show(s, SampleKeys.BatteryMv, "N0", "mV");

    /// <summary>Battery power rate from the IOCTL; many laptops do not report it.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string RateIoctl(Sample s) => s.Has(SampleKeys.BatteryRateMw) ? Show(s, SampleKeys.BatteryRateMw, "N0", "mW") : "unknown (this battery driver does not report it)";

    /// <summary>Battery charge, voltage and charger flag from WMI.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string ChargeWmi(Sample s)
    {
        string charge = Show(s, SampleKeys.WmiMwh, "N0", "mWh");
        string voltage = Show(s, SampleKeys.WmiMv, "N0", "mV");
        return $"{charge}, {voltage}, {ChargerText(s)}";
    }

    /// <summary>Tells whether WMI says the charger is connected.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    private static string ChargerText(Sample s)
    {
        double? online = s.Get(SampleKeys.WmiOnline);
        if (online == null) { return "charger unknown"; }
        return online.Value != 0 ? "charger on" : "charger off";
    }

    /// <summary>Power and temperature of the Intel DTT participant that behaves like the processor package.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string DttPackage(Sample s)
    {
        if (!s.Has(SampleKeys.DttPackageWatts)) { return "unknown (participant 3 not reported)"; }
        return $"{Show(s, SampleKeys.DttPackageWatts, "0.0", "W")}, {Show(s, SampleKeys.DttPackageTemp, "0", "C")}";
    }

    /// <summary>The power of every Intel DTT participant.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text, for example "p0 1.2 W; p3 4.2 W".</returns>
    public static string DttList(Sample s)
    {
        List<string> keys = s.Values.Keys
            .Where(key => key.StartsWith(SampleKeys.DttPrefix, StringComparison.Ordinal) && key.EndsWith("_power_w", StringComparison.Ordinal))
            .Order(StringComparer.Ordinal)
            .ToList();
        if (keys.Count == 0) { return "none read"; }
        return string.Join("; ", keys.Select(key => DttEntry(s, key)));
    }

    /// <summary>Formats one Intel DTT participant.</summary>
    /// <param name="s">The sample.</param>
    /// <param name="key">The power column name of the participant.</param>
    /// <returns>The text.</returns>
    private static string DttEntry(Sample s, string key)
    {
        string id = key[SampleKeys.DttPrefix.Length..^"_power_w".Length];
        return $"p{id} {s.Values[key].ToString("0.0", Inv)} W";
    }

    /// <summary>CPU use over all cores.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string CpuUse(Sample s) => Show(s, SampleKeys.CpuUtil, "0.0", "%");

    /// <summary>Processor performance counter and frequency.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string CpuPerformance(Sample s) => $"{Show(s, SampleKeys.CpuPerformance, "0", "%")}, {Show(s, SampleKeys.CpuMhz, "N0", "MHz")}";

    /// <summary>Highest thermal zone temperature.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string Thermal(Sample s) => Show(s, SampleKeys.ThermalMax, "0.0", "C (hottest zone)");

    /// <summary>Busiest GPU 3D engine.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string Gpu(Sample s) => Show(s, SampleKeys.Gpu3D, "0.0", "%");

    /// <summary>Disk throughput.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string Disk(Sample s) => Throughput(s, SampleKeys.DiskBytes);

    /// <summary>Network throughput.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string Network(Sample s) => Throughput(s, SampleKeys.NetBytes);

    /// <summary>Formats a throughput in kilobytes per second.</summary>
    /// <param name="s">The sample.</param>
    /// <param name="key">The value name.</param>
    /// <returns>The text.</returns>
    private static string Throughput(Sample s, string key)
    {
        double? bytes = s.Get(key);
        return bytes.HasValue ? $"{(bytes.Value / 1024).ToString("N0", Inv)} KB/s" : "unknown";
    }

    /// <summary>Screen brightness.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string Brightness(Sample s) => Show(s, SampleKeys.Brightness, "0", "%");

    /// <summary>What the display is doing, from the power notification.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>"off", "on", "dimmed" or "unknown".</returns>
    public static string Display(Sample s) => DisplayStateTexts.Describe(s.DisplayState);

    /// <summary>Power of the processor package from the Windows Energy Meter.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string EmiPackage(Sample s) => Emi(s, SampleKeys.EmiPackageWatts);

    /// <summary>Power of the processor cores from the Windows Energy Meter.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string EmiCores(Sample s) => Emi(s, SampleKeys.EmiCoresWatts);

    /// <summary>Power of the integrated graphics from the Windows Energy Meter.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string EmiGpu(Sample s) => Emi(s, SampleKeys.EmiGpuWatts);

    /// <summary>Power of the memory from the Windows Energy Meter.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The text.</returns>
    public static string EmiDram(Sample s) => Emi(s, SampleKeys.EmiDramWatts);

    /// <summary>Formats one Windows Energy Meter power value.</summary>
    /// <param name="s">The sample.</param>
    /// <param name="key">The value name.</param>
    /// <returns>The watts, or "not available" with the reason when the value is unknown.</returns>
    private static string Emi(Sample s, string key)
    {
        if (s.Has(key)) { return Show(s, key, "0.00", "W"); }
        return "not available: no value yet (a power needs two readings) or this instance is not reported";
    }

    /// <summary>The processor power settings for battery use, from the active power plan, and the Windows power mode.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>For example "EPP 50, boost Aggressive, max 100%, min 5%, power mode Balanced".</returns>
    public static string PowerSettings(Sample s)
    {
        string epp = Whole(s, SampleKeys.PowerEpp, "");
        string max = Whole(s, SampleKeys.PowerMax, "%");
        string min = Whole(s, SampleKeys.PowerMin, "%");
        return $"EPP {epp}, boost {BoostText(s)}, max {max}, min {min}, power mode {OverlayText(s)}";
    }

    /// <summary>Formats a value as a whole number with a suffix.</summary>
    /// <param name="s">The sample.</param>
    /// <param name="key">The value name.</param>
    /// <param name="suffix">Text after the number, for example "%".</param>
    /// <returns>The text, or "unknown".</returns>
    private static string Whole(Sample s, string key, string suffix)
    {
        double? value = s.Get(key);
        return value.HasValue ? $"{value.Value.ToString("0", Inv)}{suffix}" : "unknown";
    }

    /// <summary>Gives the name of the boost mode of a sample.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The name, or "unknown".</returns>
    private static string BoostText(Sample s)
    {
        double? mode = s.Get(SampleKeys.PowerBoost);
        return mode.HasValue ? PowerNames.Boost((uint)mode.Value) : "unknown";
    }

    /// <summary>Gives the name of the Windows power mode of a sample.</summary>
    /// <param name="s">The sample.</param>
    /// <returns>The name, or "unknown".</returns>
    private static string OverlayText(Sample s)
    {
        double? code = s.Get(SampleKeys.PowerOverlay);
        return code.HasValue ? PowerNames.OverlayFromCode((int)code.Value) : "unknown";
    }
}
