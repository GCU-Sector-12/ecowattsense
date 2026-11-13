// ResultWriter.cs: writes the four result files of a run: samples.csv, phases.csv, model.json and summary.txt.
// Who uses it: MainWindow, once, when a run ends (also when it was stopped, so no measurement is lost).
// Privacy: the files hold the maker, model and processor name only. No computer name, user name, serial number or device id.
// Numbers are written with the invariant culture (a dot as the decimal mark), so any program can read them.
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace EnergyCalibrator;

/// <summary>Everything that goes into the result files.</summary>
/// <param name="Machine">Maker, model and processor.</param>
/// <param name="Outcome">Samples, statuses and how the run ended.</param>
/// <param name="Results">One result per phase.</param>
/// <param name="Model">The power model.</param>
internal sealed record RunReport(MachineInfo Machine, TestOutcome Outcome, IReadOnlyList<PhaseResult> Results, PowerModel Model);

/// <summary>Creates the folder for one run: results\yyyyMMdd-HHmmss next to the program.</summary>
internal static class ResultFolder
{
    /// <summary>Gives the folder that holds all result folders.</summary>
    /// <returns>The path of the "results" folder next to the program.</returns>
    public static string Root() => Path.Combine(AppContext.BaseDirectory, "results");

    /// <summary>Creates the folder of a new run.</summary>
    /// <returns>The path of the new folder.</returns>
    /// <exception cref="IOException">The folder cannot be created (for example the program sits in a read-only place).</exception>
    public static string CreateNew()
    {
        string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture);
        string folder = Path.Combine(Root(), stamp);
        Directory.CreateDirectory(folder);
        return folder;
    }
}

/// <summary>Writes the result files.</summary>
internal static class ResultWriter
{
    private static readonly UTF8Encoding Utf8 = new(false);

    /// <summary>Writes the final files into a folder.</summary>
    /// <param name="folder">The run folder.</param>
    /// <param name="report">The data to write.</param>
    /// <param name="writeSamples">False when samples.csv grew during the run and is complete, so it is kept as it is.</param>
    /// <exception cref="IOException">A file cannot be written.</exception>
    public static void Save(string folder, RunReport report, bool writeSamples)
    {
        if (writeSamples) { File.WriteAllText(Path.Combine(folder, "samples.csv"), SamplesCsv.Build(report.Outcome.Samples), Utf8); }
        File.WriteAllText(Path.Combine(folder, "phases.csv"), PhasesCsv.Build(report.Results), Utf8);
        File.WriteAllText(Path.Combine(folder, "model.json"), ModelJson.Build(report), Utf8);
        File.WriteAllText(Path.Combine(folder, "summary.txt"), SummaryText.Build(report), Utf8);
    }
}

/// <summary>Small helpers for CSV text.</summary>
internal static class Csv
{
    /// <summary>Writes a number with the invariant culture, or an empty field when it is unknown.</summary>
    /// <param name="value">The number, or null.</param>
    /// <returns>The shortest text that reads back to the same number.</returns>
    public static string Num(double? value) => value.HasValue ? value.Value.ToString("R", CultureInfo.InvariantCulture) : "";

    /// <summary>Puts a text in quotes and doubles any quote inside it.</summary>
    /// <param name="text">The text.</param>
    /// <returns>The quoted text.</returns>
    public static string Quote(string text) => "\"" + text.Replace("\"", "\"\"") + "\"";
}

/// <summary>Builds samples.csv: one row per sample, one column per source value.</summary>
internal static class SamplesCsv
{
    // The columns that are known before the run starts. The file that grows during the run has only these, because a row
    // that is already on disk cannot gain a column later.
    private static readonly List<string> FixedColumns = SampleKeys.CsvOrder.ToList();

    /// <summary>Builds the file text.</summary>
    /// <param name="samples">All samples of the run, including the ones before the first phase.</param>
    /// <returns>The CSV text with a header row.</returns>
    public static string Build(IReadOnlyList<Sample> samples)
    {
        List<string> columns = Columns(samples);
        StringBuilder text = new();
        text.AppendLine(Header(columns));
        foreach (Sample sample in samples) { text.AppendLine(Row(sample, columns)); }
        return text.ToString();
    }

    /// <summary>Gives the header row of the file that grows during the run (the fixed columns only), without a line break.</summary>
    /// <returns>The header text.</returns>
    public static string FixedHeader() => Header(FixedColumns);

    /// <summary>Gives the row of one sample for the file that grows during the run (the fixed columns only), without a line break.</summary>
    /// <param name="sample">The sample.</param>
    /// <returns>The row text.</returns>
    public static string FixedRow(Sample sample) => Row(sample, FixedColumns);

