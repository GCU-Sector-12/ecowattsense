// DiagPaths.cs: the folders and files of the service data that the Developer page reads and exports.
// Who uses it: the snapshot loader, the file watchers of the page, the export, and the tests.
// It takes the data folder as text, so the tests can point it at a temporary folder.
#nullable enable
using System.IO;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>The paths below the service data folder (SPEC 2.2).</summary>
/// <param name="DataFolder">The data folder, normally &lt;ProgramData&gt;\EcoWattSense.</param>
public sealed record DiagPaths(string DataFolder)
{
    /// <summary>The logs folder: diag-*.jsonl and service.log.</summary>
    public string LogsFolder => Path.Combine(DataFolder, "logs");

    /// <summary>The scan folder: capabilities*.json.</summary>
    public string ScanFolder => Path.Combine(DataFolder, "scan");

    /// <summary>The status folder: status.json.</summary>
    public string StatusFolder => Path.Combine(DataFolder, "status");

    /// <summary>The history folder: one CSV file per day.</summary>
    public string HistoryFolder => Path.Combine(DataFolder, "history");

    /// <summary>The calibration folder: model.json.</summary>
    public string CalibrationFolder => Path.Combine(DataFolder, "calibration");

    /// <summary>The service log.</summary>
    public string ServiceLog => Path.Combine(LogsFolder, "service.log");

    /// <summary>The status file.</summary>
    public string StatusFile => Path.Combine(StatusFolder, "status.json");

    /// <summary>The scan result of the service context.</summary>
    public string CapabilitiesServiceFile => Path.Combine(ScanFolder, "capabilities.service.json");

    /// <summary>The scan result of the user context.</summary>
    public string CapabilitiesFile => Path.Combine(ScanFolder, "capabilities.json");
}
