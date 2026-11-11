// Analysis.cs: turns the samples of each phase into a result (PhaseAnalyser, PhaseResult) and the results into the power model (PowerModel).
// Who uses it: MainWindow after a run, and ResultWriter for the files.
// How it works: only samples taken after the settling time of a phase are used. Power is the fall of the battery charge over time:
// watts = mWh x 3.6 / seconds (1 mWh is 3.6 joules). The model uses simple subtraction between phases: no regression, no matrix maths.
// A value that cannot be worked out is null ("unknown"), never 0.
namespace EnergyCalibrator;

/// <summary>The fall of one battery counter between the first and the last used sample of a phase.</summary>
/// <param name="Mwh">Charge that was used, in mWh (first minus last).</param>
/// <param name="Seconds">Time between the first and the last sample.</param>
/// <param name="Watts">Mean power; null when the charge did not fall or there are too few samples.</param>
internal readonly record struct DropMeasurement(double? Mwh, double? Seconds, double? Watts);

/// <summary>The result of one phase.</summary>
internal sealed class PhaseResult
{
    /// <summary>The phase this result belongs to.</summary>
    public required PhaseDefinition Phase { get; init; }

    /// <summary>How the phase ended.</summary>
    public PhaseStatus Status { get; init; }

    /// <summary>Number of samples used (after the settling time).</summary>
    public int SampleCount { get; init; }

    /// <summary>Seconds between the first and the last used battery sample.</summary>
    public double? Seconds { get; init; }

    /// <summary>Battery charge used according to the IOCTL, in mWh.</summary>
    public double? DropMwh { get; init; }

    /// <summary>Mean power of the whole laptop from the IOCTL battery charge, in watts.</summary>
    public double? IoctlWatts { get; init; }

    /// <summary>Battery charge used according to WMI, in mWh (a check on the IOCTL).</summary>
    public double? WmiDropMwh { get; init; }

    /// <summary>Mean power from the WMI battery charge, in watts (a check on the IOCTL).</summary>
    public double? WmiWatts { get; init; }

    /// <summary>Mean CPU use in percent.</summary>
    public double? MeanCpuPercent { get; init; }

    /// <summary>Mean power of the processor package from Intel DTT, in watts.</summary>
    public double? MeanCpuPackageWatts { get; init; }

    /// <summary>Mean use of the busiest GPU 3D engine in percent.</summary>
    public double? MeanGpuPercent { get; init; }

    /// <summary>Mean screen brightness in percent.</summary>
    public double? MeanBrightness { get; init; }

    /// <summary>Mean of the highest thermal zone temperature in degrees Celsius.</summary>
    public double? MeanZoneTempC { get; init; }

    /// <summary>Mean temperature of Intel DTT participant 3 in degrees Celsius.</summary>
    public double? MeanPackageTempC { get; init; }

    /// <summary>Share of the used samples (0 to 1) whose display state was "off"; samples with an unknown state are left out. Null when none is known.</summary>
    public double? DisplayOffShare { get; init; }

    /// <summary>Warnings about this phase, joined with semicolons; empty when there are none.</summary>
    public string Warning { get; set; } = "";

    /// <summary>Power used by the model: the IOCTL value, or the WMI value when the IOCTL gave none.</summary>
    public double? BatteryWatts => IoctlWatts ?? WmiWatts;

    /// <summary>Share of the laptop power that Intel DTT reports for the processor package (0 to 1), or null.</summary>
    public double? CpuShare => MeanCpuPackageWatts.HasValue && BatteryWatts is > 0 ? MeanCpuPackageWatts / BatteryWatts : null;
}

/// <summary>Works out the result of every phase from the samples.</summary>
internal static class PhaseAnalyser
{
    /// <summary>Analyses all phases, also those that did not run.</summary>
    /// <param name="outcome">The samples and statuses of one run.</param>
    /// <returns>One result per phase, in phase order.</returns>
    public static IReadOnlyList<PhaseResult> AnalyseAll(TestOutcome outcome)
    {
        return Enumerable.Range(0, PhaseCatalogue.All.Count).Select(index => Analyse(index, outcome)).ToList();
    }