    /// <summary>Tells whether the samples hold values that the fixed columns do not cover (DTT participants, thermal zones, Energy Meter).</summary>
    /// <param name="samples">All samples.</param>
    /// <returns>True when a full file would have more columns than the growing file.</returns>
    public static bool HasExtraColumns(IReadOnlyList<Sample> samples) => Columns(samples).Count > FixedColumns.Count;

    /// <summary>Builds the header row.</summary>
    /// <param name="columns">The value columns.</param>
    /// <returns>The header text.</returns>
    private static string Header(List<string> columns)
    {
        return "time_local,monotonic_s,phase,seconds_into_phase,on_charger,display_state," + string.Join(",", columns);
    }

    /// <summary>Lists the columns: the fixed ones first, then every other value that was seen, in alphabetical order.</summary>
    /// <param name="samples">All samples.</param>
    /// <returns>The column names.</returns>
    private static List<string> Columns(IReadOnlyList<Sample> samples)
    {
        HashSet<string> seen = samples.SelectMany(sample => sample.Values.Keys).ToHashSet();
        IEnumerable<string> extra = seen.Except(SampleKeys.CsvOrder).Order(StringComparer.Ordinal);
        return SampleKeys.CsvOrder.Concat(extra).ToList();
    }

    /// <summary>Builds one row.</summary>
    /// <param name="sample">The sample.</param>
    /// <param name="columns">The value columns.</param>
    /// <returns>The row text.</returns>
    private static string Row(Sample sample, List<string> columns)
    {
        string time = sample.Time.ToString("yyyy-MM-dd HH:mm:ss.fff", CultureInfo.InvariantCulture);
        string start = string.Join(",", time, Csv.Num(sample.MonotonicSeconds), sample.PhaseNumber.ToString(CultureInfo.InvariantCulture), Csv.Num(sample.SecondsIntoPhase), ChargerFlag(sample), DisplayStateTexts.Describe(sample.DisplayState));
        return start + "," + string.Join(",", columns.Select(column => Csv.Num(sample.Get(column))));
    }

    /// <summary>Gives 1 on the charger, 0 on battery, empty when unknown.</summary>
    /// <param name="sample">The sample.</param>
    /// <returns>The field text.</returns>
    private static string ChargerFlag(Sample sample)
    {
        bool? charger = sample.OnCharger();
        if (charger == null) { return ""; }
        return charger.Value ? "1" : "0";
    }
}

/// <summary>Builds phases.csv: one row per phase.</summary>
internal static class PhasesCsv
{
    /// <summary>Builds the file text.</summary>
    /// <param name="results">One result per phase.</param>
    /// <returns>The CSV text with a header row.</returns>
    public static string Build(IReadOnlyList<PhaseResult> results)
    {
        StringBuilder text = new();
        text.AppendLine(Header());
        foreach (PhaseResult result in results) { text.AppendLine(Row(result)); }
        return text.ToString();
    }

    /// <summary>Gives the header row, without a line break.</summary>
    /// <returns>The header text.</returns>
    public static string Header()
    {
        return "phase,name,status,target_brightness_pct,samples_used,seconds,battery_drop_mwh,battery_w_ioctl,battery_w_wmi,battery_w_used,wmi_drop_mwh,mean_cpu_util_pct,mean_cpu_package_w_dtt,mean_emi_pkg_w,mean_emi_cores_w,mean_emi_gpu_w,mean_emi_dram_w,emi_package_mwh,mean_gpu_3d_pct,mean_brightness_pct,mean_thermal_zone_max_c,mean_dtt_package_c,cpu_share_pct,display_off_pct,warning";
    }

    /// <summary>Builds one row, without a line break.</summary>
    /// <param name="r">The phase result.</param>
    /// <returns>The row text.</returns>
    public static string Row(PhaseResult r)
    {
        string[] fields =
        [
            r.Phase.Number.ToString(CultureInfo.InvariantCulture), Csv.Quote(r.Phase.Name), Csv.Quote(PhaseStatusTexts.Describe(r.Status)),
            Csv.Num(r.Phase.BrightnessPercent), r.SampleCount.ToString(CultureInfo.InvariantCulture), Csv.Num(r.Seconds), Csv.Num(r.DropMwh),
            Csv.Num(r.IoctlWatts), Csv.Num(r.WmiWatts), Csv.Num(r.BatteryWatts), Csv.Num(r.WmiDropMwh),
            Csv.Num(r.MeanCpuPercent), Csv.Num(r.MeanCpuPackageWatts), Csv.Num(r.MeanEmiPackageWatts), Csv.Num(r.MeanEmiCoresWatts),
            Csv.Num(r.MeanEmiGpuWatts), Csv.Num(r.MeanEmiDramWatts), Csv.Num(r.EmiPackageMwh), Csv.Num(r.MeanGpuPercent), Csv.Num(r.MeanBrightness),
            Csv.Num(r.MeanZoneTempC), Csv.Num(r.MeanPackageTempC), Csv.Num(r.CpuShare * 100), Csv.Num(r.DisplayOffShare * 100), Csv.Quote(r.Warning)
        ];
        return string.Join(",", fields);
    }
}

