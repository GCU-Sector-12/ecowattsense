// ServiceModel.cs: writes the schema 2 model file that the EcoWattSense service reads (SPEC section 8), and holds the exit codes
// and the --data-dir argument of the calibrator.
// Who uses it: MainWindow calls ServiceModel.Publish when a run ends; App reads the data folder and sets the exit code;
// the tests in tests\CalibratorTests call the same methods.
// Where it writes: <data>\calibration\model.json. <data> is C:\ProgramData\EcoWattSense, or the folder given with --data-dir.
// Safety: the service treats the file as untrusted and drops the whole model when one value is outside its limits. So the same
// limits are checked here first (ModelLimits.Check); a model that would be dropped is not written at all.
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace EnergyCalibrator;

/// <summary>The exit codes of the calibrator (SPEC section 8). Setup reads them.</summary>
internal static class ExitCodes
{
    /// <summary>The model was written.</summary>
    public const int Written = 0;

    /// <summary>The user stopped the test or closed the program.</summary>
    public const int StoppedByUser = 1;

    /// <summary>The start conditions were not met (no battery, charger connected, charge below the minimum), so no test ran.</summary>
    public const int StartNotMet = 2;

    /// <summary>The test was aborted, or its model is outside the limits of the service.</summary>
    public const int TestAborted = 3;

    /// <summary>The model file could not be written.</summary>
    public const int NotWritable = 4;

    /// <summary>The code the program ends with. Starts as "start conditions not met" and is replaced when the window closes or a run ends.</summary>
    public static int Current { get; set; } = StartNotMet;
}

/// <summary>What happened to the model file after a run.</summary>
/// <param name="ExitCode">The exit code that belongs to it.</param>
/// <param name="Message">One sentence for the status line of the window.</param>
internal sealed record ModelPublication(int ExitCode, string Message);

/// <summary>Reads the command line.</summary>
internal static class CommandLine
{
    /// <summary>Gives the folder that holds the "calibration" folder: the value after "--data-dir", or the standard data folder.</summary>
    /// <param name="args">The program arguments.</param>
    /// <returns>The data folder. The standard one when the argument is missing or has no value.</returns>
    public static string DataDir(IReadOnlyList<string> args)
    {
        int at = IndexOf(args, "--data-dir");
        if (at < 0 || at + 1 >= args.Count) { return ServiceModel.StandardDataDir(); }
        return args[at + 1];
    }

    /// <summary>Finds an argument.</summary>
    /// <param name="args">The program arguments.</param>
    /// <param name="name">The argument to find (compared without regard to case).</param>
    /// <returns>Its position, or -1.</returns>
    private static int IndexOf(IReadOnlyList<string> args, string name)
    {
        return args.ToList().FindIndex(arg => string.Equals(arg, name, StringComparison.OrdinalIgnoreCase));
    }
}

/// <summary>The limits that the service checks (native model.cpp): watt values 0 to 500, per-percent values 0 to 5, the file at most 64 KB.</summary>
internal static class ModelLimits
{
    /// <summary>Upper limit of a watt value.</summary>
    public const double MaxWatts = 500;

    /// <summary>Upper limit of a watts-per-percent value.</summary>
    public const double MaxWattsPerPercent = 5;

    /// <summary>Upper limit of the file size in bytes.</summary>
    public const int MaxFileBytes = 64 * 1024;

    /// <summary>Checks the seven numbers of a model.</summary>
    /// <param name="model">The model.</param>
    /// <returns>One line for every value that is unknown, not a number or outside its limit; empty when the model is valid.</returns>
    public static IReadOnlyList<string> Check(PowerModel model)
    {
        List<string> problems = new();
        Check(problems, "base_w", model.BaseWatts, MaxWatts);
        Check(problems, "rest_w", model.RestWatts, MaxWatts);
        Check(problems, "rest_w_per_cpu_pct", model.RestWattsPerPercent, MaxWattsPerPercent);
        Check(problems, "cpu_w_per_pct", model.CpuWattsPerPercent, MaxWattsPerPercent);
        Check(problems, "screen_w_at_50", model.ScreenWattsAt50, MaxWatts);
        Check(problems, "screen_w_per_pct", model.ScreenWattsPerPercent, MaxWattsPerPercent);
        Check(problems, "graphics_w", model.GraphicsWatts, MaxWatts);
        return problems;
    }

