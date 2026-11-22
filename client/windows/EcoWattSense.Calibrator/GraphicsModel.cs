// GraphicsModel.cs: works out the two graphics numbers of model schema 3 (spec 2.3): watts for each percent of 3D use, and the part of
// them that the measured parts do not explain.
// Who uses it: PowerModel.Build. Phase 3 is "Rest, brightness 50%", phase 6 is "Graphics".
namespace EnergyCalibrator;

/// <summary>The graphics numbers of the model and the warning words that belong to them.</summary>
/// <param name="GpuWattsPerPercent">Watts of the whole laptop for each percent of 3D use, after the processor part is taken away.</param>
/// <param name="RestWattsPerGpuPercent">The same for the rest power (battery power minus the measured parts).</param>
/// <param name="Warnings">Warning words, empty when there is none.</param>
internal sealed record GraphicsTerms(double GpuWattsPerPercent, double RestWattsPerGpuPercent, IReadOnlyList<string> Warnings);

/// <summary>Works out the graphics numbers.</summary>
internal static class GraphicsModel
{
    /// <summary>Warning word: the 3D use of the Graphics phase is too close to the reference phase to divide by.</summary>
    public const string SpanTooSmall = "graphics_span_too_small";

    /// <summary>Warning word: the Graphics phase loaded a discrete card, whose power is not part of this model.</summary>
    public const string UsedDiscrete = "graphics_phase_used_discrete";

    /// <summary>Works out both numbers.</summary>
    /// <param name="results">One result per phase.</param>
    /// <param name="cpuWattsPerPercent">The model value cpu_w_per_pct, or null when unknown.</param>
    /// <param name="restWattsPerCpuPercent">The model value rest_w_per_cpu_pct.</param>
    /// <param name="parts">The measured parts that are believed; empty when none.</param>
    /// <returns>The numbers; both 0 with a warning word when they cannot be trusted, and both 0 without a word when a phase value they need is unknown (the missing-value lines of the model say that).</returns>
    public static GraphicsTerms Compute(IReadOnlyList<PhaseResult> results, double? cpuWattsPerPercent, double restWattsPerCpuPercent, IReadOnlyList<string> parts)
    {
        PhaseResult reference = results[PhaseCatalogue.ScreenReference];
        PhaseResult graphics = results[PhaseCatalogue.Graphics];
        bool complete = reference.Status == PhaseStatus.Complete && graphics.Status == PhaseStatus.Complete;
        if (!complete) { return Zero([]); }
        List<string> words = Refusals(reference, graphics);
        double? span = GpuSpan(reference, graphics);
        double? cpuSpan = Difference(graphics.MeanCpuPercent, reference.MeanCpuPercent);
        double? battery = Difference(graphics.BatteryWatts, reference.BatteryWatts);
        bool known = span.HasValue && cpuSpan.HasValue && battery.HasValue && cpuWattsPerPercent.HasValue;
        if (words.Count > 0 || !known) { return Zero(words); }
        double gpu = Math.Max(0, (battery!.Value - cpuWattsPerPercent!.Value * cpuSpan!.Value) / span!.Value);
        return new GraphicsTerms(gpu, RestPerGpu(reference, graphics, restWattsPerCpuPercent, parts, cpuSpan.Value, span.Value), words);
    }

    /// <summary>Gives the warning words for a Graphics phase that cannot be trusted.</summary>
    /// <param name="reference">The reference phase (3).</param>
    /// <param name="graphics">The Graphics phase (6).</param>
    /// <returns>The words; empty when the phase can be trusted.</returns>
    private static List<string> Refusals(PhaseResult reference, PhaseResult graphics)
    {
        List<string> words = new();
        double? span = GpuSpan(reference, graphics);
        if (!(span >= TestSettings.MinimumGpuSpanPoints)) { words.Add(SpanTooSmall); }
        if (DiscreteCardRose(reference, graphics)) { words.Add(UsedDiscrete); }
        return words;
    }

    /// <summary>Works out the rise of the 3D use between the reference phase and the Graphics phase.</summary>
    /// <param name="reference">The reference phase (3).</param>
    /// <param name="graphics">The Graphics phase (6).</param>
    /// <returns>The rise in points; null when one of the means is unknown.</returns>
    private static double? GpuSpan(PhaseResult reference, PhaseResult graphics) => Difference(graphics.MeanGpuPercent, reference.MeanGpuPercent);

    /// <summary>Works out the difference of two values that may be unknown.</summary>
    /// <param name="a">The first value.</param>
    /// <param name="b">The value to take away.</param>
    /// <returns>a minus b, or null when one is unknown.</returns>
    private static double? Difference(double? a, double? b) => a.HasValue && b.HasValue ? a.Value - b.Value : null;

    /// <summary>Tells whether an NVML device drew more than the limit more in the Graphics phase than in the reference phase.</summary>
    /// <param name="reference">The reference phase (3).</param>
    /// <param name="graphics">The Graphics phase (6).</param>
    /// <returns>True when one device did. An unknown power proves nothing.</returns>
    private static bool DiscreteCardRose(PhaseResult reference, PhaseResult graphics)
    {
        int devices = Math.Min(reference.Stream?.NvmlWatts.Count ?? 0, graphics.Stream?.NvmlWatts.Count ?? 0);
        return Enumerable.Range(0, devices).Any(device => Difference(graphics.Stream!.NvmlWatts[device], reference.Stream!.NvmlWatts[device]) > TestSettings.DiscreteGpuWattsLimit);
    }

    /// <summary>Works out rest_w_per_gpu_pct.</summary>
    /// <param name="reference">The reference phase (3).</param>
    /// <param name="graphics">The Graphics phase (6).</param>
    /// <param name="restWattsPerCpuPercent">The model value rest_w_per_cpu_pct.</param>
    /// <param name="parts">The measured parts that are believed.</param>
    /// <param name="cpuSpan">Rise of the processor use in points.</param>
    /// <param name="gpuSpan">Rise of the 3D use in points.</param>
    /// <returns>The value, at least 0; 0 when no part is measured or a rest value is unknown.</returns>
    private static double RestPerGpu(PhaseResult reference, PhaseResult graphics, double restWattsPerCpuPercent, IReadOnlyList<string> parts, double cpuSpan, double gpuSpan)
    {
        double? rest3 = RestPower.Rest(reference, parts);
        double? rest6 = RestPower.Rest(graphics, parts);
        if (parts.Count == 0 || !rest3.HasValue || !rest6.HasValue) { return 0; }
        return Math.Max(0, (rest6.Value - rest3.Value - restWattsPerCpuPercent * cpuSpan) / gpuSpan);
    }

    /// <summary>Gives zero numbers with the given warning words.</summary>
    /// <param name="words">The words.</param>
    /// <returns>Both numbers 0.</returns>
    private static GraphicsTerms Zero(IReadOnlyList<string> words) => new(0, 0, words);
}