/// <summary>Builds model.json.</summary>
internal static class ModelJson
{
    private static readonly JsonSerializerOptions Options = new() { WriteIndented = true };

    /// <summary>Builds the file text.</summary>
    /// <param name="report">The data of the run.</param>
    /// <returns>The JSON text. Unknown values are null.</returns>
    public static string Build(RunReport report)
    {
        PowerModel m = report.Model;
        var document = new
        {
            generated = DateTime.Now.ToString("yyyy-MM-ddTHH:mm:ss", CultureInfo.InvariantCulture),
            machine = new
            {
                maker = report.Machine.Maker,
                model = report.Machine.Model,
                cpu = report.Machine.Cpu,
                logical_processors = report.Machine.LogicalProcessors,
                windows = report.Machine.Windows
            },
            test = new
            {
                ended = report.Outcome.ReasonText,
                phase_seconds = TestSettings.PhaseSeconds,
                settle_seconds = TestSettings.SettleSeconds
            },
            model = new
            {
                base_w = m.BaseWatts,
                screen_w_per_pct = m.ScreenWattsPerPercent,
                screen_w_at_50 = m.ScreenWattsAt50,
                cpu_w_per_pct = m.CpuWattsPerPercent,
                cpu_w_per_pct_from_cpu50 = m.CpuWattsPerPercentFromHalf,
                graphics_w = m.GraphicsWatts
            },
            phase_power_w = report.Results.ToDictionary(r => r.Phase.Name, r => r.BatteryWatts),
            cpu_share_by_phase = report.Results.ToDictionary(r => r.Phase.Name, r => r.CpuShare),
            warnings = m.Warnings
        };
        return JsonSerializer.Serialize(document, Options);
    }
}

/// <summary>Builds summary.txt, a plain-English description of the run.</summary>
internal static class SummaryText
{
    /// <summary>Builds the file text.</summary>
    /// <param name="report">The data of the run.</param>
    /// <returns>The text.</returns>
    public static string Build(RunReport report)
    {
        List<string> lines = new();
        AddHeader(lines, report);
        AddMethod(lines);
        AddPhases(lines, report.Results);
        AddProcessorEnergy(lines, report.Results);
        AddModel(lines, report.Model);
        AddSources(lines, report.Outcome.Samples);
        AddWarnings(lines, report);
        return string.Join(Environment.NewLine, lines) + Environment.NewLine;
    }

    /// <summary>Adds the title, the machine and how the run ended.</summary>
    /// <param name="lines">The text lines.</param>
    /// <param name="report">The data of the run.</param>
    private static void AddHeader(List<string> lines, RunReport report)
    {
        lines.Add("Energy Calibrator: result summary");
        lines.Add($"Date: {report.Outcome.StartedAt.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture)} (local time)");
        lines.Add($"Computer: {report.Machine.Maker} {report.Machine.Model}");
        lines.Add($"Processor: {report.Machine.Cpu} ({report.Machine.LogicalProcessors} logical processors)");
        lines.Add($"Windows: {report.Machine.Windows}");
        lines.Add($"How the test ended: {report.Outcome.ReasonText}");
        lines.Add("");
    }

    /// <summary>Adds a short explanation of how the numbers were measured.</summary>
    /// <param name="lines">The text lines.</param>
    private static void AddMethod(List<string> lines)
    {
        lines.Add("How the numbers were measured");
        lines.Add($"Each phase ran for {TestSettings.PhaseSeconds} seconds. The first {TestSettings.SettleSeconds} seconds were ignored while the laptop settled.");
        lines.Add("The power of a phase is the fall of the battery charge divided by the time: watts = mWh x 3.6 / seconds.");
        lines.Add("It is the power of the whole laptop. The model below is made by subtracting the powers of the phases.");
        lines.Add("");
    }

    /// <summary>Adds one line for each phase.</summary>
    /// <param name="lines">The text lines.</param>
    /// <param name="results">One result per phase.</param>
    private static void AddPhases(List<string> lines, IReadOnlyList<PhaseResult> results)
    {
        lines.Add("Phase results");
        foreach (PhaseResult result in results) { lines.Add(PhaseLine(result)); }
        lines.Add("");
    }

