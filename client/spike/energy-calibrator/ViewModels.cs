// ViewModels.cs: the parts of the main window that are not XAML: the live reading rows (ReadingRow, ReadingRows, Fmt),
// the phase rows with their progress bars (PhaseRowView), the result rows (PhaseResultRow), the Start gate (StartGate),
// the instruction text (InstructionTexts) and the administrator helper (Elevation).
// Who uses it: MainWindow. Fmt is also used by Analysis.cs and ResultWriter.cs.
// Graphics on the GPU: every progress bar is a Border whose RenderTransform (ScaleX) is animated by a DoubleAnimation,
// so it is composited by WPF and never redrawn from a timer.
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Security.Principal;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;

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
}

/// <summary>One line of the live readings: a name and the latest value, or why the source is not available.</summary>
internal sealed class ReadingRow : INotifyPropertyChanged
{
    private readonly string source;
    private readonly Func<Sample, string> format;
    private string current = "waiting for the first reading";

    /// <summary>Creates a row.</summary>
    /// <param name="name">The label shown on the left.</param>
    /// <param name="source">The name of the source, to look up "not available" in a sample.</param>
    /// <param name="format">Turns a sample into the value text.</param>
    /// <param name="hint">A tooltip with more explanation.</param>
    public ReadingRow(string name, string source, Func<Sample, string> format, string hint = "")
    {
        Name = name;
        this.source = source;
        this.format = format;
        Hint = hint.Length == 0 ? null : hint;
    }

    /// <summary>The label.</summary>
    public string Name { get; }

    /// <summary>The tooltip text; null when there is none, so no empty tooltip is shown.</summary>
    public string? Hint { get; }

    /// <summary>The value text shown on the right.</summary>
    public string Value => current;

    /// <inheritdoc />
    public event PropertyChangedEventHandler? PropertyChanged;

    /// <summary>Shows the newest sample. Raises the change event only when the text changed.</summary>
    /// <param name="sample">The newest sample.</param>
    public void Update(Sample sample)
    {
        string text = sample.Unavailable.TryGetValue(source, out string? reason) ? $"not available: {reason}" : format(sample);
        if (text == current) { return; }
        current = text;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Value)));
    }
}

/// <summary>The list of live reading rows.</summary>
internal static class ReadingRows
{
    /// <summary>Creates one row for every source the program reads.</summary>
    /// <returns>The rows in display order.</returns>
    public static List<ReadingRow> Create()
    {
        return
        [
            new("Power source", SampleKeys.SourceBatteryIoctl, Fmt.PowerSource, "From the battery driver (IOCTL), with WMI and GetSystemPowerStatus as fall-backs."),
            new("Battery charge (IOCTL)", SampleKeys.SourceBatteryIoctl, Fmt.ChargeIoctl, "The charge falls in steps of about 11 mWh every 10 seconds on the test laptop."),
            new("Battery voltage (IOCTL)", SampleKeys.SourceBatteryIoctl, Fmt.VoltageIoctl),
            new("Battery rate (IOCTL)", SampleKeys.SourceBatteryIoctl, Fmt.RateIoctl, "Many laptops report no rate; the program then works from the charge."),
            new("Battery charge (WMI)", SampleKeys.SourceWmiBattery, Fmt.ChargeWmi, "A second source; it lags the IOCTL by about one 10 second step."),
            new("CPU package (Intel DTT)", SampleKeys.SourceDtt, Fmt.DttPackage, "Intel Dynamic Tuning participant 3 behaves like the processor package. The class is undocumented: this meaning is derived from a test. Needs administrator rights."),
            new("Intel DTT participants", SampleKeys.SourceDtt, Fmt.DttList, "Every participant is logged in samples.csv as dtt_p<number>_power_w and dtt_p<number>_temp_c."),
            new("CPU use", SampleKeys.SourceCpuTimes, Fmt.CpuUse, "Total over all logical processors, from GetSystemTimes."),
            new("CPU performance and frequency", SampleKeys.SourceCpuPerformance, Fmt.CpuPerformance, "Processor Information counters."),
            new("Temperature", SampleKeys.SourceThermal, Fmt.Thermal, "Thermal Zone Information counters, Kelvin converted to Celsius."),
            new("GPU 3D engine", SampleKeys.SourceGpu, Fmt.Gpu, "GPU Engine counters of type 3D: summed per engine over all processes, the busiest engine is shown."),
            new("Disk throughput", SampleKeys.SourceDisk, Fmt.Disk),
            new("Network throughput", SampleKeys.SourceNetwork, Fmt.Network),
            new("Screen brightness", SampleKeys.SourceBrightness, Fmt.Brightness),
            new("Display state", SampleKeys.SourceDisplayState, Fmt.Display, "Reported by Windows (power setting GUID_CONSOLE_DISPLAY_STATE): off, on or dimmed. Unknown until the first notification arrives."),
            new("CPU package (Windows Energy Meter)", SampleKeys.SourceEnergyMeter, Fmt.EmiPackage, "RAPL PKG, the whole processor package: watts since the previous reading from the Energy Meter counter set of Windows 11. A normal user may read it. It already holds the cores and the graphics."),
            new("CPU cores (Windows Energy Meter)", SampleKeys.SourceEnergyMeter, Fmt.EmiCores, "RAPL PP0: the processor cores."),
            new("Integrated graphics (Windows Energy Meter)", SampleKeys.SourceEnergyMeter, Fmt.EmiGpu, "RAPL PP1: the integrated graphics."),
            new("Memory (Windows Energy Meter)", SampleKeys.SourceEnergyMeter, Fmt.EmiDram, "RAPL DRAM: the memory.")
        ];
    }
}

