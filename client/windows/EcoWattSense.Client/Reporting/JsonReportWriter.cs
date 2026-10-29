using System;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace EcoWattSense.Client.Reporting;

/// <summary>Saves contract-shaped JSON for presentation and history; files are not an upload queue.</summary>
public sealed class JsonReportWriter
{
    private readonly string directory;
    public string LatestPath => Path.Combine(directory, "latest-report.json");

    public JsonReportWriter(string directory)
    {
        if (!Path.IsPathFullyQualified(directory))
            throw new ArgumentException("Report directory must be absolute.", nameof(directory));
        this.directory = directory;
    }

    public string Save(DeviceReport report)
    {
        ArgumentNullException.ThrowIfNull(report);
        using var document = JsonDocument.Parse(ReportJson.Serialize(report));
        string json = JsonSerializer.Serialize(document.RootElement,
            new JsonSerializerOptions { WriteIndented = true }) + Environment.NewLine;
        string history = Path.Combine(directory, "History",
            report.Timestamp.ToString("yyyy-MM-dd", CultureInfo.InvariantCulture));
        Directory.CreateDirectory(history);
        string name = report.DeviceId.ToString("D") + "_" +
            report.Timestamp.ToString("yyyyMMdd'T'HHmmss.fffffff'Z'", CultureInfo.InvariantCulture) + ".json";
        string path = Path.Combine(history, name);
        // Each interval has its own file; never overwrite earlier history.
        using (var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
        using (var writer = new StreamWriter(stream, new UTF8Encoding(false)))
            writer.Write(json);
        File.WriteAllText(LatestPath, json, new UTF8Encoding(false));
        return path;
    }
}