    /// <summary>Checks one value and adds a line when it is not valid.</summary>
    /// <param name="problems">The list to add to.</param>
    /// <param name="name">The field name in the file.</param>
    /// <param name="value">The value, or null when unknown.</param>
    /// <param name="max">The upper limit.</param>
    private static void Check(List<string> problems, string name, double? value, double max)
    {
        if (!value.HasValue) { problems.Add($"{name} is unknown"); return; }
        if (double.IsNaN(value.Value) || double.IsInfinity(value.Value)) { problems.Add($"{name} is not a number"); return; }
        if (value.Value < 0 || value.Value > max) { problems.Add($"{name} is {value.Value.ToString("0.####", CultureInfo.InvariantCulture)}, outside 0 to {max.ToString("0", CultureInfo.InvariantCulture)}"); }
    }
}

/// <summary>Builds and writes the model file of the service.</summary>
internal static class ServiceModel
{
    private static readonly UTF8Encoding Utf8 = new(false);
    private static readonly JsonSerializerOptions Options = new() { WriteIndented = true };

    /// <summary>Gives the standard data folder, C:\ProgramData\EcoWattSense on a normal Windows.</summary>
    /// <returns>The folder path.</returns>
    public static string StandardDataDir() => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "EcoWattSense");

    /// <summary>Builds the text of the schema 2 file.</summary>
    /// <param name="report">The data of the run.</param>
    /// <param name="utcNow">The time of writing; it is cut to whole seconds.</param>
    /// <returns>The JSON text. Only call it for a model that passed ModelLimits.Check.</returns>
    public static string BuildJson(RunReport report, DateTime utcNow)
    {
        PowerModel m = report.Model;
        var document = new
        {
            schema = 2,
            generated_utc = utcNow.ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ss'Z'", CultureInfo.InvariantCulture),
            machine = new
            {
                maker = report.Machine.Maker,
                model = report.Machine.Model,
                cpu = report.Machine.Cpu,
                logical_processors = report.Machine.LogicalProcessors,
                windows = report.Machine.Windows
            },
            measured_parts = m.MeasuredParts,
            model = new
            {
                base_w = m.BaseWatts,
                rest_w = m.RestWatts,
                rest_w_per_cpu_pct = m.RestWattsPerPercent,
                cpu_w_per_pct = m.CpuWattsPerPercent,
                screen_w_at_50 = m.ScreenWattsAt50,
                screen_w_per_pct = m.ScreenWattsPerPercent,
                graphics_w = m.GraphicsWatts
            },
            phase_power_w = report.Results.ToDictionary(r => r.Phase.Name, r => r.BatteryWatts),
            cpu_share_by_phase = report.Results.ToDictionary(r => r.Phase.Name, r => r.CpuShare),
            warnings = m.Warnings
        };
        return JsonSerializer.Serialize(document, Options);
    }

    /// <summary>Decides what to do with the model after a run, and writes the file when it may be written.</summary>
    /// <param name="report">The data of the run.</param>
    /// <param name="dataDir">The data folder (the one that holds "calibration").</param>
    /// <param name="utcNow">The time of writing.</param>
    /// <returns>The exit code and a sentence for the window. Nothing is written for a stopped or aborted run, or a model outside the limits.</returns>
    public static ModelPublication Publish(RunReport report, string dataDir, DateTime utcNow)
    {
        if (report.Outcome.Reason == AbortReason.UserStop) { return new ModelPublication(ExitCodes.StoppedByUser, "The test was stopped, so no model was written for the service."); }
        if (report.Outcome.Reason != AbortReason.None) { return new ModelPublication(ExitCodes.TestAborted, "The test was aborted, so no model was written for the service."); }
        IReadOnlyList<string> problems = ModelLimits.Check(report.Model);
        if (problems.Count > 0) { return new ModelPublication(ExitCodes.TestAborted, $"No model was written for the service: {string.Join("; ", problems)}."); }
        string json = BuildJson(report, utcNow);
        if (Utf8.GetByteCount(json) > ModelLimits.MaxFileBytes) { return new ModelPublication(ExitCodes.TestAborted, "No model was written for the service: the file would be larger than 64 KB."); }
        return Write(dataDir, json);
    }

    /// <summary>Writes the model file: copies the current one to model.previous.json, writes a temporary file, and renames it.</summary>
    /// <param name="dataDir">The data folder.</param>
    /// <param name="json">The file text.</param>
    /// <returns>Code 0 when written; code 4 when a file or folder cannot be written (no permission, a locked file). The current file is then unchanged.</returns>
    public static ModelPublication Write(string dataDir, string json)
    {
        try
        {
            WriteFiles(dataDir, json);
            return new ModelPublication(ExitCodes.Written, "The model for the service was written.");
        }
        catch (IOException) { return NotWritable(); }
        catch (UnauthorizedAccessException) { return NotWritable(); }
    }

    /// <summary>Gives the result for a file that could not be written.</summary>
    /// <returns>Code 4 and its sentence.</returns>
    private static ModelPublication NotWritable() => new(ExitCodes.NotWritable, "The model file for the service could not be written (no permission, or the file is in use).");

    /// <summary>Does the file work of Write. The temporary file is deleted when anything fails, so no half file is left.</summary>
    /// <param name="dataDir">The data folder.</param>
    /// <param name="json">The file text.</param>
    /// <exception cref="IOException">A file is locked or a folder is missing.</exception>
    /// <exception cref="UnauthorizedAccessException">The account may not write there.</exception>
    private static void WriteFiles(string dataDir, string json)
    {
        string folder = Path.Combine(dataDir, "calibration");
        string target = Path.Combine(folder, "model.json");
        string temporary = Path.Combine(folder, "model.json.tmp");
        Directory.CreateDirectory(folder);
        BackUp(target, Path.Combine(folder, "model.previous.json"));
        try
        {
            File.WriteAllText(temporary, json, Utf8);
            File.Move(temporary, target, true);
        }
        finally { File.Delete(temporary); }
    }

    /// <summary>Copies the current model file to the "previous" file. Does nothing at the first run, when there is no model yet.</summary>
    /// <param name="target">The current file.</param>
    /// <param name="previous">The file to copy to.</param>
    private static void BackUp(string target, string previous)
    {
        if (!File.Exists(target)) { return; }
        File.Copy(target, previous, true);
    }
}