/// <summary>One row of the phase list, with a progress bar that is animated on the GPU.</summary>
internal sealed class PhaseRowView
{
    private static readonly Brush ActiveBrush = Frozen("#E8F0FE");
    private static readonly Brush TrackBrush = Frozen("#E5E7EB");
    private static readonly Brush RunningBrush = Frozen("#2563EB");
    private static readonly Brush DoneBrush = Frozen("#16A34A");
    private static readonly Brush InvalidBrush = Frozen("#DC2626");
    private static readonly Brush StoppedBrush = Frozen("#D97706");
    private static readonly Brush DetailBrush = Frozen("#6B7280");

    private readonly PhaseDefinition phase;
    private readonly Border root = new() { CornerRadius = new CornerRadius(6), Margin = new Thickness(0, 0, 0, 4), Padding = new Thickness(8, 6, 8, 6) };
    private readonly Border fill = new() { RenderTransformOrigin = new Point(0, 0.5), Background = RunningBrush };
    private readonly ScaleTransform fillScale = new(0, 1);
    private readonly TextBlock statusText = new() { VerticalAlignment = VerticalAlignment.Center, FontWeight = FontWeights.SemiBold };

    /// <summary>Creates the row and its controls.</summary>
    /// <param name="phase">The phase this row shows.</param>
    public PhaseRowView(PhaseDefinition phase)
    {
        this.phase = phase;
        fill.RenderTransform = fillScale;
        Build();
    }

    /// <summary>The element to put in the phase list.</summary>
    public UIElement Root => root;

    /// <summary>Puts the row back to "waiting".</summary>
    public void Reset()
    {
        fillScale.BeginAnimation(ScaleTransform.ScaleXProperty, null);
        fillScale.ScaleX = 0;
        fill.Background = RunningBrush;
        root.Background = Brushes.Transparent;
        statusText.Text = PhaseStatusTexts.Describe(PhaseStatus.NotRun);
    }

    /// <summary>Highlights the row and starts the progress bar. The animation runs for the length of the phase.</summary>
    public void Start()
    {
        root.Background = ActiveBrush;
        statusText.Text = PhaseStatusTexts.Describe(PhaseStatus.Running);
        DoubleAnimation progress = new(0, 1, TimeSpan.FromSeconds(phase.Seconds)) { FillBehavior = FillBehavior.HoldEnd };
        fillScale.BeginAnimation(ScaleTransform.ScaleXProperty, progress);
    }

    /// <summary>Stops the progress bar where it is (or fills it when the phase is complete) and shows the final status.</summary>
    /// <param name="status">The final status of the phase.</param>
    public void Finish(PhaseStatus status)
    {
        double reached = fillScale.ScaleX;
        fillScale.BeginAnimation(ScaleTransform.ScaleXProperty, null);
        fillScale.ScaleX = status == PhaseStatus.Complete ? 1 : reached;
        fill.Background = BrushFor(status);
        statusText.Text = PhaseStatusTexts.Describe(status);
        root.Background = Brushes.Transparent;
    }

