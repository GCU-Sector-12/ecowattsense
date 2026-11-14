// RestModel.cs: works out the "rest" part of the power model for the service (SPEC section 8): the laptop power that the
// Energy Meter parts (processor package, memory) do not explain.
// Who uses it: PowerModel.Build in Analysis.cs, and through it the model file that ServiceModel.cs writes.
// How it works: rest power of a phase = battery power minus every measured part of that phase. A measured part is an
// Energy Meter kind that gave a value in the test. With no measured part the rest is not used (the service then serves level L3 only).
namespace EnergyCalibrator;

/// <summary>Works out the rest power of the phases.</summary>
internal static class RestPower
{
    /// <summary>Name of the processor package part, as the service reads it (SPEC 3.3).</summary>
    public const string PackagePart = "cpu_package";

    /// <summary>Name of the memory part, as the service reads it (SPEC 3.3).</summary>
    public const string DramPart = "dram";

    /// <summary>Lists the Energy Meter kinds that gave a value in at least one phase.</summary>
    /// <param name="results">One result per phase.</param>
    /// <returns>The part names in a fixed order (package, then memory); empty when the Energy Meter gave nothing.</returns>
    public static IReadOnlyList<string> MeasuredParts(IReadOnlyList<PhaseResult> results)
    {
        List<string> parts = new();
        if (results.Any(r => r.MeanEmiPackageWatts.HasValue)) { parts.Add(PackagePart); }
        if (results.Any(r => r.MeanEmiDramWatts.HasValue)) { parts.Add(DramPart); }
        return parts;
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
