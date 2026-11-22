// DiagExporter.cs: the whole export in one call: build the list, write the ZIP.
// Who uses it: the Export button of the Developer page (MainWindow.Developer.cs), off the window thread.
// The Save dialog is not here: the window asks for the path, then calls Export. Keeping the list (DiagExportPlan) and the
// writer (DiagZipWriter) apart lets a later "send" function use the same list.
#nullable enable
using System;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Runs the export.</summary>
public static class DiagExporter
{
    /// <summary>Builds the list and writes the ZIP.</summary>
    /// <param name="dataFolder">The service data folder.</param>
    /// <param name="targetPath">The full path of the ZIP file the user chose.</param>
    /// <returns>The result with the counts. Saved is false when the export failed.</returns>
    public static ExportResult Export(string dataFolder, string targetPath)
        => DiagZipWriter.Write(DiagExportPlan.Build(dataFolder), targetPath);
}