    /// <summary>Gives the colour of the progress bar for a final status.</summary>
    /// <param name="status">The status.</param>
    /// <returns>Green for done, red for invalid, amber for stopped.</returns>
    private static Brush BrushFor(PhaseStatus status)
    {
        if (status == PhaseStatus.Complete) { return DoneBrush; }
        if (status == PhaseStatus.InvalidCharger || status == PhaseStatus.InvalidInput || status == PhaseStatus.Failed) { return InvalidBrush; }
        if (status == PhaseStatus.InvalidDisplayOn || status == PhaseStatus.InvalidDisplayWoke) { return InvalidBrush; }
        return StoppedBrush;
    }

    /// <summary>Builds the grid of the row: number, name and detail, progress bar, status.</summary>
    private void Build()
    {
        Grid grid = new();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(32) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(330) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(170) });
        AddCell(grid, 0, new TextBlock { Text = phase.Number.ToString(CultureInfo.InvariantCulture), VerticalAlignment = VerticalAlignment.Center, FontWeight = FontWeights.SemiBold });
        AddCell(grid, 1, BuildNames());
        AddCell(grid, 2, BuildTrack());
        AddCell(grid, 3, statusText);
        root.Child = grid;
        Reset();
    }

    /// <summary>Puts an element in a column of the grid.</summary>
    /// <param name="grid">The grid.</param>
    /// <param name="column">The column number.</param>
    /// <param name="element">The element.</param>
    private static void AddCell(Grid grid, int column, UIElement element)
    {
        Grid.SetColumn(element, column);
        grid.Children.Add(element);
    }

    /// <summary>Builds the name and the one-sentence detail of the phase.</summary>
    /// <returns>The element.</returns>
    private UIElement BuildNames()
    {
        StackPanel names = new() { VerticalAlignment = VerticalAlignment.Center };
        names.Children.Add(new TextBlock { Text = phase.Name, FontWeight = FontWeights.SemiBold });
        names.Children.Add(new TextBlock { Text = phase.Detail, Foreground = DetailBrush, FontSize = 11.5, TextWrapping = TextWrapping.Wrap });
        return names;
    }

    /// <summary>Builds the progress track; the fill inside it is scaled by the animation.</summary>
    /// <returns>The element.</returns>
    private UIElement BuildTrack()
    {
        return new Border { Height = 10, Background = TrackBrush, VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(8, 0, 16, 0), ClipToBounds = true, Child = fill };
    }

    /// <summary>Makes a frozen brush from a colour text.</summary>
    /// <param name="hex">A colour such as "#2563EB".</param>
    /// <returns>The brush.</returns>
    private static Brush Frozen(string hex)
    {
        SolidColorBrush brush = new((Color)ColorConverter.ConvertFromString(hex)!);
        brush.Freeze();
        return brush;
    }
}

/// <summary>One row of the results table, with every number already formatted as text.</summary>
internal sealed class PhaseResultRow
{
    /// <summary>Phase number and name.</summary>
    public string Name { get; init; } = "";
    /// <summary>How the phase ended.</summary>
    public string Status { get; init; } = "";
    /// <summary>Samples used.</summary>
    public string Samples { get; init; } = "";
    /// <summary>Seconds between the first and the last used sample.</summary>
    public string Seconds { get; init; } = "";
    /// <summary>Battery charge used in mWh.</summary>
    public string Drop { get; init; } = "";
    /// <summary>Power from the IOCTL in watts.</summary>
    public string WattsIoctl { get; init; } = "";
    /// <summary>Power from WMI in watts.</summary>
    public string WattsWmi { get; init; } = "";
    /// <summary>Mean CPU use.</summary>
    public string Cpu { get; init; } = "";
    /// <summary>Mean CPU package power from Intel DTT.</summary>
    public string CpuPackage { get; init; } = "";
    /// <summary>Mean CPU package power from the Windows Energy Meter.</summary>
    public string EmiPackage { get; init; } = "";
    /// <summary>Package energy from the Windows Energy Meter in mWh.</summary>
    public string EmiMwh { get; init; } = "";
    /// <summary>Mean GPU 3D use.</summary>
    public string Gpu { get; init; } = "";
    /// <summary>Mean screen brightness.</summary>
    public string Brightness { get; init; } = "";
    /// <summary>Mean hottest thermal zone.</summary>
    public string ZoneTemp { get; init; } = "";
    /// <summary>Mean Intel DTT package temperature.</summary>
    public string PackageTemp { get; init; } = "";
    /// <summary>Share of the laptop power that the processor package uses.</summary>
    public string Share { get; init; } = "";
    /// <summary>Warnings of the phase.</summary>
    public string Warning { get; init; } = "";

