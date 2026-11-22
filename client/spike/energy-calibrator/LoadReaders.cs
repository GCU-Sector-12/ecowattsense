// LoadReaders.cs: readers for CPU use, user input, the performance counters and the optional Energy Meter counter set.
// Who uses it: SampleCollector, once per sample. None of them needs administrator rights.
// Counter names are the English ones. .NET looks them up in the English counter strings first, so they normally work on any
// display language. When a category is missing, the reader says so and the test goes on without that source.
// A counter value needs two reads (it is a rate or a percentage over time), so the first sample has no counter values.
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace EnergyCalibrator;

/// <summary>Total CPU use over all cores, from GetSystemTimes. Needs two reads, so the first sample has no value.</summary>
internal sealed class CpuTimesReader
{
    private bool hasPrevious;
    private ulong previousIdle;
    private ulong previousKernel;
    private ulong previousUser;

    /// <summary>Reads the times and stores the CPU use since the previous read.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">GetSystemTimes failed.</exception>
    public void Read(Sample sample)
    {
        if (!NativeMethods.GetSystemTimes(out NativeMethods.FileTime idle, out NativeMethods.FileTime kernel, out NativeMethods.FileTime user))
        {
            throw new SourceUnavailableException($"GetSystemTimes failed, error {Marshal.GetLastWin32Error()}");
        }
        double? use = Utilisation(idle.ToUInt64(), kernel.ToUInt64(), user.ToUInt64());
        sample.SetChecked(SampleKeys.CpuUtil, use, 0, 100);
    }

    /// <summary>Works out the busy share since the previous read and remembers the new times.</summary>
    /// <param name="idle">Idle time now (100 ns units).</param>
    /// <param name="kernel">Kernel time now; it includes the idle time.</param>
    /// <param name="user">User time now.</param>
    /// <returns>The busy percentage, or null on the first read or when no time has passed.</returns>
    private double? Utilisation(ulong idle, ulong kernel, ulong user)
    {
        bool first = !hasPrevious;
        double total = (double)(kernel - previousKernel) + (double)(user - previousUser);
        double idleDelta = idle - previousIdle;
        hasPrevious = true;
        previousIdle = idle;
        previousKernel = kernel;
        previousUser = user;
        if (first || total <= 0) { return null; }
        return 100.0 * (total - idleDelta) / total;
    }
}

/// <summary>Seconds since the last mouse or keyboard input, from GetLastInputInfo.</summary>
internal sealed class UserInputReader
{
    /// <summary>Reads the idle time and stores it.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <exception cref="SourceUnavailableException">GetLastInputInfo failed.</exception>
    public void Read(Sample sample)
    {
        NativeMethods.LastInputInfo info = new() { Size = (uint)Marshal.SizeOf<NativeMethods.LastInputInfo>() };
        if (!NativeMethods.GetLastInputInfo(ref info))
        {
            throw new SourceUnavailableException($"GetLastInputInfo failed, error {Marshal.GetLastWin32Error()}");
        }
        // Both tick counts are 32 bit and wrap together, so the unsigned difference is right even after a wrap.
        uint now = unchecked((uint)Environment.TickCount64);
        uint idleMilliseconds = unchecked(now - info.Time);
        sample.SetChecked(SampleKeys.UserIdle, idleMilliseconds / 1000.0, 0, 86_400.0 * 30);
    }
}

/// <summary>
/// Reads one counter of one category for all its instances at once (one registry read per sample) and works out
/// the values from two consecutive reads. This is much cheaper than one PerformanceCounter object per instance,
/// which matters for "GPU Engine", where every process has several instances.
/// </summary>
internal sealed class CategorySnapshotReader
{
    private readonly string categoryName;
    private readonly string counterName;
    private Dictionary<string, CounterSample> previous = new();
    private bool? categoryExists;

    /// <summary>Creates a reader for one counter.</summary>
    /// <param name="categoryName">English category name, for example "GPU Engine".</param>
    /// <param name="counterName">English counter name, for example "Utilization Percentage".</param>
    public CategorySnapshotReader(string categoryName, string counterName)
    {
        this.categoryName = categoryName;
        this.counterName = counterName;
    }

    /// <summary>Reads all instances now and calculates the value of each instance that also existed in the previous read.</summary>
    /// <returns>Instance name to value. Empty on the first read.</returns>
    /// <exception cref="SourceUnavailableException">The category or the counter does not exist.</exception>
    public Dictionary<string, double> ReadValues()
    {
        InstanceDataCollection instances = ReadInstances();
        Dictionary<string, CounterSample> current = new();
        Dictionary<string, double> values = new();
        foreach (InstanceData data in instances.Values) { Collect(data, current, values); }
        previous = current;
        return values;
    }