    /// <summary>Analyses one phase.</summary>
    /// <param name="index">Index of the phase.</param>
    /// <param name="outcome">The samples and statuses of one run (a run that is still going is fine).</param>
    /// <returns>The result, with its warnings.</returns>
    public static PhaseResult Analyse(int index, TestOutcome outcome)
    {
        PhaseDefinition phase = PhaseCatalogue.All[index];
        List<Sample> used = outcome.Samples
            .Where(sample => sample.PhaseNumber == phase.Number && sample.SecondsIntoPhase >= TestSettings.SettleSeconds)
            .ToList();
        DropMeasurement ioctl = MeasureDrop(used, SampleKeys.BatteryMwh);
        DropMeasurement wmi = MeasureDrop(used, SampleKeys.WmiMwh);
        PhaseResult result = new()
        {
            Phase = phase,
            Status = outcome.Statuses[index],
            SampleCount = used.Count,
            Seconds = ioctl.Seconds ?? wmi.Seconds,
            DropMwh = ioctl.Mwh,
            IoctlWatts = ioctl.Watts,
            WmiDropMwh = wmi.Mwh,
            WmiWatts = wmi.Watts,
            MeanCpuPercent = Mean(used, SampleKeys.CpuUtil),
            MeanCpuPackageWatts = Mean(used, SampleKeys.DttPackageWatts),
            MeanGpuPercent = Mean(used, SampleKeys.Gpu3D),
            MeanBrightness = Mean(used, SampleKeys.Brightness),
            MeanZoneTempC = Mean(used, SampleKeys.ThermalMax),
            MeanPackageTempC = Mean(used, SampleKeys.DttPackageTemp),
            DisplayOffShare = OffShare(used)
        };
        result.Warning = PhaseWarnings.Describe(result, outcome.Notes[index]);
        return result;
    }

    /// <summary>Works out the battery drop between the first and the last sample that has the counter.</summary>
    /// <param name="used">The samples after the settling time.</param>
    /// <param name="key">Key of the battery charge counter.</param>
    /// <returns>The drop; all values null when fewer than two samples have the counter.</returns>
    private static DropMeasurement MeasureDrop(List<Sample> used, string key)
    {
        List<Sample> withCharge = used.Where(sample => sample.Has(key)).ToList();
        if (withCharge.Count < 2) { return new DropMeasurement(null, null, null); }
        Sample first = withCharge[0];
        Sample last = withCharge[^1];
        double mwh = first.Values[key] - last.Values[key];
        double seconds = last.MonotonicSeconds - first.MonotonicSeconds;
        double? watts = mwh > 0 && seconds > 0 ? mwh * 3.6 / seconds : null;
        return new DropMeasurement(mwh, seconds, watts);
    }

    /// <summary>Works out the share of samples that were taken while the display was off.</summary>
    /// <param name="used">The samples after the settling time.</param>
    /// <returns>A number from 0 to 1, or null when no sample has a known display state.</returns>
    private static double? OffShare(List<Sample> used)
    {
        List<Sample> known = used.Where(sample => sample.DisplayState != DisplayStateKind.Unknown).ToList();
        if (known.Count == 0) { return null; }
        int off = known.Count(sample => sample.DisplayState == DisplayStateKind.Off);
        return off / (double)known.Count;
    }

    /// <summary>Works out the mean of a value over the samples that have it.</summary>
    /// <param name="used">The samples after the settling time.</param>
    /// <param name="key">The value name.</param>
    /// <returns>The mean, or null when no sample has the value.</returns>
    private static double? Mean(List<Sample> used, string key)
    {
        List<double> values = used.Where(sample => sample.Has(key)).Select(sample => sample.Values[key]).ToList();
        if (values.Count == 0) { return null; }
        return values.Average();
    }
}

