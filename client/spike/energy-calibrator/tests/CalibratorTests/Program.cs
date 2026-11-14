// Program.cs: console tests of the calibrator model (schema 2 file for the service).
// Who uses it: tests\Test-Calibrator.ps1. Exit code = number of failed checks (0 = all passed).
// What it checks: the analysis of the real run of 12 Nov 2026, the rest power, the exact fields of the file, the backup file,
// the exit codes (written, stopped, aborted, not writable), a run without Energy Meter data, and the --data-dir argument.
// Data: data\phases-20261112.csv (copied next to the program). Files are written to a temporary folder that is deleted at the end.
using System.IO;
using System.Text.Json;
using System.Text.RegularExpressions;
using EnergyCalibrator;

namespace CalibratorTests;

/// <summary>The test program.</summary>
internal static class Program
{
    private static int failures;
    private static readonly DateTime FixedTime = new(2026, 11, 14, 10, 30, 5, 987, DateTimeKind.Utc);

    /// <summary>Runs every group of checks.</summary>
    /// <returns>The number of failed checks.</returns>
    private static int Main()
    {
        string data = Path.Combine(AppContext.BaseDirectory, "data", "phases-20261112.csv");
        string temp = Path.Combine(Path.GetTempPath(), "ews-cal-tests-" + Guid.NewGuid().ToString("N"));
        IReadOnlyList<PhaseResult> results = PhaseData.Load(data, false);
        CheckAnalysis(results);
        CheckRest(results);
        CheckJson(results);
        CheckWriteAndBackup(results, Path.Combine(temp, "a"));
        CheckLockedFile(results, Path.Combine(temp, "b"));
        CheckNoEmi(data, Path.Combine(temp, "c"));
        CheckLimitsAndExitCodes(results, Path.Combine(temp, "d"));
        CheckCommandLine();
        RemoveFolder(temp);
        Console.WriteLine(failures == 0 ? "ALL CHECKS PASSED" : $"{failures} CHECK(S) FAILED");
        return failures;
    }

    /// <summary>Records and prints one check.</summary>
    /// <param name="ok">True when the check passed.</param>
    /// <param name="name">What was checked.</param>
    private static void Check(bool ok, string name)
    {
        Console.WriteLine($"{(ok ? "PASS" : "FAIL")}  {name}");
        if (!ok) { failures++; }
    }

    /// <summary>Tells whether a value is within a tolerance of the expected one.</summary>
    /// <param name="actual">The value, or null.</param>
    /// <param name="expected">The expected value.</param>
    /// <param name="tolerance">The allowed difference.</param>
    /// <returns>True when the value is known and close enough.</returns>
    private static bool Near(double? actual, double expected, double tolerance) => actual.HasValue && Math.Abs(actual.Value - expected) <= tolerance;

    /// <summary>Builds the report of a run from phase results.</summary>
    /// <param name="results">The phase results.</param>
    /// <param name="reason">How the run ended.</param>
    /// <returns>The report.</returns>
    private static RunReport Report(IReadOnlyList<PhaseResult> results, AbortReason reason)
    {
        MachineInfo machine = new("Test Maker", "Test Model", "Test CPU", 8, "10.0.0.0");
        TestOutcome outcome = new(FixedTime, new List<Sample>(), results.Select(r => r.Status).ToList(), Enumerable.Repeat("", results.Count).ToList(), reason, "test", new List<string>());
        return new RunReport(machine, outcome, results, PowerModel.Build(results), new PowerRunRecord());
    }

    /// <summary>The model of the 12 Nov data equals the model.json of that run (same code, within 0.01).</summary>
    /// <param name="results">The phase results.</param>
    private static void CheckAnalysis(IReadOnlyList<PhaseResult> results)
    {
        PowerModel model = PowerModel.Build(results);
        Check(Near(model.BaseWatts, 3.477953946540612, 0.01), "base_w equals the 12 Nov model");
        Check(Near(model.CpuWattsPerPercent, 0.07513902946829529, 0.01), "cpu_w_per_pct equals the 12 Nov model");
        Check(Near(model.ScreenWattsAt50, 1.4033889053186281, 0.01), "screen_w_at_50 equals the 12 Nov model");
        Check(Near(model.ScreenWattsPerPercent, 0.010372476612386574, 0.01), "screen_w_per_pct equals the 12 Nov model");
        Check(Near(model.GraphicsWatts, 2.4407283145470515, 0.01), "graphics_w equals the 12 Nov model");
        Check(ModelLimits.Check(model).Count == 0, "the 12 Nov model is inside the limits of the service");
    }

