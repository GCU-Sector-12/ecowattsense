// DiagSnapshotLoader.cs: reads everything the Developer page shows, in one call that runs off the window thread.
// Who uses it: MainWindow.Developer.cs (on a file change event while the page is visible) and the tests.
// The result holds plain data. The window thread only copies it into the grids, so a slow disk never freezes the window.
#nullable enable
using System;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Reads all the data of the Developer page.</summary>
public static class DiagSnapshotLoader
{
    /// <summary>Reads the diag files, the two scan files, status.json and the end of service.log.</summary>
    /// <param name="dataFolder">The service data folder.</param>
    /// <param name="nowUtc">The time now, for the 24 hour limit of the Readings tab.</param>
    /// <returns>The snapshot. A source that cannot be read is marked unavailable; nothing throws for file problems.</returns>
    public static DiagSnapshot Load(string dataFolder, DateTimeOffset nowUtc)
    {
        var paths = new DiagPaths(dataFolder);
        DiagReadResult counts = DiagFileReader.Read(paths.LogsFolder, nowUtc);
        return new DiagSnapshot
        {
            Readings = DiagTableBuilder.Build(counts.Readings, "ts"),
            Reports = DiagTableBuilder.Build(counts.Reports, "ts", "queue"),
            SendsEvents = DiagTableBuilder.Build(counts.SendsEvents, "ts", "t"),
            Counts = counts,
            CapabilitiesService = JsonFileRows.Read(paths.CapabilitiesServiceFile),
            Capabilities = JsonFileRows.Read(paths.CapabilitiesFile),
            Status = JsonFileRows.Read(paths.StatusFile),
            Log = LogTailReader.Read(paths.ServiceLog)
        };
    }
}
