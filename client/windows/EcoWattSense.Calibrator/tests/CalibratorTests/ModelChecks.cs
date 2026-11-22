// ModelChecks.cs: checks of the model of schema 3: the measured parts and the trust check (spec 2.2), the graphics numbers (2.3),
// the conditions block (2.4), the limits, the file fields and channels.csv.
// Who uses it: Program.Main. The numbers of PhaseBuilder.Normal are small and exact, so every expected value is worked out by hand in the comment.
using System.IO;
using System.Text.Json;
using EnergyCalibrator;
using static CalibratorTests.Harness;

namespace CalibratorTests;

/// <summary>Checks of PowerModel, GraphicsModel, RunConditions and the schema 3 file.</summary>
internal static class ModelChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Group("measured parts", PartsChecks);
        Group("trust check", TrustChecks);
        Group("graphics numbers", GraphicsChecks);
        Group("graphics refused", GraphicsRefusedChecks);
        Group("conditions", ConditionsChecks);
        Group("emi source warning", SourceWarningChecks);
        Group("limits", LimitsChecks);
        Group("schema 3 file", FileChecks);
        Group("channels csv", ChannelsChecks);
    }

    /// <summary>Normal phases: both parts are measured; the rest is battery minus parts (phase 7: 3.5 - 0.5 - 0.2 = 2.8 W).</summary>
    private static void PartsChecks()
    {
        PowerModel m = PowerModel.Build(PhaseBuilder.Normal());
        Check(m.MeasuredParts.SequenceEqual(["cpu_package", "dram"]), "parts: package and memory are measured parts");
        Check(Near(m.RestWatts, 2.8), "parts: rest_w is battery - package - memory in the screen off phase (2.8 W)");
        IReadOnlyList<PhaseResult> noDram = PhaseBuilder.Build(
            new(3.9, 2, 0.8), new(4.9, 2, 0.8), new(4.9, 2, 0.8), new(6.0, 50, 2.0), new(9.0, 100, 5.0), new(7.5, 4, 1.5, null, 60), new(3.5, 1, 0.5));
        Check(PowerModel.Build(noDram).MeasuredParts.SequenceEqual(["cpu_package"]), "parts: a laptop without a memory channel (AMD) has only the package");
        IReadOnlyList<PhaseResult> lateDram = PhaseBuilder.Build(
            new(3.9, 2, 0.8), new(4.9, 2, 0.8), new(4.9, 2, 0.8, 0.3), new(6.0, 50, 2.0), new(9.0, 100, 5.0, 0.5), new(7.5, 4, 1.5, 0.4, 60), new(3.5, 1, 0.5));
        Check(PowerModel.Build(lateDram).MeasuredParts.SequenceEqual(["cpu_package"]), "parts: a part that is not known in phase 7 is not a measured part");
        IReadOnlyList<PhaseResult> noPackage = PhaseBuilder.Build(
            new(3.9, 2), new(4.9, 2), new(4.9, 2, null, 0.3), new(6.0, 50, 2.0, 0.4), new(9.0, 100, 5.0, 0.5), new(7.5, 4, 1.5, 0.4, 60), new(3.5, 1, 0.5, 0.2));
        Check(PowerModel.Build(noPackage).MeasuredParts.SequenceEqual(["dram"]), "parts: a part is measured only when it is known in phases 3, 5 and 7");
        IReadOnlyList<PhaseResult> none = PhaseBuilder.Build(new(3.9, 2), new(4.9, 2), new(4.9, 2), new(6.0, 50), new(9.0, 100), new(7.5, 4, null, null, 60), new(3.5, 1));
        PowerModel plain = PowerModel.Build(none);
        Check(plain.MeasuredParts.Count == 0 && plain.RestWatts == 0 && plain.RestWattsPerPercent == 0, "parts: no part gives empty parts and rest values 0");
        Check(!plain.WarningWords.Contains("measured_above_battery"), "parts: no part gives no trust warning");
    }

    /// <summary>Parts above the battery power in phase 3, 5 or 7 are not believed; equal is believed; the sum of both parts counts.</summary>
    private static void TrustChecks()
    {
        PowerModel above = PowerModel.Build(PhaseBuilder.MeasuredAbove());
        Check(above.MeasuredParts.Count == 0 && above.RestWatts == 0 && above.RestWattsPerPercent == 0 && above.RestWattsPerGpuPercent == 0, "trust: parts above the battery in phases 3, 5 and 7 give empty parts and rest values 0");
        Check(above.WarningWords.Contains("measured_above_battery") && above.Warnings.Contains("measured_above_battery"), "trust: the warning measured_above_battery is given");
        Check(PowerModel.Build(PhaseBuilder.Normal()).WarningWords.All(w => w != "measured_above_battery"), "trust: normal parts give no warning");
        Check(AboveIn(2).MeasuredParts.Count == 0, "trust: above the battery in phase 3 only is enough");
        Check(AboveIn(4).MeasuredParts.Count == 0, "trust: above the battery in phase 5 only is enough");
        Check(AboveIn(6).MeasuredParts.Count == 0, "trust: above the battery in phase 7 only is enough");
        Check(AboveIn(3).WarningWords.All(w => w != "measured_above_battery") && AboveIn(3).MeasuredParts.Count == 2, "trust: above the battery in phase 4 only does not count (only 3, 5 and 7 are checked)");
        IReadOnlyList<PhaseResult> equal = PhaseBuilder.Build(
            new(3.9, 2, 0.8, 0.3), new(4.9, 2, 0.8, 0.3), new(4.9, 2, 4.0, 0.9), new(6.0, 50, 2.0, 0.4), new(9.0, 100, 5.0, 0.5), new(7.5, 4, 1.5, 0.4, 60), new(3.5, 1, 0.5, 0.2));
        Check(PowerModel.Build(equal).MeasuredParts.Count == 2, "trust: parts that add up to exactly the battery power are believed (4.0 + 0.9 = 4.9)");
        IReadOnlyList<PhaseResult> sumAbove = PhaseBuilder.Build(
            new(3.9, 2, 0.8, 0.3), new(4.9, 2, 0.8, 0.3), new(4.9, 2, 4.0, 1.0), new(6.0, 50, 2.0, 0.4), new(9.0, 100, 5.0, 0.5), new(7.5, 4, 1.5, 0.4, 60), new(3.5, 1, 0.5, 0.2));
        Check(PowerModel.Build(sumAbove).MeasuredParts.Count == 0, "trust: each part below the battery but their sum above it is not believed");
        PowerModel stillWritten = PowerModel.Build(PhaseBuilder.MeasuredAbove());
        Check(ModelLimits.Check(stillWritten).Count == 0, "trust: the model of untrusted parts is inside the limits, so it is written (it serves level L3)");
    }

    /// <summary>Gives the model of the normal phases with the package above the battery in one phase (by index).</summary>
    /// <param name="index">Index of the phase.</param>
    /// <returns>The model.</returns>
    private static PowerModel AboveIn(int index)
    {
        List<PhaseResult> list = PhaseBuilder.Normal().ToList();
        PhaseResult old = list[index];
        list[index] = new PhaseResult { Phase = old.Phase, Status = old.Status, SampleCount = old.SampleCount, Seconds = old.Seconds, IoctlWatts = old.IoctlWatts, MeanCpuPercent = old.MeanCpuPercent, MeanEmiPackageWatts = old.BatteryWatts + 1, MeanEmiDramWatts = old.MeanEmiDramWatts, MeanGpuPercent = old.MeanGpuPercent };
        return PowerModel.Build(list);
    }

    /// <summary>
    /// The numbers of PhaseBuilder.Normal. P3 4.9, P5 9.0, P6 7.5; cpu3 2, cpu5 100, cpu6 4; gpu3 0, gpu6 60.
    /// cpu_w_per_pct = (9.0 - 4.9) / (100 - 2) = 0.0418367. gpu_w_per_pct = (7.5 - 4.9 - 0.0418367 x (4 - 2)) / 60 = 0.0419388.
    /// R3 = 4.9 - 0.8 - 0.3 = 3.8; R5 = 9.0 - 5.0 - 0.5 = 3.5; R6 = 7.5 - 1.5 - 0.4 = 5.6. rest_w_per_cpu_pct = max(0, (3.5 - 3.8) / 98) = 0.
    /// rest_w_per_gpu_pct = max(0, (5.6 - 3.8 - 0 x 2) / 60) = 0.03.
    /// </summary>
    private static void GraphicsChecks()
    {
        PowerModel m = PowerModel.Build(PhaseBuilder.Normal());
        Check(Near(m.CpuWattsPerPercent, 4.1 / 98, 1e-9), "graphics: cpu_w_per_pct is as before (0.0418367)");
        Check(Near(m.GpuWattsPerPercent, (2.6 - 4.1 / 98 * 2) / 60, 1e-9), "graphics: gpu_w_per_pct = (P6 - P3 - cpu_w_per_pct x (cpu6 - cpu3)) / (gpu6 - gpu3)");
        Check(Near(m.RestWattsPerGpuPercent, 0.03, 1e-9), "graphics: rest_w_per_gpu_pct = (R6 - R3 - rest_w_per_cpu_pct x (cpu6 - cpu3)) / (gpu6 - gpu3)");
        Check(Near(m.GraphicsWatts, 2.6, 1e-9), "graphics: graphics_w stays P6 - P3");
        Check(m.WarningWords.All(w => !w.StartsWith("graphics")), "graphics: a good span gives no graphics warning");
        IReadOnlyList<PhaseResult> exact = Phases(gpu6: 5);
        PowerModel five = PowerModel.Build(exact);
        Check(five.GpuWattsPerPercent > 0 && five.WarningWords.All(w => w != "graphics_span_too_small"), "graphics: a span of exactly 5 points is accepted");
        IReadOnlyList<PhaseResult> falling = Phases(battery6: 4.0);
        PowerModel low = PowerModel.Build(falling);
        Check(low.GpuWattsPerPercent == 0 && low.RestWattsPerGpuPercent == 0 && low.WarningWords.All(w => w != "graphics_span_too_small"), "graphics: a power that fell is clamped to 0 without a warning");
        IReadOnlyList<PhaseResult> restFalls = Phases(package6: 5.9);
        Check(PowerModel.Build(restFalls).RestWattsPerGpuPercent == 0, "graphics: a negative rest value is clamped to 0");
        PowerModel above = PowerModel.Build(PhaseBuilder.MeasuredAbove());
        Check(above.RestWattsPerGpuPercent == 0 && above.GpuWattsPerPercent > 0, "graphics: untrusted parts give rest_w_per_gpu_pct 0 and the gpu number stays");
    }

    /// <summary>Builds the normal phases with some values of phases 3 and 6 changed.</summary>
    /// <param name="gpu6">Mean 3D use of phase 6.</param>
    /// <param name="battery6">Battery power of phase 6.</param>
    /// <param name="package6">Package power of phase 6.</param>
    /// <param name="gpu6Known">False to make the 3D use of phase 6 unknown.</param>
    /// <param name="stream6">Stream numbers of phase 6.</param>
    /// <param name="stream3">Stream numbers of phase 3.</param>
    /// <returns>The phases.</returns>
    private static IReadOnlyList<PhaseResult> Phases(double gpu6 = 60, double battery6 = 7.5, double package6 = 1.5, bool gpu6Known = true, StreamPhase? stream6 = null, StreamPhase? stream3 = null)
    {
        return PhaseBuilder.Build(
            new(3.9, 2, 0.8, 0.3, 0), new(4.9, 2, 0.8, 0.3, 0), new(4.9, 2, 0.8, 0.3, 0, stream3),
            new(6.0, 50, 2.0, 0.4, 0), new(9.0, 100, 5.0, 0.5, 0), new(battery6, 4, package6, 0.4, gpu6Known ? gpu6 : null, stream6), new(3.5, 1, 0.5, 0.2, 0));
    }

    /// <summary>Gives the stream numbers of a phase with the NVML power and the adapter use.</summary>
    /// <param name="nvmlWatts">NVML power in watts.</param>
    /// <returns>The numbers.</returns>
    private static StreamPhase Nvml(double nvmlWatts) => new([], [], [nvmlWatts], [10, 50], null);

    /// <summary>A span below 5 points, an unknown use, and a discrete card that drew more than 1 W make both numbers 0 with a warning word.</summary>
    private static void GraphicsRefusedChecks()
    {
        PowerModel small = PowerModel.Build(Phases(gpu6: 4.9));
        Check(small.GpuWattsPerPercent == 0 && small.RestWattsPerGpuPercent == 0 && small.WarningWords.Contains("graphics_span_too_small"), "graphics refused: a span below 5 points gives 0, 0 and graphics_span_too_small");
        PowerModel unknown = PowerModel.Build(Phases(gpu6Known: false));
        Check(unknown.GpuWattsPerPercent == 0 && unknown.WarningWords.Contains("graphics_span_too_small"), "graphics refused: an unknown 3D use is treated as a span that is too small");
        PowerModel discrete = PowerModel.Build(Phases(stream3: Nvml(5.0), stream6: Nvml(6.5)));
        Check(discrete.GpuWattsPerPercent == 0 && discrete.RestWattsPerGpuPercent == 0 && discrete.WarningWords.Contains("graphics_phase_used_discrete"), "graphics refused: an NVML device that drew 1.5 W more gives 0, 0 and graphics_phase_used_discrete");
        PowerModel exactly = PowerModel.Build(Phases(stream3: Nvml(5.0), stream6: Nvml(6.0)));
        Check(exactly.GpuWattsPerPercent > 0 && !exactly.WarningWords.Contains("graphics_phase_used_discrete"), "graphics refused: exactly 1 W more is not over 1 W");
        PowerModel idle = PowerModel.Build(Phases(stream3: Nvml(5.0), stream6: Nvml(4.0)));
        Check(idle.GpuWattsPerPercent > 0 && !idle.WarningWords.Contains("graphics_phase_used_discrete"), "graphics refused: an NVML device that drew less is fine");
        PowerModel noNvml = PowerModel.Build(Phases(stream3: new StreamPhase([], [], [null], [], null), stream6: Nvml(9.0)));
        Check(noNvml.GpuWattsPerPercent > 0 && !noNvml.WarningWords.Contains("graphics_phase_used_discrete"), "graphics refused: an unknown NVML power in phase 3 is no proof, so no flag");
        PowerModel both = PowerModel.Build(Phases(gpu6: 3, stream3: Nvml(5.0), stream6: Nvml(9.0)));
        Check(both.WarningWords.Contains("graphics_span_too_small") && both.WarningWords.Contains("graphics_phase_used_discrete"), "graphics refused: both reasons can be given together");
    }

    /// <summary>The conditions come from the header; unknown values are left out; a stream is named as the source.</summary>
    private static void ConditionsChecks()
    {
        RunConditions c = RunConditions.From(StreamData.Header());
        string[] names = ["cpu_vendor", "cpu_family", "cpu_model", "physical_cores", "battery_design_mwh", "battery_full_mwh", "battery_cycle_count", "displays", "external_displays", "refresh_hz", "emi_source"];
        Check(c.Values.Keys.SequenceEqual(names), "conditions: the fields are exactly the eleven of the spec, in order");
        Check((string)c.Values["cpu_vendor"] == "AuthenticAMD" && (int)c.Values["cpu_family"] == 25 && (int)c.Values["cpu_model"] == 80 && (int)c.Values["physical_cores"] == 8, "conditions: the processor fields are copied");
        Check((int)c.Values["battery_design_mwh"] == 50000 && (int)c.Values["battery_full_mwh"] == 45000 && (int)c.Values["battery_cycle_count"] == 120, "conditions: the battery fields are copied");
        Check((int)c.Values["displays"] == 2 && (int)c.Values["external_displays"] == 1 && Near((double)c.Values["refresh_hz"], 60), "conditions: the display fields are copied");
        Check((string)c.Values["emi_source"] == "stream", "conditions: the source is stream");
        Check(c.Warnings.SequenceEqual(["external_display_connected"]), "conditions: an external display gives the warning external_display_connected");
        WatchHeader bare = StreamData.HeaderOf(("RAPL_Package0_PKG", "cpu_package")) with { Display = new WatchDisplay(1, 0, null) };
        RunConditions b = RunConditions.From(bare);
        Check(b.Values.Keys.SequenceEqual(["displays", "external_displays", "emi_source"]) && b.Warnings.Count == 0, "conditions: unknown values are left out and no external display gives no warning");
        RunConditions none = RunConditions.From(null);
        Check(none.Values.Count == 1 && (string)none.Values["emi_source"] == "counter_set" && none.Warnings.Count == 0, "conditions: without the stream only emi_source is written, as counter_set");
        RunConditions unknownExternal = RunConditions.From(bare with { Display = new WatchDisplay(1, null, 60) });
        Check(unknownExternal.Warnings.Count == 0, "conditions: an unknown number of external displays gives no warning");
    }

    /// <summary>Without the stream the warning emi_stream_unavailable is given; with it, not. External displays reach the model warnings.</summary>
    private static void SourceWarningChecks()
    {
        PowerModel counter = PowerModel.Build(PhaseBuilder.Normal());
        Check(counter.WarningWords.Contains("emi_stream_unavailable") && (string)counter.Conditions.Values["emi_source"] == "counter_set", "source: no stream gives emi_stream_unavailable and emi_source counter_set");
        PowerModel stream = PowerModel.Build(PhaseBuilder.Normal(), StreamData.Header());
        Check(!stream.WarningWords.Contains("emi_stream_unavailable") && (string)stream.Conditions.Values["emi_source"] == "stream", "source: a stream gives no emi_stream_unavailable and emi_source stream");
        Check(stream.WarningWords.Contains("external_display_connected") && stream.Warnings.Contains("external_display_connected"), "source: the external display warning is in the model warnings");
    }

    /// <summary>The new numbers are checked against 0 to 5 like the other per-percent values.</summary>
    private static void LimitsChecks()
    {
        PowerModel ok = new() { BaseWatts = 3, RestWatts = 0, RestWattsPerPercent = 0, CpuWattsPerPercent = 0.1, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2 };
        Check(ModelLimits.Check(ok).Count == 0, "limits: the two new numbers default to 0 and are valid");
        PowerModel big = new() { BaseWatts = 3, RestWatts = 0, RestWattsPerPercent = 0, CpuWattsPerPercent = 0.1, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2, GpuWattsPerPercent = 5.5 };
        Check(ModelLimits.Check(big).Count == 1 && ModelLimits.Check(big)[0].StartsWith("gpu_w_per_pct"), "limits: gpu_w_per_pct above 5 is one problem");
        PowerModel negative = new() { BaseWatts = 3, RestWatts = 0, RestWattsPerPercent = 0, CpuWattsPerPercent = 0.1, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2, RestWattsPerGpuPercent = -0.1 };
        Check(ModelLimits.Check(negative).Count == 1 && ModelLimits.Check(negative)[0].StartsWith("rest_w_per_gpu_pct"), "limits: a negative rest_w_per_gpu_pct is one problem");
        PowerModel nan = new() { BaseWatts = 3, RestWatts = 0, RestWattsPerPercent = 0, CpuWattsPerPercent = 0.1, ScreenWattsAt50 = 1, ScreenWattsPerPercent = 0.01, GraphicsWatts = 2, GpuWattsPerPercent = double.NaN };
        Check(ModelLimits.Check(nan).Count == 1, "limits: gpu_w_per_pct that is not a number is one problem");
    }

    /// <summary>The written file has exactly the schema 3 fields.</summary>
    private static void FileChecks()
    {
        WatchHeader header = StreamData.Header();
        StreamPhase s3 = new(["cpu_package"], [1.0], [5.0], [10, 50], 1.0);
        StreamPhase s6 = new(["cpu_package"], [2.0], [5.5], [11, 80], 2.0);
        IReadOnlyList<PhaseResult> results = Phases(stream3: s3, stream6: s6);
        string json = ServiceModel.BuildJson(PhaseBuilder.Report(results, header), PhaseBuilder.FixedTime);
        using JsonDocument doc = JsonDocument.Parse(json);
        JsonElement root = doc.RootElement;
        string[] top = ["schema", "generated_utc", "machine", "measured_parts", "model", "conditions", "phase_power_w", "phase_nvml_w", "phase_gpu_use_pct", "cpu_share_by_phase", "warnings"];
        string[] model = ["base_w", "rest_w", "rest_w_per_cpu_pct", "cpu_w_per_pct", "screen_w_at_50", "screen_w_per_pct", "graphics_w", "gpu_w_per_pct", "rest_w_per_gpu_pct"];
        Check(root.GetProperty("schema").GetInt32() == 3, "file: schema is 3");
        Check(root.EnumerateObject().Select(p => p.Name).SequenceEqual(top), "file: top-level fields are the schema 3 fields");
        Check(root.GetProperty("model").EnumerateObject().Select(p => p.Name).SequenceEqual(model), "file: the model has the seven old numbers and the two graphics numbers");
        Check(Near(root.GetProperty("model").GetProperty("gpu_w_per_pct").GetDouble(), PowerModel.Build(results, header).GpuWattsPerPercent), "file: gpu_w_per_pct is the model value");
        JsonElement conditions = root.GetProperty("conditions");
        Check(conditions.GetProperty("emi_source").GetString() == "stream" && conditions.GetProperty("cpu_vendor").GetString() == "AuthenticAMD" && conditions.GetProperty("battery_full_mwh").GetInt32() == 45000, "file: the conditions block is written");
        Check(root.GetProperty("warnings").EnumerateArray().Any(w => w.GetString() == "external_display_connected"), "file: the warning words are in warnings");
        Check(root.GetProperty("phase_nvml_w").GetProperty(PhaseCatalogue.All[5].Name)[0].GetDouble() == 5.5, "file: phase_nvml_w holds the NVML power of each phase");
        Check(root.GetProperty("phase_gpu_use_pct").GetProperty(PhaseCatalogue.All[5].Name)[1].GetDouble() == 80, "file: phase_gpu_use_pct holds the adapter use of each phase");
        Check(root.GetProperty("phase_nvml_w").GetProperty(PhaseCatalogue.All[0].Name).GetArrayLength() == 0, "file: a phase without stream numbers has an empty list");
        using JsonDocument without = JsonDocument.Parse(ServiceModel.BuildJson(PhaseBuilder.Report(PhaseBuilder.Normal()), PhaseBuilder.FixedTime));
        JsonElement bare = without.RootElement.GetProperty("conditions");
        Check(bare.EnumerateObject().Select(p => p.Name).SequenceEqual(["emi_source"]) && bare.GetProperty("emi_source").GetString() == "counter_set", "file: without the stream the conditions hold only emi_source");
    }

    /// <summary>channels.csv lists every channel of the header with its kind and the power in each phase.</summary>
    private static void ChannelsChecks()
    {
        WatchHeader header = StreamData.HeaderWithGpu(("RAPL_Package0_PKG", "cpu_package"), ("RAPL_Package0_Core0_CORE", "cpu_core"));
        List<PhaseResult> results = new();
        for (int i = 0; i < 7; i++) { results.Add(WithStream(i, new StreamPhase(["cpu_package", "cpu_core"], [i + 1.0, i == 2 ? null : 0.5], [i * 2.0], [i * 10.0, null], null))); }
        string[] lines = ChannelsCsv.Build(header, results).Split('\n', StringSplitOptions.RemoveEmptyEntries).Select(l => l.TrimEnd('\r')).ToArray();
        Check(lines.Length == 1 + 2 + 1 + 2, "channels: one header row, 2 energy channels, 1 NVML device and 2 adapters");
        Check(lines[0] == "source,name,kind,phase_1,phase_2,phase_3,phase_4,phase_5,phase_6,phase_7", "channels: the header row names the seven phases");
        Check(lines[1].StartsWith("emi,RAPL_Package0_PKG,cpu_package,1,2,3,4,5,6,7"), "channels: an energy channel has its kind and its power in each phase");
        Check(lines[2].StartsWith("emi,RAPL_Package0_Core0_CORE,cpu_core,0.5,0.5,,0.5"), "channels: an unknown power is an empty field");
        Check(lines[3].StartsWith("nvml,NVIDIA Test,,0,2,4,6,8,10,12"), "channels: an NVML device has its power in watts");
        Check(lines[4].StartsWith("gpu_use,adapter 0,power_saving,0,10,20") && lines[5].StartsWith("gpu_use,adapter 1,high_performance,,,,"), "channels: an adapter has its role and its use in percent");
        PhaseResult noStream = PhaseBuilder.Build(new PhaseNumbers(1, 1))[0];
        string empty = ChannelsCsv.Build(header, [noStream]);
        Check(empty.Split('\n', StringSplitOptions.RemoveEmptyEntries).Length == 6, "channels: a phase without stream numbers still gives the rows");
    }

    /// <summary>Builds a phase result with stream numbers.</summary>
    /// <param name="index">Index of the phase.</param>
    /// <param name="stream">The stream numbers.</param>
    /// <returns>The result.</returns>
    private static PhaseResult WithStream(int index, StreamPhase stream)
    {
        return new PhaseResult { Phase = PhaseCatalogue.All[index], Status = PhaseStatus.Complete, Stream = stream };
    }
}