    /// <summary>Rest power = battery power minus package minus memory in the "screen off" phase; measured parts named as the service reads them.</summary>
    /// <param name="results">The phase results.</param>
    private static void CheckRest(IReadOnlyList<PhaseResult> results)
    {
        PowerModel model = PowerModel.Build(results);
        double expected = 3.477953946540612 - 1.239157569827508 - 0.37526723101741777;
        Check(Near(model.RestWatts, expected, 1e-9), "rest_w = battery - package - DRAM in the screen off phase");
        Check(model.MeasuredParts.SequenceEqual(["cpu_package", "dram"]), "measured_parts are cpu_package and dram");
        Check(model.RestWattsPerPercent is > 0 and <= ModelLimits.MaxWattsPerPercent, "rest_w_per_cpu_pct is above 0 and inside the limit");
    }

    /// <summary>The written JSON has exactly the schema 2 fields and a UTC time in whole seconds.</summary>
    /// <param name="results">The phase results.</param>
    private static void CheckJson(IReadOnlyList<PhaseResult> results)
    {
        using JsonDocument doc = JsonDocument.Parse(ServiceModel.BuildJson(Report(results, AbortReason.None), FixedTime));
        JsonElement root = doc.RootElement;
        string[] top = ["schema", "generated_utc", "machine", "measured_parts", "model", "phase_power_w", "cpu_share_by_phase", "warnings"];
        string[] model = ["base_w", "rest_w", "rest_w_per_cpu_pct", "cpu_w_per_pct", "screen_w_at_50", "screen_w_per_pct", "graphics_w"];
        string[] machine = ["maker", "model", "cpu", "logical_processors", "windows"];
        Check(Names(root).SequenceEqual(top), "top-level fields are exactly the schema 2 fields");
        Check(Names(root.GetProperty("model")).SequenceEqual(model), "model fields are exactly the seven numbers");
        Check(Names(root.GetProperty("machine")).SequenceEqual(machine), "machine fields are as before");
        Check(root.GetProperty("schema").GetInt32() == 2, "schema is 2");
        Check(root.GetProperty("generated_utc").GetString() == "2026-11-14T10:30:05Z", "generated_utc is UTC, whole seconds, with Z");
        Check(Regex.IsMatch(root.GetProperty("generated_utc").GetString() ?? "", @"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$"), "generated_utc matches the pattern of the service");
    }

    /// <summary>Lists the property names of a JSON object.</summary>
    /// <param name="element">The object.</param>
    /// <returns>The names in file order.</returns>
    private static IEnumerable<string> Names(JsonElement element) => element.EnumerateObject().Select(property => property.Name);

    /// <summary>The first run writes model.json without a previous file; the second makes model.previous.json from the first.</summary>
    /// <param name="results">The phase results.</param>
    /// <param name="dataDir">A temporary data folder.</param>
    private static void CheckWriteAndBackup(IReadOnlyList<PhaseResult> results, string dataDir)
    {
        string model = Path.Combine(dataDir, "calibration", "model.json");
        string previous = Path.Combine(dataDir, "calibration", "model.previous.json");
        ModelPublication first = ServiceModel.Publish(Report(results, AbortReason.None), dataDir, FixedTime);
        string firstText = File.ReadAllText(model);
        Check(first.ExitCode == 0 && File.Exists(model), "first run writes model.json and gives code 0");
        Check(!File.Exists(previous), "first run (no old model) makes no previous file");
        ModelPublication second = ServiceModel.Publish(Report(results, AbortReason.None), dataDir, FixedTime.AddMinutes(5));
        Check(second.ExitCode == 0 && File.Exists(previous), "second run gives code 0 and makes model.previous.json");
        Check(File.ReadAllText(previous) == firstText, "model.previous.json holds the first file");
        Check(File.ReadAllText(model) != firstText, "model.json holds the new file");
        Check(!File.Exists(model + ".tmp"), "no temporary file is left");
    }