    /// <summary>Describes one phase in one line.</summary>
    /// <param name="r">The phase result.</param>
    /// <returns>The line.</returns>
    private static string PhaseLine(PhaseResult r)
    {
        string power = Fmt.WithUnit(r.BatteryWatts, "0.00", "W");
        string drop = r.DropMwh.HasValue ? $"battery fell {r.DropMwh.Value.ToString("0", CultureInfo.InvariantCulture)} mWh in {Fmt.Number(r.Seconds, "0")} s" : "no battery drop measured";
        string share = r.CpuShare.HasValue ? $"; processor package {Fmt.Number(r.CpuShare * 100, "0")}% of it ({r.CpuShareSource})" : "";
        return $"{r.Phase.Number}. {r.Phase.Name}: {power} ({drop}; CPU use {Fmt.Number(r.MeanCpuPercent, "0")}%; brightness {Fmt.Number(r.MeanBrightness, "0")}%; display off in {Fmt.Number(r.DisplayOffShare * 100, "0")}% of samples){share} [{PhaseStatusTexts.Describe(r.Status)}]";
    }

    /// <summary>Adds the processor energy of each phase from the Windows Energy Meter, next to the battery drop, and says which source the CPU share used.</summary>
    /// <param name="lines">The text lines.</param>
    /// <param name="results">One result per phase.</param>
    private static void AddProcessorEnergy(List<string> lines, IReadOnlyList<PhaseResult> results)
    {
        lines.Add("Processor energy by phase (Windows Energy Meter, a normal user may read it)");
        lines.Add("The package value is the whole processor package and already holds the cores and the graphics; do not add the values up.");
        lines.Add("The CPU share of a phase uses the package power of the Windows Energy Meter when it was read, otherwise Intel DTT; each phase line above names the source.");
        foreach (PhaseResult result in results) { lines.Add(EnergyLine(result)); }
        lines.Add("");
    }

    /// <summary>Describes the Energy Meter values of one phase in one line.</summary>
    /// <param name="r">The phase result.</param>
    /// <returns>The line.</returns>
    private static string EnergyLine(PhaseResult r)
    {
        string watts = $"package {Fmt.WithUnit(r.MeanEmiPackageWatts, "0.00", "W")}, cores {Fmt.WithUnit(r.MeanEmiCoresWatts, "0.00", "W")}, graphics {Fmt.WithUnit(r.MeanEmiGpuWatts, "0.00", "W")}, memory {Fmt.WithUnit(r.MeanEmiDramWatts, "0.00", "W")}";
        string energy = $"package energy {Fmt.WithUnit(r.EmiPackageMwh, "0.0", "mWh")} against battery drop {Fmt.WithUnit(r.DropMwh, "0.0", "mWh")}";
        return $"{r.Phase.Number}. {r.Phase.Name}: {watts}; {energy}";
    }

    /// <summary>Adds the model lines.</summary>
    /// <param name="lines">The text lines.</param>
    /// <param name="model">The power model.</param>
    private static void AddModel(List<string> lines, PowerModel model)
    {
        lines.Add("Power model of this laptop");
        lines.AddRange(model.DescribeLines());
        lines.Add("");
    }

    /// <summary>Adds the sources that could not be read, with the reason and how often.</summary>
    /// <param name="lines">The text lines.</param>
    /// <param name="samples">All samples.</param>
    private static void AddSources(List<string> lines, IReadOnlyList<Sample> samples)
    {
        lines.Add("Sources that could not be read");
        var problems = samples.SelectMany(sample => sample.Unavailable).GroupBy(pair => pair.Key).OrderBy(group => group.Key, StringComparer.Ordinal).ToList();
        if (problems.Count == 0)
        {
            lines.Add("All sources could be read.");
            lines.Add("");
            return;
        }
        foreach (var problem in problems) { lines.Add($"- {problem.Key}: missing in {problem.Count()} of {samples.Count} samples ({problem.First().Value})"); }
        lines.Add("");
    }

    /// <summary>Adds the warnings of the phases, of the model and of the clean-up.</summary>
    /// <param name="lines">The text lines.</param>
    /// <param name="report">The data of the run.</param>
    private static void AddWarnings(List<string> lines, RunReport report)
    {
        List<string> warnings = report.Results.Where(r => r.Warning.Length > 0).Select(r => $"Phase {r.Phase.Number} ({r.Phase.Name}): {r.Warning}").ToList();
        warnings.AddRange(report.Model.Warnings);
        warnings.AddRange(report.Outcome.RestoreProblems);
        lines.Add("Warnings");
        lines.AddRange(warnings.Count == 0 ? ["None."] : warnings.Select(warning => $"- {warning}"));
    }
}
