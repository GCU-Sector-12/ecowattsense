// RestModel.cs: works out the "rest" part of the power model for the service (SPEC section 8): the laptop power that the
// Energy Meter parts (processor package, memory) do not explain.
// Who uses it: PowerModel.Build in Analysis.cs, and through it the model file that ServiceModel.cs writes.
// How it works: rest power of a phase = battery power minus every measured part of that phase. A measured part is an
// Energy Meter kind that is known in the phases 3, 5 and 7. When the parts add up to more than the battery power in one of those
// phases they are not believed (AboveBattery). With no measured part the rest is not used (the service then serves level L3 only).
namespace EnergyCalibrator;

/// <summary>Works out the rest power of the phases.</summary>
internal static class RestPower
{
    /// <summary>Name of the processor package part, as the service reads it (SPEC 3.3).</summary>
    public const string PackagePart = "cpu_package";

    /// <summary>Name of the memory part, as the service reads it (SPEC 3.3).</summary>
    public const string DramPart = "dram";

    private const double SumTolerance = 1e-9;

    /// <summary>Lists the Energy Meter kinds that are known in the phases 3, 5 and 7 (the three phases the rest power is worked out from).</summary>
    /// <param name="results">One result per phase.</param>
    /// <returns>The part names in a fixed order (package, then memory); empty when no kind is known in all three phases.</returns>
    public static IReadOnlyList<string> MeasuredParts(IReadOnlyList<PhaseResult> results)
    {
        List<PhaseResult> phases = RestPhases(results);
        List<string> parts = new();
        if (phases.All(r => r.MeanEmiPackageWatts.HasValue)) { parts.Add(PackagePart); }
        if (phases.All(r => r.MeanEmiDramWatts.HasValue)) { parts.Add(DramPart); }
        return parts;
    }

    /// <summary>Tells whether the measured parts add up to more than the battery power in one of the phases 3, 5 or 7.</summary>
    /// <param name="results">One result per phase.</param>
    /// <param name="parts">The measured parts.</param>
    /// <returns>True when the parts are not believable (a model of the processor can read above the real discharge, as on some AMD laptops).</returns>
    public static bool AboveBattery(IReadOnlyList<PhaseResult> results, IReadOnlyList<string> parts)
    {
        return RestPhases(results).Any(r => PartsAboveBattery(r, parts));
    }

    /// <summary>Gives the phases the rest power is worked out from: the reference phase (3), CPU 100% (5) and screen off (7).</summary>
    /// <param name="results">One result per phase.</param>
    /// <returns>The three phases.</returns>
    private static List<PhaseResult> RestPhases(IReadOnlyList<PhaseResult> results)
    {
        return [results[PhaseCatalogue.ScreenReference], results[PhaseCatalogue.CpuFull], results[PhaseCatalogue.ScreenOff]];
    }

    /// <summary>Tells whether the measured parts of one phase are above its battery power.</summary>
    /// <param name="result">The phase.</param>
    /// <param name="parts">The measured parts.</param>
    /// <returns>True when their sum is above the battery power. False when there is no part or no battery power.</returns>
    private static bool PartsAboveBattery(PhaseResult result, IReadOnlyList<string> parts)
    {
        if (parts.Count == 0 || result.BatteryWatts is not double battery) { return false; }
        double sum = PartWatts(result, parts, PackagePart, result.MeanEmiPackageWatts) + PartWatts(result, parts, DramPart, result.MeanEmiDramWatts);
        return sum > battery + SumTolerance;
    }

    /// <summary>Gives the power of one part when it is measured.</summary>
    /// <param name="result">The phase.</param>
    /// <param name="parts">The measured parts.</param>
    /// <param name="part">The part.</param>
    /// <param name="watts">Its power in the phase.</param>
    /// <returns>The power, or 0 when the part is not measured.</returns>
    private static double PartWatts(PhaseResult result, IReadOnlyList<string> parts, string part, double? watts)
    {
        return parts.Contains(part) ? watts.GetValueOrDefault() : 0;
    }

    /// <summary>Works out the rest power of one phase.</summary>
    /// <param name="result">The phase result.</param>
    /// <param name="parts">The measured parts of the whole test.</param>
    /// <returns>Battery power minus the measured parts, or null when the phase is not complete, has no battery power, or lacks a measured part.</returns>
    public static double? Rest(PhaseResult result, IReadOnlyList<string> parts)
    {
        if (result.Status != PhaseStatus.Complete || result.BatteryWatts is not double battery) { return null; }
        double? afterPackage = Take(battery, parts, PackagePart, result.MeanEmiPackageWatts);
        return Take(afterPackage, parts, DramPart, result.MeanEmiDramWatts);
    }

    /// <summary>Works out how much the rest power grows for each percent of CPU use.</summary>
    /// <param name="busy">The CPU 100% phase.</param>
    /// <param name="reference">The reference phase (same brightness, idle).</param>
    /// <param name="span">Difference in mean CPU use between the two phases, in percentage points.</param>
    /// <param name="parts">The measured parts of the whole test.</param>
    /// <returns>Watts per percent, never below 0 (a fall is measurement noise); null when a value is unknown or the span is too small.</returns>
    public static double? PerCpuPercent(PhaseResult busy, PhaseResult reference, double? span, IReadOnlyList<string> parts)
    {
        if (span is not double points || points < TestSettings.MinimumCpuSpanPoints) { return null; }
        double? busyRest = Rest(busy, parts);
        double? referenceRest = Rest(reference, parts);
        if (!busyRest.HasValue || !referenceRest.HasValue) { return null; }
        return Math.Max(0, (busyRest.Value - referenceRest.Value) / points);
    }

    /// <summary>Takes one measured part away from a total, when that part is measured.</summary>
    /// <param name="total">The total so far, or null when it is unknown.</param>
    /// <param name="parts">The measured parts of the whole test.</param>
    /// <param name="part">The part to take away.</param>
    /// <param name="measured">The mean power of that part in this phase, or null.</param>
    /// <returns>The total unchanged when the part is not measured; null when the part is measured but missing in this phase; else the difference.</returns>
    private static double? Take(double? total, IReadOnlyList<string> parts, string part, double? measured)
    {
        if (!parts.Contains(part)) { return total; }
        if (!total.HasValue || !measured.HasValue) { return null; }
        return total.Value - measured.Value;
    }
}