    /// <summary>Reads the whole category and picks the counter.</summary>
    /// <returns>All instances of the counter.</returns>
    /// <exception cref="SourceUnavailableException">The category or the counter does not exist.</exception>
    private InstanceDataCollection ReadInstances()
    {
        categoryExists ??= PerformanceCounterCategory.Exists(categoryName);
        if (categoryExists == false)
        {
            throw new SourceUnavailableException($"counter set '{categoryName}' not found (English counter names are used)");
        }
        InstanceDataCollectionCollection all = new PerformanceCounterCategory(categoryName).ReadCategory();
        if (!all.Contains(counterName)) { throw new SourceUnavailableException($"counter '{counterName}' not found in '{categoryName}'"); }
        return all[counterName];
    }

    /// <summary>Remembers one instance and, when the previous read had it too, calculates its value.</summary>
    /// <param name="data">The instance data.</param>
    /// <param name="current">The samples of this read, to become the "previous" ones.</param>
    /// <param name="values">The calculated values.</param>
    private void Collect(InstanceData data, Dictionary<string, CounterSample> current, Dictionary<string, double> values)
    {
        current[data.InstanceName] = data.Sample;
        if (!previous.TryGetValue(data.InstanceName, out CounterSample old)) { return; }
        values[data.InstanceName] = CounterSample.Calculate(old, data.Sample);
    }
}

/// <summary>The performance counters of the program, one method per source so that each can fail on its own.</summary>
internal sealed class CounterReaders
{
    private const string TotalInstance = "_Total";
    private readonly CategorySnapshotReader cpuPerformance = new("Processor Information", "% Processor Performance");
    private readonly CategorySnapshotReader cpuFrequency = new("Processor Information", "Processor Frequency");
    private readonly CategorySnapshotReader disk = new("PhysicalDisk", "Disk Bytes/sec");
    private readonly CategorySnapshotReader thermal = new("Thermal Zone Information", "Temperature");
    private readonly CategorySnapshotReader gpu = new("GPU Engine", "Utilization Percentage");
    private readonly CategorySnapshotReader network = new("Network Interface", "Bytes Total/sec");

    /// <summary>Stores "Processor Information(_Total)\% Processor Performance". Zero is treated as unknown.</summary>
    /// <param name="sample">The sample to fill.</param>
    public void ReadCpuPerformance(Sample sample) => sample.SetChecked(SampleKeys.CpuPerformance, TotalOf(cpuPerformance.ReadValues()), 1, 1000);

    /// <summary>Stores "Processor Information(_Total)\Processor Frequency" in MHz. Zero is treated as unknown.</summary>
    /// <param name="sample">The sample to fill.</param>
    public void ReadCpuFrequency(Sample sample) => sample.SetChecked(SampleKeys.CpuMhz, TotalOf(cpuFrequency.ReadValues()), 1, 10_000);

    /// <summary>Stores "PhysicalDisk(_Total)\Disk Bytes/sec".</summary>
    /// <param name="sample">The sample to fill.</param>
    public void ReadDisk(Sample sample) => sample.SetChecked(SampleKeys.DiskBytes, TotalOf(disk.ReadValues()), 0, 1e12);

    /// <summary>Stores the sum of "Network Interface(*)\Bytes Total/sec" over all adapters.</summary>
    /// <param name="sample">The sample to fill.</param>
    public void ReadNetwork(Sample sample) => sample.SetChecked(SampleKeys.NetBytes, SumOf(network.ReadValues()), 0, 1e12);

    /// <summary>
    /// Stores the use of the busiest GPU 3D engine. Instances of "GPU Engine(*)" whose name holds "engtype_3D" are grouped
    /// by engine (the name without the process id), summed per engine over all processes, and the highest engine is taken.
    /// </summary>
    /// <param name="sample">The sample to fill.</param>
    public void ReadGpu(Sample sample) => sample.SetChecked(SampleKeys.Gpu3D, Busiest3DEngine(gpu.ReadValues()), 0, 100);

    /// <summary>Stores the thermal zone temperatures (Kelvin converted to Celsius) and the highest believable one.</summary>
    /// <param name="sample">The sample to fill.</param>
    public void ReadThermal(Sample sample)
    {
        Dictionary<string, double> kelvin = thermal.ReadValues();
        List<double> celsius = kelvin.OrderBy(pair => pair.Key, StringComparer.Ordinal).Select(pair => pair.Value - 273.15).ToList();
        // Zones are numbered by the sorted order of their names, so no ACPI path ends up in the files.
        for (int i = 0; i < celsius.Count; i++) { sample.SetChecked($"{SampleKeys.ThermalZonePrefix}{i + 1}_c", celsius[i], -50, 200); }
        sample.SetChecked(SampleKeys.ThermalMax, MaxBelievable(celsius), -50, 200);
    }

