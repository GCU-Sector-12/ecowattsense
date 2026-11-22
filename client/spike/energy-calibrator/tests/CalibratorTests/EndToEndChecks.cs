// EndToEndChecks.cs: runs the whole chain on made-up samples: stream lines and battery counters -> phase analysis -> model -> files.
// Who uses it: Program.Main. The laptop is an AMD-like one: the package channel reads above the battery discharge in the phases 3, 5 and 7
// (seen on forums), so the parts are not believed, the model is written and the files carry the warning words.
using System.IO;
using System.Text.Json;
using EnergyCalibrator;
using static CalibratorTests.Harness;

namespace CalibratorTests;

/// <summary>Checks of the chain from samples to files.</summary>
internal static class EndToEndChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Group("end to end above", AboveBattery);
        Group("end to end normal", NormalLaptop);
        Group("end to end files", FilesChecks);
    }

    /// <summary>Battery power, processor use, 3D use and package power of the seven phases of an AMD-like laptop.</summary>
    private static readonly (double Battery, double Cpu, double Gpu, double Package)[] Above =
    [
        (3.9, 2, 0, 3.0), (4.9, 2, 0, 3.0), (5.0, 2, 0, 6.0), (6.0, 50, 0, 7.0), (9.0, 100, 0, 11.0), (7.5, 4, 60, 8.0), (3.5, 1, 0, 4.0)
    ];

    /// <summary>Battery power, processor use, 3D use and package power of a laptop whose package is below the battery power.</summary>
    private static readonly (double Battery, double Cpu, double Gpu, double Package)[] Normal =
    [
        (3.9, 2, 0, 0.8), (4.9, 2, 0, 0.8), (4.9, 2, 0, 0.8), (6.0, 50, 0, 2.0), (9.0, 100, 0, 5.0), (7.5, 4, 60, 1.5), (3.5, 1, 0, 0.5)
    ];

    /// <summary>Builds the outcome of a whole run.</summary>
    /// <param name="phases">The numbers of the seven phases.</param>
    /// <param name="header">The stream header.</param>
    /// <returns>The outcome with the samples of all phases.</returns>
    private static TestOutcome Outcome((double Battery, double Cpu, double Gpu, double Package)[] phases, WatchHeader header)
    {
        List<Sample> samples = new();
        for (int p = 0; p < phases.Length; p++) { samples.AddRange(PhaseSamples(p + 1, phases[p])); }
        PhaseStatus[] statuses = Enumerable.Repeat(PhaseStatus.Complete, PhaseCatalogue.All.Count).ToArray();
        string[] notes = Enumerable.Repeat("", PhaseCatalogue.All.Count).ToArray();
        return new TestOutcome(PhaseBuilder.FixedTime, samples, statuses, notes, AbortReason.None, "The test ran to the end.", [], header, 0);
    }

    /// <summary>Builds the ten used samples of one phase.</summary>
    /// <param name="number">Number of the phase.</param>
    /// <param name="n">Its numbers.</param>
    /// <returns>The samples.</returns>
    private static List<Sample> PhaseSamples(int number, (double Battery, double Cpu, double Gpu, double Package) n)
    {
        double dropMwh = n.Battery * 18 / 3.6;
        List<Sample> samples = StreamData.Samples(number, 10, i => Line(i, n.Package));
        for (int i = 0; i < samples.Count; i++) { Fill(samples[i], i, n, dropMwh); }
        return samples;
    }

    /// <summary>Builds the stream line of sample i: package n watts, a core 1 W, NVML 0.</summary>
    /// <param name="i">Position of the sample.</param>
    /// <param name="packageWatts">The package power.</param>
    /// <returns>The line.</returns>
    private static WatchSample Line(int i, double packageWatts)
    {
        long step = (long)(packageWatts * 2_000_000);
        return StreamData.Line(2000L * i, [step * i, 2_000_000L * i], [0.0], [0.0, 0.0]);
    }

    /// <summary>Adds the battery, processor and 3D values to a sample.</summary>
    /// <param name="sample">The sample.</param>
    /// <param name="i">Position of the sample.</param>
    /// <param name="n">The numbers of the phase.</param>
    /// <param name="dropMwh">The battery charge that falls over the ten samples.</param>
    private static void Fill(Sample sample, int i, (double Battery, double Cpu, double Gpu, double Package) n, double dropMwh)
    {
        sample.Set(SampleKeys.BatteryMwh, 30000 - dropMwh * i / 9.0);
        sample.Set(SampleKeys.BatteryState, 2);
        sample.Set(SampleKeys.CpuUtil, n.Cpu);
        sample.Set(SampleKeys.Gpu3D, n.Gpu);
        sample.Set(SampleKeys.Brightness, sample.PhaseNumber == 7 ? 0 : PhaseCatalogue.All[sample.PhaseNumber - 1].BrightnessPercent ?? 0);
    }

    /// <summary>The header of the AMD-like laptop: a package channel, a core channel, a discrete card and two adapters.</summary>
    /// <returns>The header.</returns>
    private static WatchHeader AmdHeader()
    {
        return StreamData.HeaderWithGpu(("RAPL_Package0_PKG", "cpu_package"), ("RAPL_Package0_Core0_CORE", "cpu_core"));
    }

    /// <summary>Package power above the battery discharge: no measured parts, the model is written, the words are in the file.</summary>
    private static void AboveBattery()
    {
        TestOutcome outcome = Outcome(Above, AmdHeader());
        IReadOnlyList<PhaseResult> results = PhaseAnalyser.AnalyseAll(outcome);
        PowerModel model = PowerModel.Build(results, outcome.Stream);
        Check(Near(results[2].BatteryWatts, 5.0, 1e-6) && Near(results[2].MeanEmiPackageWatts, 6.0, 1e-6), "end to end above: battery 5.0 W and package 6.0 W are worked out from the samples");
        Check(model.MeasuredParts.Count == 0 && model.RestWatts == 0, "end to end above: parts are not believed");
        string temp = Path.Combine(Path.GetTempPath(), "ews-cal-e2e-" + Guid.NewGuid().ToString("N"));
        ModelPublication published = ServiceModel.Publish(new RunReport(new MachineInfo("M", "M", "C", 8, "10"), outcome, results, model, new PowerRunRecord()), temp, PhaseBuilder.FixedTime);
        Check(published.ExitCode == 0, "end to end above: the model is written (code 0)");
        string file = Path.Combine(temp, "calibration", "model.json");
        using JsonDocument doc = JsonDocument.Parse(File.ReadAllText(file));
        bool word = doc.RootElement.GetProperty("warnings").EnumerateArray().Any(w => w.GetString() == "measured_above_battery");
        Check(word && doc.RootElement.GetProperty("measured_parts").GetArrayLength() == 0, "end to end above: the file has no parts and the warning measured_above_battery");
        Directory.Delete(temp, true);
    }

    /// <summary>A normal laptop: the parts are believed, the graphics numbers are written.</summary>
    private static void NormalLaptop()
    {
        TestOutcome outcome = Outcome(Normal, AmdHeader());
        IReadOnlyList<PhaseResult> results = PhaseAnalyser.AnalyseAll(outcome);
        PowerModel model = PowerModel.Build(results, outcome.Stream);
        Check(model.MeasuredParts.SequenceEqual(["cpu_package"]), "end to end normal: the package is a measured part (no memory channel)");
        Check(Near(model.RestWatts, 3.5 - 0.5, 1e-6), "end to end normal: rest_w is battery - package in the screen off phase");
        Check(model.GpuWattsPerPercent > 0 && model.WarningWords.All(w => w != "measured_above_battery" && !w.StartsWith("graphics")), "end to end normal: the graphics number is set and no trust warning is given");
        Check(ModelLimits.Check(model).Count == 0, "end to end normal: the model is inside the limits");
    }

    /// <summary>The result folder gets channels.csv and the summary says where the energy values came from; without a stream there is no channels.csv.</summary>
    private static void FilesChecks()
    {
        string temp = Path.Combine(Path.GetTempPath(), "ews-cal-files-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        TestOutcome outcome = Outcome(Normal, AmdHeader());
        IReadOnlyList<PhaseResult> results = PhaseAnalyser.AnalyseAll(outcome);
        RunReport report = new(new MachineInfo("M", "M", "C", 8, "10"), outcome, results, PowerModel.Build(results, outcome.Stream), new PowerRunRecord());
        ResultWriter.Save(temp, report, true);
        string summary = File.ReadAllText(Path.Combine(temp, "summary.txt"));
        Check(File.Exists(Path.Combine(temp, "channels.csv")) && File.ReadAllLines(Path.Combine(temp, "channels.csv"))[1].StartsWith("emi,RAPL_Package0_PKG,cpu_package,0.8"), "files: channels.csv is written with the package power of phase 1");
        Check(summary.Contains("energy stream of ews-scan.exe") && summary.Contains("0 bad lines skipped"), "files: summary.txt names the energy stream and the bad line count");
        using JsonDocument model = JsonDocument.Parse(File.ReadAllText(Path.Combine(temp, "model.json")));
        Check(model.RootElement.GetProperty("conditions").GetProperty("emi_source").GetString() == "stream" && model.RootElement.GetProperty("model").TryGetProperty("gpu_w_per_pct", out _), "files: the result model.json has the conditions and the graphics number");
        string other = Path.Combine(temp, "plain");
        Directory.CreateDirectory(other);
        IReadOnlyList<PhaseResult> plain = PhaseBuilder.Normal();
        ResultWriter.Save(other, PhaseBuilder.Report(plain), true);
        Check(!File.Exists(Path.Combine(other, "channels.csv")) && File.ReadAllText(Path.Combine(other, "summary.txt")).Contains("counter set"), "files: without the stream there is no channels.csv and the summary names the counter set");
        Directory.Delete(temp, true);
    }
}
