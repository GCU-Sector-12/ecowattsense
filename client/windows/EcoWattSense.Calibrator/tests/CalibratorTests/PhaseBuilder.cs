// PhaseBuilder.cs: builds phase results and reports from plain numbers, so a check can say "phase 3 drew 7 W on the battery and 8 W in the package".
// Who uses it: the *Checks.cs files of this folder. Phase numbers here are the numbers of the test (1 to 7), not indexes.
using EnergyCalibrator;

namespace CalibratorTests;

/// <summary>The numbers of one phase.</summary>
/// <param name="Battery">Battery power in watts.</param>
/// <param name="Cpu">Mean processor use in percent.</param>
/// <param name="Package">Mean package power in watts, or null.</param>
/// <param name="Dram">Mean memory power in watts, or null.</param>
/// <param name="Gpu">Mean 3D use in percent, or null.</param>
/// <param name="Stream">The stream numbers of the phase, or null.</param>
internal sealed record PhaseNumbers(double? Battery, double? Cpu, double? Package = null, double? Dram = null, double? Gpu = null, StreamPhase? Stream = null);

/// <summary>Builds phase results from numbers.</summary>
internal static class PhaseBuilder
{
    /// <summary>The time the checks write files with.</summary>
    public static readonly DateTime FixedTime = new(2026, 11, 22, 10, 30, 5, DateTimeKind.Utc);

    /// <summary>Builds seven complete phases.</summary>
    /// <param name="phases">The numbers of phases 1 to 7.</param>
    /// <returns>The results in phase order.</returns>
    public static IReadOnlyList<PhaseResult> Build(params PhaseNumbers[] phases)
    {
        return phases.Select((numbers, index) => One(index, numbers)).ToList();
    }

    /// <summary>Builds one complete phase.</summary>
    /// <param name="index">Index of the phase.</param>
    /// <param name="n">Its numbers.</param>
    /// <returns>The result.</returns>
    private static PhaseResult One(int index, PhaseNumbers n)
    {
        return new PhaseResult
        {
            Phase = PhaseCatalogue.All[index],
            Status = PhaseStatus.Complete,
            SampleCount = 30,
            Seconds = 120,
            IoctlWatts = n.Battery,
            MeanCpuPercent = n.Cpu,
            MeanEmiPackageWatts = n.Package,
            MeanEmiDramWatts = n.Dram,
            MeanGpuPercent = n.Gpu,
            Stream = n.Stream
        };
    }

    /// <summary>Gives a laptop whose parts are below the battery power in every phase (the normal case, close to the 12 Nov run).</summary>
    /// <returns>Phases 1 to 7.</returns>
    public static IReadOnlyList<PhaseResult> Normal() => Build(
        new(3.9, 2, 0.8, 0.3, 0), new(4.9, 2, 0.8, 0.3, 0), new(4.9 - 0.0, 2, 0.8, 0.3, 0),
        new(6.0, 50, 2.0, 0.4, 0), new(9.0, 100, 5.0, 0.5, 0), new(7.5, 4, 1.5, 0.4, 60), new(3.5, 1, 0.5, 0.2, 0));

    /// <summary>Gives a laptop whose package power is above the battery power in the phases 3, 5 and 7 (seen on AMD forums).</summary>
    /// <returns>Phases 1 to 7.</returns>
    public static IReadOnlyList<PhaseResult> MeasuredAbove() => Build(
        new(3.9, 2, 3.0, 0.3, 0), new(4.9, 2, 3.0, 0.3, 0), new(5.0, 2, 6.0, 0.3, 0),
        new(6.0, 50, 7.0, 0.4, 0), new(9.0, 100, 11.0, 0.5, 0), new(7.5, 4, 8.0, 0.4, 60), new(3.5, 1, 4.0, 0.2, 0));

    /// <summary>Builds a report of a finished run for the model file.</summary>
    /// <param name="results">The phase results.</param>
    /// <param name="stream">The stream header of the run, or null.</param>
    /// <returns>The report.</returns>
    public static RunReport Report(IReadOnlyList<PhaseResult> results, WatchHeader? stream = null)
    {
        MachineInfo machine = new("Test Maker", "Test Model", "Test CPU", 8, "10.0.0.0");
        TestOutcome outcome = new(FixedTime, new List<Sample>(), results.Select(r => r.Status).ToList(), Enumerable.Repeat("", results.Count).ToList(), AbortReason.None, "test", new List<string>(), stream);
        return new RunReport(machine, outcome, results, PowerModel.Build(results, stream), new PowerRunRecord());
    }
}
