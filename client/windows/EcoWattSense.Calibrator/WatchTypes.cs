// WatchTypes.cs: the data of the stream that "ews-scan.exe --watch" prints (spec section 1.6): the header (what is measured) and one line per period.
// Who uses it: WatchParser builds them from text; WatchSession and WatchClient keep them; Sample stores the latest line;
// StreamPhase, PowerModel and ResultWriter work from them. No Windows calls and no window code, so the console tests compile this file.
namespace EnergyCalibrator;

/// <summary>One energy channel of the header.</summary>
/// <param name="Name">The channel name, for example RAPL_Package0_PKG. Cut to 128 characters, control characters removed.</param>
/// <param name="Kind">The kind the scanner gave it: cpu_package, dram, cpu_core, cpu_cores, igpu, system or other. Unknown words become other.</param>
/// <param name="Device">Number of the EMI device.</param>
/// <param name="Index">Number of the channel in its device.</param>
internal sealed record WatchChannel(string Name, string Kind, int Device, int Index);

/// <summary>One NVML device of the header.</summary>
/// <param name="Name">The device name, cleaned like a channel name.</param>
/// <param name="Adapter">Index in the adapter list of the scanner, or null.</param>
/// <param name="Power">True when the power read answered.</param>
/// <param name="Energy">True when the total energy counter answered.</param>
internal sealed record WatchNvml(string Name, int? Adapter, bool Power, bool Energy);

/// <summary>One graphics adapter whose use the stream reports.</summary>
/// <param name="Adapter">Index in the adapter list of the scanner.</param>
/// <param name="Role">only, power_saving, high_performance or other.</param>
/// <param name="VendorId">PCI vendor id.</param>
internal sealed record WatchGpuUse(int Adapter, string Role, int VendorId);

/// <summary>The processor fields of the header. A field the scanner left out is null.</summary>
/// <param name="CpuVendor">Vendor string, for example AuthenticAMD. Letters, digits and a few signs only, at most 32 characters.</param>
/// <param name="CpuFamily">Processor family.</param>
/// <param name="CpuModel">Processor model.</param>
/// <param name="PhysicalCores">Number of physical cores.</param>
internal sealed record WatchMachine(string? CpuVendor, int? CpuFamily, int? CpuModel, int? PhysicalCores);

/// <summary>The battery fields of the header. A field the scanner left out is null.</summary>
/// <param name="DesignMwh">Designed capacity in mWh.</param>
/// <param name="FullMwh">Full charge capacity in mWh.</param>
/// <param name="CycleCount">Charge cycles.</param>
internal sealed record WatchBattery(int? DesignMwh, int? FullMwh, int? CycleCount);

/// <summary>The display fields of the header. A field the scanner left out is null.</summary>
/// <param name="Displays">Active displays.</param>
/// <param name="ExternalDisplays">Active displays that are not the built-in panel.</param>
/// <param name="RefreshHz">Refresh rate of the built-in panel.</param>
internal sealed record WatchDisplay(int? Displays, int? ExternalDisplays, double? RefreshHz);

/// <summary>The first line of the stream.</summary>
/// <param name="PeriodMs">Milliseconds between two lines.</param>
/// <param name="Emi">The energy channels, in the order of the lists of every line.</param>
/// <param name="Nvml">The NVML devices.</param>
/// <param name="GpuUse">The adapters whose use is reported.</param>
/// <param name="Machine">Processor fields.</param>
/// <param name="Battery">Battery fields.</param>
/// <param name="Display">Display fields.</param>
internal sealed record WatchHeader(
    int PeriodMs,
    IReadOnlyList<WatchChannel> Emi,
    IReadOnlyList<WatchNvml> Nvml,
    IReadOnlyList<WatchGpuUse> GpuUse,
    WatchMachine Machine,
    WatchBattery Battery,
    WatchDisplay Display);

/// <summary>One line of the stream after the header. A list has the length of the matching list of the header. A null entry means "unknown".</summary>
/// <param name="TimeMs">Milliseconds on the monotonic clock of the scanner since it started.</param>
/// <param name="EmiUj">Energy of each channel in microjoules since its first good read.</param>
/// <param name="NvmlMw">Power of each NVML device in milliwatts.</param>
/// <param name="NvmlMj">Total energy of each NVML device in millijoules since its first good read.</param>
/// <param name="GpuPct">Use of each adapter in percent.</param>
internal sealed record WatchSample(
    long TimeMs,
    IReadOnlyList<long?> EmiUj,
    IReadOnlyList<double?> NvmlMw,
    IReadOnlyList<double?> NvmlMj,
    IReadOnlyList<double?> GpuPct);

/// <summary>The kind words of the energy channels that the calibrator uses.</summary>
internal static class WatchKinds
{
    /// <summary>The whole processor package.</summary>
    public const string Package = "cpu_package";

    /// <summary>The memory.</summary>
    public const string Dram = "dram";

    /// <summary>The processor cores (Intel PP0).</summary>
    public const string Cores = "cpu_cores";

    /// <summary>The integrated graphics (Intel PP1).</summary>
    public const string Igpu = "igpu";

    /// <summary>One physical core of a package (AMD). Part of the package, never a measured part.</summary>
    public const string Core = "cpu_core";

    /// <summary>The whole computer.</summary>
    public const string System = "system";

    /// <summary>Everything else.</summary>
    public const string Other = "other";

    /// <summary>The words the calibrator accepts from the stream.</summary>
    public static readonly IReadOnlySet<string> Known = new HashSet<string> { Package, Dram, Cores, Igpu, Core, System, Other };
}