    /// <summary>A model file that cannot be replaced (locked) gives code 4 and stays as it was.</summary>
    /// <param name="results">The phase results.</param>
    /// <param name="dataDir">A temporary data folder.</param>
    private static void CheckLockedFile(IReadOnlyList<PhaseResult> results, string dataDir)
    {
        string folder = Path.Combine(dataDir, "calibration");
        string model = Path.Combine(folder, "model.json");
        Directory.CreateDirectory(folder);
        File.WriteAllText(model, "old");
        FileStream lockHandle = new(model, FileMode.Open, FileAccess.Read, FileShare.None);
        ModelPublication result = ServiceModel.Publish(Report(results, AbortReason.None), dataDir, FixedTime);
        lockHandle.Dispose();
        Check(result.ExitCode == 4, "a locked model.json gives code 4");
        Check(File.ReadAllText(model) == "old", "the locked file is unchanged");
        Check(!File.Exists(model + ".tmp"), "no temporary file is left after the failure");
    }

    /// <summary>Without Energy Meter data: empty measured_parts, rest values 0, and the model is still written.</summary>
    /// <param name="dataPath">The phases.csv file.</param>
    /// <param name="dataDir">A temporary data folder.</param>
    private static void CheckNoEmi(string dataPath, string dataDir)
    {
        IReadOnlyList<PhaseResult> results = PhaseData.Load(dataPath, true);
        ModelPublication result = ServiceModel.Publish(Report(results, AbortReason.None), dataDir, FixedTime);
        using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(Path.Combine(dataDir, "calibration", "model.json")));
        JsonElement model = doc.RootElement.GetProperty("model");
        Check(result.ExitCode == 0, "no Energy Meter data: model written, code 0");
        Check(doc.RootElement.GetProperty("measured_parts").GetArrayLength() == 0, "no Energy Meter data: measured_parts is empty");
        Check(model.GetProperty("rest_w").GetDouble() == 0, "no Energy Meter data: rest_w is 0");
        Check(model.GetProperty("rest_w_per_cpu_pct").GetDouble() == 0, "no Energy Meter data: rest_w_per_cpu_pct is 0");
    }

    /// <summary>Stopped and aborted runs, and values outside the limits, write nothing and give codes 1 and 3.</summary>
    /// <param name="results">The phase results.</param>
    /// <param name="dataDir">A temporary data folder.</param>
    private static void CheckLimitsAndExitCodes(IReadOnlyList<PhaseResult> results, string dataDir)
    {
        string model = Path.Combine(dataDir, "calibration", "model.json");
        Check(ServiceModel.Publish(Report(results, AbortReason.UserStop), dataDir, FixedTime).ExitCode == 1, "stopped by the user gives code 1");
        Check(ServiceModel.Publish(Report(results, AbortReason.Charger), dataDir, FixedTime).ExitCode == 3, "charger plugged in gives code 3");
        Check(ServiceModel.Publish(Report(results, AbortReason.Failure), dataDir, FixedTime).ExitCode == 3, "an error gives code 3");
        PowerModel wild = new() { BaseWatts = 600, RestWatts = 0, RestWattsPerPercent = 0, CpuWattsPerPercent = 0.1, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2 };
        Check(ModelLimits.Check(wild).Count == 1, "base_w above 500 W is one limit problem");
        PowerModel unknown = new() { BaseWatts = 3, RestWatts = 0, RestWattsPerPercent = 0, CpuWattsPerPercent = null, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2 };
        Check(ModelLimits.Check(unknown).Count == 1, "an unknown value is one limit problem");
        PowerModel steep = new() { BaseWatts = 3, RestWatts = 0, RestWattsPerPercent = 6, CpuWattsPerPercent = 0.1, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2 };
        Check(ModelLimits.Check(steep).Count == 1, "a per-percent value above 5 is one limit problem");
        Check(!File.Exists(model), "stopped and aborted runs wrote no model.json");
    }

    /// <summary>The --data-dir argument replaces the standard folder.</summary>
    private static void CheckCommandLine()
    {
        Check(CommandLine.DataDir(["--data-dir", @"C:\x\y"]) == @"C:\x\y", "--data-dir gives the folder");
        Check(CommandLine.DataDir([]) == ServiceModel.StandardDataDir(), "no argument gives the standard folder");
        Check(CommandLine.DataDir(["--data-dir"]) == ServiceModel.StandardDataDir(), "--data-dir without a value gives the standard folder");
        Check(ServiceModel.StandardDataDir().EndsWith(@"\EcoWattSense", StringComparison.OrdinalIgnoreCase), "the standard folder ends with EcoWattSense");
    }

    /// <summary>Deletes the temporary test folder.</summary>
    /// <param name="folder">The folder.</param>
    private static void RemoveFolder(string folder)
    {
        if (!Directory.Exists(folder)) { return; }
        Directory.Delete(folder, true);
    }
}
