// StreamPhase.cs: works out, for one phase, the power of each energy channel, of each NVML device and the use of each adapter
// from the stream lines that the used samples carry (spec 2.1).
// Who uses it: PhaseAnalyser fills PhaseResult.Stream with it; PowerModel, ResultWriter and the checks read it.
namespace EnergyCalibrator;

/// <summary>The stream numbers of one phase. Every list has the order and length of the header list. A null entry is "unknown".</summary>
/// <param name="ChannelKinds">The kind of each energy channel.</param>
/// <param name="ChannelWatts">Power of each energy channel in watts.</param>
/// <param name="NvmlWatts">Mean power of each NVML device in watts.</param>
/// <param name="GpuUsePercent">Mean use of each adapter in percent.</param>
/// <param name="PackageMwh">Energy of the package channels over the used samples in mWh; null when unknown.</param>
internal sealed record StreamPhase(
    IReadOnlyList<string> ChannelKinds,
    IReadOnlyList<double?> ChannelWatts,
    IReadOnlyList<double?> NvmlWatts,
    IReadOnlyList<double?> GpuUsePercent,
    double? PackageMwh)
{
    private const double MicrojoulesPerJoule = 1e6;
    private const double MicrojoulesPerMilliwattHour = 3.6e6;

    /// <summary>Gives the power of one kind: the sum of its channels.</summary>
    /// <param name="kind">The kind word.</param>
    /// <returns>The sum; null when the header has no channel of that kind or one of them is unknown.</returns>
    public double? KindWatts(string kind)
    {
        List<double?> watts = ChannelKinds.Zip(ChannelWatts).Where(pair => pair.First == kind).Select(pair => pair.Second).ToList();
        if (watts.Count == 0 || watts.Any(IsUnknown)) { return null; }
        return watts.Sum(value => value.GetValueOrDefault());
    }

    /// <summary>Tells whether every energy channel of the phase is unknown, although the header has channels.</summary>
    /// <returns>True when the stream gave no usable energy value in this phase.</returns>
    public bool NoEnergyData() => ChannelWatts.Count > 0 && ChannelWatts.All(IsUnknown);

    /// <summary>Works out the numbers of a phase.</summary>
    /// <param name="used">The samples of the phase after the settling time, in time order.</param>
    /// <param name="header">The header of the stream.</param>
    /// <returns>The numbers.</returns>
    public static StreamPhase Measure(IReadOnlyList<Sample> used, WatchHeader header)
    {
        List<WatchSample?> lines = used.Select(sample => sample.Watch).ToList();
        List<long?> energy = Enumerable.Range(0, header.Emi.Count).Select(index => EnergyUj(lines, index)).ToList();
        double? seconds = SecondsBetween(lines);
        return new StreamPhase(
            header.Emi.Select(channel => channel.Kind).ToList(),
            energy.Select(uj => WattsOf(uj, seconds)).ToList(),
            Enumerable.Range(0, header.Nvml.Count).Select(index => MeanNvmlWatts(lines, index)).ToList(),
            Enumerable.Range(0, header.GpuUse.Count).Select(index => MeanOf(lines.Select(line => Entry(line?.GpuPct, index)))).ToList(),
            PackageEnergyMwh(header, energy));
    }

    /// <summary>Tells whether a value is unknown.</summary>
    /// <param name="value">The value.</param>
    /// <returns>True when it is null.</returns>
    private static bool IsUnknown(double? value) => !value.HasValue;

    /// <summary>Tells whether a value is known.</summary>
    /// <param name="value">The value.</param>
    /// <returns>True when it is not null.</returns>
    private static bool IsKnown(double? value) => value.HasValue;

    /// <summary>Tells whether a line is missing.</summary>
    /// <param name="line">The line, or null.</param>
    /// <returns>True when it is null.</returns>
    private static bool IsMissing(WatchSample? line) => line == null;

    /// <summary>Takes one entry of a list that may be missing or too short.</summary>
    /// <param name="list">The list, or null.</param>
    /// <param name="index">The position.</param>
    /// <returns>The entry, or null.</returns>
    private static double? Entry(IReadOnlyList<double?>? list, int index)
    {
        bool there = list != null && index < list.Count;
        return there ? list![index] : null;
    }

    /// <summary>Works out the energy one channel used between the first and the last line.</summary>
    /// <param name="lines">The line of each used sample; null for a sample without a fresh line.</param>
    /// <param name="index">Position of the channel in the header.</param>
    /// <returns>The energy in microjoules; null when fewer than two samples, a sample has no line, a value is null or the energy fell.</returns>
    private static long? EnergyUj(List<WatchSample?> lines, int index)
    {
        if (lines.Count < 2 || lines.Any(IsMissing)) { return null; }
        List<long?> values = lines.Select(line => EnergyEntry(line!, index)).ToList();
        if (values.Any(value => value == null)) { return null; }
        long difference = values[^1]!.Value - values[0]!.Value;
        return difference < 0 ? null : difference;
    }

    /// <summary>Takes the energy entry of a channel.</summary>
    /// <param name="line">The line.</param>
    /// <param name="index">Position of the channel.</param>
    /// <returns>The entry, or null when the line has none.</returns>
    private static long? EnergyEntry(WatchSample line, int index)
    {
        bool there = index < line.EmiUj.Count;
        return there ? line.EmiUj[index] : null;
    }

    /// <summary>Works out the time between the first and the last line.</summary>
    /// <param name="lines">The line of each used sample.</param>
    /// <returns>The seconds; null when fewer than two samples, a sample has no line, or the time does not move forward.</returns>
    private static double? SecondsBetween(List<WatchSample?> lines)
    {
        if (lines.Count < 2 || lines.Any(IsMissing)) { return null; }
        long milliseconds = lines[^1]!.TimeMs - lines[0]!.TimeMs;
        return milliseconds > 0 ? milliseconds / 1000.0 : null;
    }

    /// <summary>Works out the power from an energy and a time.</summary>
    /// <param name="microjoules">The energy, or null.</param>
    /// <param name="seconds">The time, or null.</param>
    /// <returns>The power in watts, or null when one of them is unknown.</returns>
    private static double? WattsOf(long? microjoules, double? seconds)
    {
        if (microjoules == null || seconds == null) { return null; }
        return microjoules.Value / MicrojoulesPerJoule / seconds.Value;
    }

    /// <summary>Works out the mean power of an NVML device from the lines that exist.</summary>
    /// <param name="lines">The line of each used sample.</param>
    /// <param name="index">Position of the device in the header.</param>
    /// <returns>The mean in watts, or null when no line has a value.</returns>
    private static double? MeanNvmlWatts(List<WatchSample?> lines, int index)
    {
        double? milliwatts = MeanOf(lines.Select(line => Entry(line?.NvmlMw, index)));
        return milliwatts / 1000.0;
    }

    /// <summary>Works out the mean of the values that are known.</summary>
    /// <param name="values">The values.</param>
    /// <returns>The mean, or null when no value is known.</returns>
    private static double? MeanOf(IEnumerable<double?> values)
    {
        List<double?> known = values.Where(IsKnown).ToList();
        if (known.Count == 0) { return null; }
        return known.Average(value => value.GetValueOrDefault());
    }

    /// <summary>Adds up the energy of the package channels.</summary>
    /// <param name="header">The header.</param>
    /// <param name="energy">The energy of each channel in microjoules, or null.</param>
    /// <returns>The energy in mWh; null when there is no package channel or one of them is unknown.</returns>
    private static double? PackageEnergyMwh(WatchHeader header, List<long?> energy)
    {
        List<long?> package = header.Emi.Zip(energy).Where(pair => pair.First.Kind == WatchKinds.Package).Select(pair => pair.Second).ToList();
        if (package.Count == 0 || package.Any(value => value == null)) { return null; }
        return package.Sum(value => value.GetValueOrDefault()) / MicrojoulesPerMilliwattHour;
    }
}