    /// <summary>Picks the "_Total" instance.</summary>
    /// <param name="values">Instance name to value.</param>
    /// <returns>The total, or null when there is none yet.</returns>
    private static double? TotalOf(Dictionary<string, double> values) => values.TryGetValue(TotalInstance, out double total) ? total : null;

    /// <summary>Adds all instances up.</summary>
    /// <param name="values">Instance name to value.</param>
    /// <returns>The sum, or null when there are no values yet.</returns>
    private static double? SumOf(Dictionary<string, double> values) => values.Count == 0 ? null : values.Values.Sum();

    /// <summary>Takes the highest temperature that is believable (between -50 and 200 degrees Celsius).</summary>
    /// <param name="celsius">Temperatures in degrees Celsius.</param>
    /// <returns>The highest believable one, or null when there is none.</returns>
    private static double? MaxBelievable(List<double> celsius)
    {
        List<double> believable = celsius.Where(value => value > -50 && value < 200).ToList();
        if (believable.Count == 0) { return null; }
        return believable.Max();
    }

    /// <summary>Works out the use of the busiest 3D engine.</summary>
    /// <param name="values">Instance name to utilisation percentage.</param>
    /// <returns>The use of the busiest engine, at most 100, or null when no 3D instance was read.</returns>
    private static double? Busiest3DEngine(Dictionary<string, double> values)
    {
        List<double> perEngine = values
            .Where(pair => pair.Key.Contains("engtype_3D", StringComparison.OrdinalIgnoreCase))
            .GroupBy(pair => EngineKey(pair.Key))
            .Select(group => group.Sum(pair => pair.Value))
            .ToList();
        if (perEngine.Count == 0) { return null; }
        return Math.Min(100.0, perEngine.Max());
    }

    /// <summary>Removes the process id from a GPU engine instance name, so all processes of one engine share a key.</summary>
    /// <param name="instance">Instance name such as pid_12_luid_0x0_0x1_phys_0_eng_0_engtype_3D.</param>
    /// <returns>The part from "_luid_" on, or the whole name when it has none.</returns>
    private static string EngineKey(string instance)
    {
        int cut = instance.IndexOf("_luid_", StringComparison.OrdinalIgnoreCase);
        return cut >= 0 ? instance[cut..] : instance;
    }
}

/// <summary>The raw Energy (picowatt-hours) and Time (milliseconds) counters of one Energy Meter instance, read at one moment.</summary>
/// <param name="Energy">Cumulative energy in picowatt-hours.</param>
/// <param name="Milliseconds">Cumulative time in milliseconds (in this counter set the unit is ms, not 100 ns).</param>
internal readonly record struct EnergyReading(long Energy, long Milliseconds);

/// <summary>The energy used and the time that passed between two readings of one instance.</summary>
/// <param name="Pwh">Energy in picowatt-hours.</param>
/// <param name="Milliseconds">Time in milliseconds.</param>
internal readonly record struct EnergyStep(double Pwh, double Milliseconds);

/// <summary>
/// Optional "Energy Meter" counter set (Windows 11 with RAPL energy counters; a normal user may read it). Logs the raw Energy and Time
/// values of every instance and the power of the package, cores, graphics and memory in watts since the previous sample.
/// </summary>
internal sealed class EnergyMeterReader
{
    private const string Category = "Energy Meter";
    private const string PackageInstance = "RAPL_Package0_PKG";

    // 1 picowatt-hour is 3.6e-9 joules, so picowatt-hours per millisecond x 3.6e-6 gives watts.
    private const double WattsPerPwhPerMillisecond = 3.6e-6;

    // A value above this many watts is a bad read (a counter that was reset or wrapped), so it stays unknown.
    private const double MaxBelievableWatts = 500.0;

    // PKG is the whole package and already holds PP0 and PP1; the four values must never be added up.
    private static readonly (string Instance, string Key)[] Channels =
    [
        (PackageInstance, SampleKeys.EmiPackageWatts),
        ("RAPL_Package0_PP0", SampleKeys.EmiCoresWatts),
        ("RAPL_Package0_PP1", SampleKeys.EmiGpuWatts),
        ("RAPL_Package0_DRAM", SampleKeys.EmiDramWatts)
    ];

    private Dictionary<string, EnergyReading> previous = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>Reads the Energy and Time counters of all instances and works out the watts since the previous read.</summary>
    /// <param name="sample">The sample to fill. The first read has no watts, because a power needs two reads.</param>
    /// <exception cref="SourceUnavailableException">The counter set is missing or has no instance.</exception>
    public void Read(Sample sample)
    {
        InstanceDataCollectionCollection all = OpenSet();
        StoreRaw(sample, all["Energy"], "energy");
        StoreRaw(sample, all["Time"], "time");
        Dictionary<string, EnergyReading> now = Readings(all["Energy"], all["Time"]);
        foreach ((string instance, string key) in Channels) { StoreWatts(sample, key, Step(instance, now)); }
        sample.EmiPackageDeltaPwh = Step(PackageInstance, now)?.Pwh;
        previous = now;
    }