    /// <summary>Formats a phase result for the table.</summary>
    /// <param name="r">The result.</param>
    /// <returns>The row.</returns>
    public static PhaseResultRow From(PhaseResult r)
    {
        return new PhaseResultRow
        {
            Name = $"{r.Phase.Number}. {r.Phase.Name}",
            Status = PhaseStatusTexts.Describe(r.Status),
            Samples = r.SampleCount.ToString(CultureInfo.InvariantCulture),
            Seconds = Fmt.Number(r.Seconds, "0"),
            Drop = Fmt.Number(r.DropMwh, "0.0"),
            WattsIoctl = Fmt.Number(r.IoctlWatts, "0.00"),
            WattsWmi = Fmt.Number(r.WmiWatts, "0.00"),
            Cpu = Fmt.Number(r.MeanCpuPercent, "0.0"),
            CpuPackage = Fmt.Number(r.MeanCpuPackageWatts, "0.00"),
            EmiPackage = Fmt.Number(r.MeanEmiPackageWatts, "0.00"),
            EmiMwh = Fmt.Number(r.EmiPackageMwh, "0.0"),
            Gpu = Fmt.Number(r.MeanGpuPercent, "0.0"),
            Brightness = Fmt.Number(r.MeanBrightness, "0"),
            ZoneTemp = Fmt.Number(r.MeanZoneTempC, "0.0"),
            PackageTemp = Fmt.Number(r.MeanPackageTempC, "0.0"),
            Share = Fmt.Number(r.CpuShare * 100, "0"),
            Warning = r.Warning
        };
    }
}

/// <summary>Decides whether the Start button may be pressed.</summary>
internal static class StartGate
{
    /// <summary>Checks the latest sample.</summary>
    /// <param name="sample">The latest sample, or null before the first one.</param>
    /// <returns>Whether the test may start, and the message to show.</returns>
    public static (bool CanStart, string Message) Evaluate(Sample? sample)
    {
        if (sample == null) { return (false, "Reading the battery..."); }
        bool? onCharger = sample.OnCharger();
        if (onCharger == null) { return (false, "The battery state cannot be read, so the test cannot start."); }
        if (onCharger == true) { return (false, "Unplug the charger to start"); }
        if (!sample.Has(SampleKeys.BatteryMwh) && !sample.Has(SampleKeys.WmiMwh)) { return (false, "The battery charge cannot be read, so the test cannot measure anything."); }
        if (sample.BatteryPercent < TestSettings.MinimumStartBatteryPercent) { return (false, $"The battery is below {TestSettings.MinimumStartBatteryPercent}%. Charge it first, then unplug the charger."); }
        return (true, "Ready. Press Start.");
    }
}

/// <summary>The text that tells the user what to do before the test.</summary>
internal static class InstructionTexts
{
    /// <summary>Builds the instruction text from the settings.</summary>
    /// <returns>The text, one numbered step per line.</returns>
    public static string Build()
    {
        int totalSeconds = PhaseCatalogue.All.Sum(phase => phase.Seconds);
        double minutes = Math.Ceiling(totalSeconds / 60.0);
        string offMinutes = (TestSettings.PhaseSeconds / 60.0).ToString("0.#", CultureInfo.InvariantCulture);
        return string.Join(Environment.NewLine,
            "1. Unplug the charger. The test measures how fast the battery empties.",
            "2. Close other programs. Do not touch the laptop during the test: no mouse, no keyboard. Lowest brightness still lights the screen; the screen goes dark only in the last phase.",
            $"3. The whole test takes about {minutes:0} minutes. The screen brightness changes, a full-screen animation plays, and in the last phase the display turns off by itself for {offMinutes} minutes. It switches on again at the end.",
            "4. Press Start. Stop restores the brightness and the display at once.");
    }
}

/// <summary>Helps to restart the program with administrator rights.</summary>
internal static class Elevation
{
    /// <summary>Tells whether the program runs as administrator.</summary>
    /// <returns>True when elevated.</returns>
    public static bool IsElevated()
    {
        using WindowsIdentity identity = WindowsIdentity.GetCurrent();
        return new WindowsPrincipal(identity).IsInRole(WindowsBuiltInRole.Administrator);
    }

    /// <summary>Starts a new copy of the program with the "runas" verb, which shows the Windows permission prompt.</summary>
    /// <exception cref="System.ComponentModel.Win32Exception">The user refused the prompt.</exception>
    public static void RestartElevated()
    {
        string path = Environment.ProcessPath ?? throw new InvalidOperationException("the program path is unknown");
        Process.Start(new ProcessStartInfo(path) { Verb = "runas", UseShellExecute = true });
    }
}
