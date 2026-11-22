// DiagExportPlan.cs: decides which files go into the diagnostics ZIP, with the masked status (spec D2, section 2.3).
// Who uses it: DiagExporter (the Export button) and the tests. A later "send" function can use the same list, because
// the list is built apart from the code that writes the ZIP and from the Save dialog.
// The ZIP holds only: scan\capabilities.json, scan\capabilities.service.json, scan\scan.log, scan\scan.service.log,
// history\*.csv, logs\service.log, logs\service.previous.log, logs\diag-*.jsonl, calibration\model.json and
// status\status.json without server_url, device_name and server_device_name. There is no path to config\, queue\,
// Client\ or logs\install.log in this file, so they cannot get in. ZIP names use "/" as the separator.
// A fixed file that is missing, over 64 MB or cannot be masked is counted in MissingCount and left out.
#nullable enable
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Builds the list of files for the export.</summary>
public static class DiagExportPlan
{
    /// <summary>Builds the list.</summary>
    /// <param name="dataFolder">The service data folder.</param>
    /// <returns>The files that exist now, and the count of fixed files that are missing.</returns>
    public static ExportPlan Build(string dataFolder)
    {
        var paths = new DiagPaths(dataFolder);
        var plan = new PlanBuilder();
        plan.AddFile("scan/capabilities.json", paths.CapabilitiesFile);
        plan.AddFile("scan/capabilities.service.json", paths.CapabilitiesServiceFile);
        plan.AddFile("scan/scan.log", Path.Combine(paths.ScanFolder, "scan.log"));
        plan.AddFile("scan/scan.service.log", Path.Combine(paths.ScanFolder, "scan.service.log"));
        plan.AddFiles(paths.HistoryFolder, "*.csv", "history", IsCsv);
        plan.AddFile("logs/service.log", paths.ServiceLog);
        plan.AddFile("logs/service.previous.log", Path.Combine(paths.LogsFolder, "service.previous.log"));
        plan.AddFiles(paths.LogsFolder, "diag-*.jsonl", "logs", DiagFileName.IsDiagFile);
        plan.AddFile("calibration/model.json", Path.Combine(paths.CalibrationFolder, "model.json"));
        plan.AddMaskedStatus(paths.StatusFile);
        return plan.ToPlan();
    }

    /// <summary>Tells whether a file name ends with .csv. The Windows file search also finds longer extensions, so the name is checked again.</summary>
    /// <param name="path">The file path.</param>
    /// <returns>True for a .csv file.</returns>
    private static bool IsCsv(string path) => path.EndsWith(".csv", StringComparison.OrdinalIgnoreCase);

    /// <summary>Collects the entries.</summary>
    private sealed class PlanBuilder
    {
        private readonly List<ExportEntry> entries = new();
        private int missing;

        /// <summary>Adds one fixed file when it exists and is not too big. Else it counts as missing.</summary>
        /// <param name="zipPath">The name inside the ZIP.</param>
        /// <param name="source">The file on disk.</param>
        public void AddFile(string zipPath, string source)
        {
            long length = SafeFile.Length(source);
            if (length < 0 || length > DiagLimits.MaxFileBytes)
            {
                missing++;
                return;
            }
            entries.Add(new ExportEntry(zipPath, source, null));
        }

        /// <summary>Adds the files of one folder that match a pattern. Sub folders are not searched.</summary>
        /// <param name="folder">The folder.</param>
        /// <param name="pattern">The file search pattern.</param>
        /// <param name="zipFolder">The folder name inside the ZIP.</param>
        /// <param name="accept">Checks each found path again.</param>
        public void AddFiles(string folder, string pattern, string zipFolder, Func<string, bool> accept)
        {
            foreach (string file in ListFiles(folder, pattern, accept)) AddFile(zipFolder + "/" + Path.GetFileName(file), file);
        }

        /// <summary>Adds status.json with the private fields removed. A file that is missing or cannot be masked counts as missing.</summary>
        /// <param name="statusFile">The status file.</param>
        public void AddMaskedStatus(string statusFile)
        {
            byte[]? bytes = SafeFile.TryReadBytes(statusFile, DiagLimits.MaxFileBytes);
            byte[]? masked = bytes is null ? null : StatusMasker.Mask(bytes);
            if (masked is null)
            {
                missing++;
                return;
            }
            entries.Add(new ExportEntry("status/status.json", null, masked));
        }

        /// <summary>Gives the result.</summary>
        /// <returns>The plan.</returns>
        public ExportPlan ToPlan() => new(entries, missing);

        /// <summary>Lists the matching files of a folder. A missing or unreadable folder gives none.</summary>
        /// <param name="folder">The folder.</param>
        /// <param name="pattern">The file search pattern.</param>
        /// <param name="accept">Checks each path.</param>
        /// <returns>The accepted file paths, in name order.</returns>
        private static List<string> ListFiles(string folder, string pattern, Func<string, bool> accept)
        {
            try { return Directory.GetFiles(folder, pattern, SearchOption.TopDirectoryOnly).Where(accept).Order(StringComparer.Ordinal).ToList(); }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException) { return new List<string>(); }
        }
    }
}