/// <summary>Builds the warning text of a phase from its result.</summary>
internal static class PhaseWarnings
{
    /// <summary>Collects every warning that applies to a phase.</summary>
    /// <param name="result">The phase result without warnings.</param>
    /// <param name="note">A note from the runner (for example a brightness that could not be set), or an empty string.</param>
    /// <returns>The warnings joined with semicolons; empty when there are none.</returns>
    public static string Describe(PhaseResult result, string note)
    {
        List<string> warnings = new();
        AddText(warnings, note);
        AddText(warnings, StatusWarning(result));
        AddText(warnings, BatteryWarning(result));
        AddText(warnings, BrightnessWarning(result));
        AddText(warnings, LoadWarning(result));
        return string.Join("; ", warnings);
    }

    /// <summary>Adds a text to a list when it is not empty.</summary>
    /// <param name="list">The list.</param>
    /// <param name="text">The text.</param>
    private static void AddText(List<string> list, string text)
    {
        if (text.Length == 0) { return; }
        list.Add(text);
    }

    /// <summary>Warns when the phase did not run to its end.</summary>
    /// <param name="r">The result.</param>
    /// <returns>A warning or an empty string.</returns>
    private static string StatusWarning(PhaseResult r)
    {
        if (r.Status == PhaseStatus.NotRun) { return "phase did not run"; }
        if (r.Status != PhaseStatus.Complete) { return $"not valid: {PhaseStatusTexts.Describe(r.Status)}"; }
        return "";
    }

    /// <summary>Warns about a missing, tiny or odd battery drop.</summary>
    /// <param name="r">The result.</param>
    /// <returns>A warning or an empty string.</returns>
    private static string BatteryWarning(PhaseResult r)
    {
        if (r.Status == PhaseStatus.NotRun) { return ""; }
        if (r.BatteryWatts == null) { return "no battery power could be worked out (too few samples, or the charge did not fall)"; }
        if (r.IoctlWatts == null) { return "battery power taken from WMI because the IOCTL gave no charge"; }
        if (r.DropMwh < TestSettings.LowDropWarningMwh) { return $"battery fell only {r.DropMwh:0} mWh; the battery steps are coarse, so this power is rough"; }
        return "";
    }

    /// <summary>Warns when the screen brightness was not the one the phase asked for.</summary>
    /// <param name="r">The result.</param>
    /// <returns>A warning or an empty string.</returns>
    private static string BrightnessWarning(PhaseResult r)
    {
        if (r.Status == PhaseStatus.NotRun || r.Phase.BrightnessPercent is not int target) { return ""; }
        if (r.MeanBrightness is not double mean) { return "brightness could not be read, so the screen setting is unchecked"; }
        if (Math.Abs(mean - target) > TestSettings.BrightnessTolerancePoints) { return $"mean brightness {mean:0}% is not the target {target}%"; }
        return "";
    }

    /// <summary>Warns when the CPU or GPU load of the phase did not reach the expected level.</summary>
    /// <param name="r">The result.</param>
    /// <returns>A warning or an empty string.</returns>
    private static string LoadWarning(PhaseResult r)
    {
        if (r.Status == PhaseStatus.NotRun) { return ""; }
        if (r.Phase.Load == PhaseLoad.CpuHalf && !(Math.Abs((r.MeanCpuPercent ?? -1000) - 50) <= TestSettings.CpuHalfTolerancePoints)) { return $"CPU use {Show(r.MeanCpuPercent)} is not near 50%"; }
        if (r.Phase.Load == PhaseLoad.CpuFull && !(r.MeanCpuPercent >= TestSettings.CpuFullMinimumPercent)) { return $"CPU use {Show(r.MeanCpuPercent)} is below {TestSettings.CpuFullMinimumPercent:0}%"; }
        if (r.Phase.Load == PhaseLoad.Graphics && !(r.MeanGpuPercent >= TestSettings.GraphicsMinimumGpuPercent)) { return $"GPU 3D use {Show(r.MeanGpuPercent)} is very low, so the graphics load may not have registered"; }
        return "";
    }

