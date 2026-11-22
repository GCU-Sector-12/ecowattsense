// StreamPhaseChecks.cs: checks of the phase numbers from the stream (spec 2.1): the power of a channel from the first and the last used line,
// the unknown rules (a null at either end or in between, a missing line, no time between), the sum of a kind, and the link to PhaseAnalyser.
// Who uses it: Program.Main.
using EnergyCalibrator;
using static CalibratorTests.Harness;

namespace CalibratorTests;

/// <summary>Checks of StreamPhase and of PhaseAnalyser with a stream.</summary>
internal static class StreamPhaseChecks
{
    private static readonly WatchHeader ThreeChannels = StreamData.HeaderWithGpu(("RAPL_Package0_PKG", "cpu_package"), ("RAPL_Package0_Core0_CORE", "cpu_core"), ("RAPL_Package0_DRAM", "dram"));

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Group("stream phase power", PowerChecks);
        Group("stream phase unknown", UnknownChecks);
        Group("stream phase kinds", KindChecks);
        Group("stream phase gpu", GpuChecks);
        Group("stream analyser", AnalyserChecks);
    }

    /// <summary>Builds the line of sample i: package 5 W, core 2 W, memory 0.5 W, NVML 1000 x (i + 1) mW, use 10 and 20 percent.</summary>
    /// <param name="i">Position of the sample; its time is 2000 x i ms.</param>
    /// <returns>The line.</returns>
    private static WatchSample Steady(int i)
    {
        long uj = 1_000_000L * i;
        return StreamData.Line(2000L * i, [10 * uj, 4 * uj, uj], [1000.0 * (i + 1)], [10, 20]);
    }

    /// <summary>The power is the energy difference over the time between the first and the last used line.</summary>
    private static void PowerChecks()
    {
        List<Sample> used = StreamData.Samples(3, 10, Steady);
        StreamPhase p = StreamPhase.Measure(used, ThreeChannels);
        Check(Near(p.ChannelWatts[0], 5.0), "stream phase: package power is 5 W");
        Check(Near(p.ChannelWatts[1], 2.0) && Near(p.ChannelWatts[2], 0.5), "stream phase: core 2 W and memory 0.5 W");
        Check(Near(p.PackageMwh, 25.0), "stream phase: package energy is 25 mWh (90 J)");
        Check(p.ChannelKinds.SequenceEqual(["cpu_package", "cpu_core", "dram"]), "stream phase: the kinds of the channels are kept in order");
    }

    /// <summary>A null at either end or in between, a missing line, no time between and too few samples make a channel unknown.</summary>
    private static void UnknownChecks()
    {
        StreamPhase nullAtEnd = StreamPhase.Measure(StreamData.Samples(3, 10, i => i == 9 ? StreamData.Line(2000L * i, [null, 4_000_000L * i, 1_000_000L * i]) : Steady(i)), ThreeChannels);
        Check(nullAtEnd.ChannelWatts[0] == null && nullAtEnd.ChannelWatts[2] != null, "stream phase unknown: a null at the last line makes only that channel unknown");
        StreamPhase nullAtStart = StreamPhase.Measure(StreamData.Samples(3, 10, i => i == 0 ? StreamData.Line(0, [null, 0, 0]) : Steady(i)), ThreeChannels);
        Check(nullAtStart.ChannelWatts[0] == null, "stream phase unknown: a null at the first line makes the channel unknown");
        StreamPhase nullBetween = StreamPhase.Measure(StreamData.Samples(3, 10, i => i == 5 ? StreamData.Line(2000L * i, [10_000_000L * i, 4_000_000L * i, null]) : Steady(i)), ThreeChannels);
        Check(nullBetween.ChannelWatts[2] == null && nullBetween.ChannelWatts[0] != null, "stream phase unknown: a null in between makes the channel unknown");
        StreamPhase noLine = StreamPhase.Measure(StreamData.Samples(3, 10, i => i == 5 ? null : Steady(i)), ThreeChannels);
        Check(noLine.ChannelWatts.All(w => w == null), "stream phase unknown: a sample without a line makes every energy channel unknown");
        Check(Near(noLine.NvmlWatts[0], MeanNvmlWithout(5)), "stream phase unknown: the NVML mean uses the lines that exist");
        StreamPhase sameLine = StreamPhase.Measure(StreamData.Samples(3, 10, _ => Steady(4)), ThreeChannels);
        Check(sameLine.ChannelWatts.All(w => w == null), "stream phase unknown: the same line in every sample means no time between, so unknown");
        StreamPhase one = StreamPhase.Measure(StreamData.Samples(3, 1, Steady), ThreeChannels);
        Check(one.ChannelWatts.All(w => w == null) && one.PackageMwh == null, "stream phase unknown: one sample is too few");
        StreamPhase none = StreamPhase.Measure([], ThreeChannels);
        Check(none.ChannelWatts.Count == 3 && none.ChannelWatts.All(w => w == null), "stream phase unknown: no sample gives unknown values with the right length");
        StreamPhase backwards = StreamPhase.Measure(StreamData.Samples(3, 4, i => StreamData.Line(2000L * i, [1000 - 100L * i, 0, 0])), ThreeChannels);
        Check(backwards.ChannelWatts[0] == null, "stream phase unknown: an energy that fell between the first and the last line is unknown");
    }

    /// <summary>The mean NVML power in watts when the line of one sample is missing.</summary>
    /// <param name="missing">The position of the missing sample.</param>
    /// <returns>The mean in watts.</returns>
    private static double MeanNvmlWithout(int missing)
    {
        return Enumerable.Range(0, 10).Where(i => i != missing).Average(i => (i + 1) * 1.0);
    }

    /// <summary>A kind is the sum of its channels; it is unknown when one channel is unknown; cpu_core is a kind of its own.</summary>
    private static void KindChecks()
    {
        WatchHeader two = StreamData.HeaderOf(("RAPL_Package0_PKG", "cpu_package"), ("RAPL_Package1_PKG", "cpu_package"), ("RAPL_Package0_Core0_CORE", "cpu_core"), ("X", "other"));
        StreamPhase p = StreamPhase.Measure(StreamData.Samples(3, 10, i => StreamData.Line(2000L * i, [10_000_000L * i, 5_000_000L * i, 3_000_000L * i, 1_000_000L * i])), two);
        Check(Near(p.KindWatts("cpu_package"), 7.5), "stream kinds: two package channels are added (5 W + 2.5 W)");
        Check(Near(p.KindWatts("cpu_core"), 1.5), "stream kinds: cpu_core has its own power and does not enter the package");
        Check(p.KindWatts("dram") == null, "stream kinds: a kind that the header does not have is unknown");
        StreamPhase oneNull = StreamPhase.Measure(StreamData.Samples(3, 10, i => StreamData.Line(2000L * i, [10_000_000L * i, i == 3 ? null : 5_000_000L * i, 0, 0])), two);
        Check(oneNull.KindWatts("cpu_package") == null, "stream kinds: the sum is unknown when one of its channels is unknown");
    }

    /// <summary>NVML power and adapter use are means; all null gives unknown.</summary>
    private static void GpuChecks()
    {
        StreamPhase p = StreamPhase.Measure(StreamData.Samples(6, 10, Steady), ThreeChannels);
        Check(Near(p.NvmlWatts[0], 5.5), "stream gpu: the NVML mean power is 5.5 W");
        Check(Near(p.GpuUsePercent[0], 10) && Near(p.GpuUsePercent[1], 20), "stream gpu: the use of each adapter is its mean");
        StreamPhase unknown = StreamPhase.Measure(StreamData.Samples(6, 10, i => StreamData.Line(2000L * i, [0, 0, 0], [null], [null, 5])), ThreeChannels);
        Check(unknown.NvmlWatts[0] == null && unknown.GpuUsePercent[0] == null && Near(unknown.GpuUsePercent[1], 5), "stream gpu: all null gives unknown, others are not touched");
    }

    /// <summary>PhaseAnalyser uses the stream for the Energy Meter values and warns when it had no data; without a stream it reads the counter keys as before.</summary>
    private static void AnalyserChecks()
    {
        List<Sample> samples = StreamData.Samples(3, 10, Steady);
        TestOutcome outcome = Outcome(2, samples, ThreeChannels);
        PhaseResult r = PhaseAnalyser.Analyse(2, outcome);
        Check(Near(r.MeanEmiPackageWatts, 5.0) && Near(r.MeanEmiDramWatts, 0.5), "analyser: package and memory power come from the stream kinds");
        Check(r.MeanEmiCoresWatts == null && r.MeanEmiGpuWatts == null, "analyser: no cores (PP0) and no integrated graphics (PP1) channel gives unknown");
        Check(Near(r.EmiPackageMwh, 25.0), "analyser: package energy comes from the stream");
        Check(r.Stream != null && r.CpuShareSource == "Windows Energy Meter", "analyser: the stream numbers are kept and the CPU share names its source");
        PhaseResult gap = PhaseAnalyser.Analyse(2, Outcome(2, StreamData.Samples(3, 10, i => i == 4 ? null : Steady(i)), ThreeChannels));
        Check(gap.MeanEmiPackageWatts == null && gap.Warning.Contains("energy stream"), "analyser: a gap in the stream gives unknown power and a phase warning");
        List<Sample> counters = StreamData.Samples(3, 4, _ => null);
        foreach (Sample s in counters) { s.Set(SampleKeys.EmiPackageWatts, 4.0); }
        PhaseResult noStream = PhaseAnalyser.Analyse(2, Outcome(2, counters, null));
        Check(Near(noStream.MeanEmiPackageWatts, 4.0) && noStream.Stream == null && !noStream.Warning.Contains("energy stream"), "analyser: without a stream the counter values are used as before");
    }

    /// <summary>Builds the outcome of a run in which phase index has the given samples.</summary>
    /// <param name="index">Index of the phase.</param>
    /// <param name="samples">The samples.</param>
    /// <param name="header">The stream header, or null.</param>
    /// <returns>The outcome.</returns>
    private static TestOutcome Outcome(int index, List<Sample> samples, WatchHeader? header)
    {
        PhaseStatus[] statuses = Enumerable.Repeat(PhaseStatus.NotRun, PhaseCatalogue.All.Count).ToArray();
        statuses[index] = PhaseStatus.Complete;
        string[] notes = Enumerable.Repeat("", PhaseCatalogue.All.Count).ToArray();
        return new TestOutcome(PhaseBuilder.FixedTime, samples, statuses, notes, AbortReason.None, "test", [], header);
    }
}