    /// <summary>Reads the whole counter set and checks that it has what is needed.</summary>
    /// <returns>The counter data.</returns>
    /// <exception cref="SourceUnavailableException">The counter set is missing or has no instance.</exception>
    private static InstanceDataCollectionCollection OpenSet()
    {
        if (!PerformanceCounterCategory.Exists(Category)) { throw new SourceUnavailableException("counter set 'Energy Meter' not found (Windows 11 with a supported processor only)"); }
        InstanceDataCollectionCollection all = new PerformanceCounterCategory(Category).ReadCategory();
        if (!all.Contains("Energy") || !all.Contains("Time")) { throw new SourceUnavailableException("'Energy Meter' has no Energy and Time counters"); }
        if (all["Energy"].Count == 0) { throw new SourceUnavailableException("'Energy Meter' has no instances"); }
        return all;
    }

    /// <summary>Stores the raw value of every instance of one counter.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <param name="counter">The counter data.</param>
    /// <param name="suffix">The end of the column name.</param>
    private static void StoreRaw(Sample sample, InstanceDataCollection counter, string suffix)
    {
        foreach (InstanceData data in counter.Values) { sample.Set($"{SampleKeys.EnergyMeterPrefix}{Clean(data.InstanceName)}_{suffix}", data.RawValue); }
    }

    /// <summary>Pairs the Energy and the Time value of every instance that has both.</summary>
    /// <param name="energy">The Energy counter data.</param>
    /// <param name="time">The Time counter data.</param>
    /// <returns>The readings by instance name (case does not matter).</returns>
    private static Dictionary<string, EnergyReading> Readings(InstanceDataCollection energy, InstanceDataCollection time)
    {
        Dictionary<string, long> times = RawValues(time);
        Dictionary<string, EnergyReading> readings = new(StringComparer.OrdinalIgnoreCase);
        foreach (KeyValuePair<string, long> pair in RawValues(energy)) { AddReading(readings, pair, times); }
        return readings;
    }

    /// <summary>Adds one instance to the readings when its Time value is known.</summary>
    /// <param name="readings">The readings to fill.</param>
    /// <param name="energy">The instance name and its Energy value.</param>
    /// <param name="times">The Time values by instance name.</param>
    private static void AddReading(Dictionary<string, EnergyReading> readings, KeyValuePair<string, long> energy, Dictionary<string, long> times)
    {
        if (!times.TryGetValue(energy.Key, out long milliseconds)) { return; }
        readings[energy.Key] = new EnergyReading(energy.Value, milliseconds);
    }

    /// <summary>Takes the raw value of every instance of one counter.</summary>
    /// <param name="counter">The counter data.</param>
    /// <returns>The raw values by instance name (case does not matter).</returns>
    private static Dictionary<string, long> RawValues(InstanceDataCollection counter)
    {
        Dictionary<string, long> raw = new(StringComparer.OrdinalIgnoreCase);
        foreach (InstanceData data in counter.Values) { raw[data.InstanceName] = data.RawValue; }
        return raw;
    }

    /// <summary>Works out the energy and time between the previous read and this one for one instance.</summary>
    /// <param name="instance">The instance name.</param>
    /// <param name="now">The readings of this read.</param>
    /// <returns>The step, or null when the instance is missing, there was no earlier read, or a counter did not move forward (reset).</returns>
    private EnergyStep? Step(string instance, Dictionary<string, EnergyReading> now)
    {
        if (!now.TryGetValue(instance, out EnergyReading current)) { return null; }
        if (!previous.TryGetValue(instance, out EnergyReading before)) { return null; }
        long pwh = current.Energy - before.Energy;
        long milliseconds = current.Milliseconds - before.Milliseconds;
        if (pwh < 0 || milliseconds <= 0) { return null; }
        return new EnergyStep(pwh, milliseconds);
    }

    /// <summary>Stores the watts of a step; an unknown step or an unbelievable value stays unknown.</summary>
    /// <param name="sample">The sample to fill.</param>
    /// <param name="key">The column name.</param>
    /// <param name="step">The energy and time between two reads, or null.</param>
    private static void StoreWatts(Sample sample, string key, EnergyStep? step)
    {
        if (step is not EnergyStep known) { return; }
        sample.SetChecked(key, known.Pwh * WattsPerPwhPerMillisecond / known.Milliseconds, 0, MaxBelievableWatts);
    }

    /// <summary>Keeps only letters and digits of an instance name, for use in a column name.</summary>
    /// <param name="name">The instance name.</param>
    /// <returns>The cleaned name.</returns>
    private static string Clean(string name) => new(name.Where(char.IsLetterOrDigit).ToArray());
}
