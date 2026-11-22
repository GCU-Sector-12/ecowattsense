// MachineInfo.cs: maker, model and processor of this computer, as the result files and model.json hold them.
// Who uses it: ServiceModel, ResultWriter and MainWindow. The WMI reading (Read) is in WmiReaders.cs, the other half of the same type,
// so the console tests can compile this half without any Windows call.
namespace EnergyCalibrator;

/// <summary>Maker, model and processor of this computer. It holds no computer name, user name, serial number or device id.</summary>
/// <param name="Maker">Manufacturer from Win32_ComputerSystem.</param>
/// <param name="Model">Model from Win32_ComputerSystem.</param>
/// <param name="Cpu">Processor name from Win32_Processor.</param>
/// <param name="LogicalProcessors">Number of logical processors.</param>
/// <param name="Windows">Windows version number.</param>
internal sealed partial record MachineInfo(string Maker, string Model, string Cpu, int LogicalProcessors, string Windows)
{
    /// <summary>Used until the real values are read, and when they cannot be read.</summary>
    public static MachineInfo Unknown { get; } = new("unknown", "unknown", "unknown", Environment.ProcessorCount, Environment.OSVersion.Version.ToString());
}
