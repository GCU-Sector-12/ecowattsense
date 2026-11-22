// MeasuredChecks.cs: checks of the measured parts and of the trust check (spec 2.2): when the measured parts are above the battery power
// in the phases 3, 5 or 7, they are not believed, the model is still written, and the result carries the warning measured_above_battery.
// Who uses it: Program.Main.
using System.IO;
using System.Text.Json;
using EnergyCalibrator;
using static CalibratorTests.Harness;

namespace CalibratorTests;

/// <summary>Checks of the measured parts and the trust check.</summary>
internal static class MeasuredChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Group("measured above battery", MeasuredAboveBattery);
    }

    /// <summary>Package power above the battery power in the phases 3, 5 and 7.</summary>
    private static void MeasuredAboveBattery()
    {
        IReadOnlyList<PhaseResult> results = PhaseBuilder.MeasuredAbove();
        PowerModel model = PowerModel.Build(results);
        string temp = Path.Combine(Path.GetTempPath(), "ews-cal-m-" + Guid.NewGuid().ToString("N"));
        MachineInfo machine = new("Test Maker", "Test Model", "Test CPU", 8, "10.0.0.0");
        TestOutcome outcome = new(PhaseBuilder.FixedTime, new List<Sample>(), results.Select(r => r.Status).ToList(), Enumerable.Repeat("", results.Count).ToList(), AbortReason.None, "test", new List<string>());
        ModelPublication published = ServiceModel.Publish(new RunReport(machine, outcome, results, model, new PowerRunRecord()), temp, PhaseBuilder.FixedTime);
        string file = Path.Combine(temp, "calibration", "model.json");
        Info($"measured above battery: exit {published.ExitCode}, file written {File.Exists(file)}, rest_w {model.RestWatts}, parts [{string.Join(",", model.MeasuredParts)}], message: {published.Message}");
        Check(published.ExitCode == 0 && File.Exists(file), "measured above battery: the model is still written (code 0)");
        Check(model.MeasuredParts.Count == 0, "measured above battery: measured_parts is empty");
        Check(model.RestWatts == 0, "measured above battery: rest_w is 0");
        Check(model.RestWattsPerPercent == 0, "measured above battery: rest_w_per_cpu_pct is 0");
        Check(model.Warnings.Contains("measured_above_battery"), "measured above battery: the warning measured_above_battery is given");
        if (File.Exists(file)) { CheckFileWarning(file); }
        if (Directory.Exists(temp)) { Directory.Delete(temp, true); }
    }

    /// <summary>The written file lists the warning word.</summary>
    /// <param name="file">The model file.</param>
    private static void CheckFileWarning(string file)
    {
        using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
        bool listed = doc.RootElement.GetProperty("warnings").EnumerateArray().Any(w => w.GetString() == "measured_above_battery");
        Check(listed, "measured above battery: the file lists the warning word");
    }
}