    /// <summary>Shows a percentage for a warning text.</summary>
    /// <param name="value">The value, or null.</param>
    /// <returns>For example "63%", or "unknown".</returns>
    private static string Show(double? value) => value.HasValue ? $"{value.Value:0}%" : "unknown";
}

/// <summary>The power model of the laptop, worked out by subtracting the powers of the phases.</summary>
internal sealed class PowerModel
{
    /// <summary>Power with the display off: everything except the screen.</summary>
    public double? BaseWatts { get; init; }

    /// <summary>Extra power for each percent of brightness, from the 0% and 100% phases.</summary>
    public double? ScreenWattsPerPercent { get; init; }

    /// <summary>Power of the screen at 50% brightness: the reference phase minus the display-off phase.</summary>
    public double? ScreenWattsAt50 { get; init; }

    /// <summary>Extra power for each percent of CPU use, from the CPU 100% phase.</summary>
    public double? CpuWattsPerPercent { get; init; }

    /// <summary>The same from the CPU 50% phase, to show how linear the CPU power is.</summary>
    public double? CpuWattsPerPercentFromHalf { get; init; }

    /// <summary>Extra power of the graphics load over the reference phase.</summary>
    public double? GraphicsWatts { get; init; }

    /// <summary>Notes about the parts of the model that could not be worked out.</summary>
    public IReadOnlyList<string> Warnings => BuildWarnings();

    /// <summary>Works out the model from the phase results.</summary>
    /// <param name="results">One result per phase, in phase order.</param>
    /// <returns>The model. Parts that need a missing or invalid phase are null.</returns>
    public static PowerModel Build(IReadOnlyList<PhaseResult> results)
    {
        double? screenOff = Power(results, PhaseCatalogue.ScreenOff);
        double? screenLow = Power(results, PhaseCatalogue.ScreenLow);
        double? screenHigh = Power(results, PhaseCatalogue.ScreenHigh);
        double? reference = Power(results, PhaseCatalogue.ScreenReference);
        return new PowerModel
        {
            BaseWatts = screenOff,
            ScreenWattsPerPercent = Subtract(screenHigh, screenLow) / 100.0,
            ScreenWattsAt50 = Subtract(reference, screenOff),
            CpuWattsPerPercent = PerPercent(Power(results, PhaseCatalogue.CpuFull), reference, CpuSpan(results, PhaseCatalogue.CpuFull)),
            CpuWattsPerPercentFromHalf = PerPercent(Power(results, PhaseCatalogue.CpuHalf), reference, CpuSpan(results, PhaseCatalogue.CpuHalf)),
            GraphicsWatts = Subtract(Power(results, PhaseCatalogue.Graphics), reference)
        };
    }

    /// <summary>Gives the lines that describe the model in plain English, for the window and for summary.txt.</summary>
    /// <returns>One line per model value.</returns>
    public IReadOnlyList<string> DescribeLines()
    {
        return
        [
            $"Base power (display off, everything except the screen): {Fmt.WithUnit(BaseWatts, "0.00", "W")}",
            $"Screen: {Fmt.WithUnit(ScreenWattsPerPercent, "0.0000", "W")} for each 1% of brightness (from the lowest and the highest brightness)",
            $"Screen at 50% brightness: {Fmt.WithUnit(ScreenWattsAt50, "0.00", "W")} (reference phase minus display off)",
            $"CPU: {Fmt.WithUnit(CpuWattsPerPercent, "0.0000", "W")} for each 1% of CPU use (from the CPU 100% phase)",
            $"CPU, from the CPU 50% phase: {Fmt.WithUnit(CpuWattsPerPercentFromHalf, "0.0000", "W")} for each 1% (close to the line above means the CPU power is linear)",
            $"Graphics: {Fmt.WithUnit(GraphicsWatts, "0.00", "W")} more than the reference phase at the same brightness"
        ];
    }

    /// <summary>Gives the battery power of a phase when the phase is complete.</summary>
    /// <param name="results">All phase results.</param>
    /// <param name="index">Index of the phase.</param>
    /// <returns>The watts, or null for a phase that is not complete or has no power.</returns>
    private static double? Power(IReadOnlyList<PhaseResult> results, int index)
    {
        PhaseResult result = results[index];
        if (result.Status != PhaseStatus.Complete) { return null; }
        return result.BatteryWatts;
    }

    /// <summary>Subtracts two values that may be unknown.</summary>
    /// <param name="a">The first value.</param>
    /// <param name="b">The value to take away.</param>
    /// <returns>a minus b, or null when one of them is unknown.</returns>
    private static double? Subtract(double? a, double? b) => a.HasValue && b.HasValue ? a.Value - b.Value : null;

    /// <summary>Works out the difference in mean CPU use between a phase and the reference phase.</summary>
    /// <param name="results">All phase results.</param>
    /// <param name="index">Index of the busy phase.</param>
    /// <returns>The difference in percentage points, or null when a value is unknown or a phase is not complete.</returns>
    private static double? CpuSpan(IReadOnlyList<PhaseResult> results, int index)
    {
        bool complete = results[index].Status == PhaseStatus.Complete && results[PhaseCatalogue.ScreenReference].Status == PhaseStatus.Complete;
        if (!complete) { return null; }
        return Subtract(results[index].MeanCpuPercent, results[PhaseCatalogue.ScreenReference].MeanCpuPercent);
    }

    /// <summary>Divides the extra power by the extra CPU use.</summary>
    /// <param name="busy">Power of the busy phase.</param>
    /// <param name="rest">Power of the reference phase.</param>
    /// <param name="span">Difference in CPU use in percentage points.</param>
    /// <returns>Watts per percent, or null when a value is unknown or the span is too small to divide by.</returns>
    private static double? PerPercent(double? busy, double? rest, double? span)
    {
        if (span is not double points || points < TestSettings.MinimumCpuSpanPoints) { return null; }
        return Subtract(busy, rest) / points;
    }

    /// <summary>Lists the model values that could not be worked out, with the phases they need.</summary>
    /// <returns>One line per missing value.</returns>
    private List<string> BuildWarnings()
    {
        List<string> warnings = new();
        AddMissing(warnings, BaseWatts, "Base power needs phase 7 (Screen off) to be complete with a battery reading.");
        AddMissing(warnings, ScreenWattsPerPercent, "Screen power per percent needs phases 1 and 2 to be complete.");
        AddMissing(warnings, ScreenWattsAt50, "Screen power at 50% needs phases 3 and 7 to be complete.");
        AddMissing(warnings, CpuWattsPerPercent, "CPU power per percent needs phases 3 and 5 to be complete, with CPU use at least 5 points higher in phase 5.");
        AddMissing(warnings, CpuWattsPerPercentFromHalf, "CPU power per percent from the 50% phase needs phases 3 and 4 to be complete, with CPU use at least 5 points higher in phase 4.");
        AddMissing(warnings, GraphicsWatts, "Graphics power needs phases 3 and 6 to be complete.");
        return warnings;
    }

    /// <summary>Adds a line when a model value is unknown.</summary>
    /// <param name="warnings">The list.</param>
    /// <param name="value">The model value.</param>
    /// <param name="text">The line to add.</param>
    private static void AddMissing(List<string> warnings, double? value, string text)
    {
        if (value.HasValue) { return; }
        warnings.Add(text);
    }
}
