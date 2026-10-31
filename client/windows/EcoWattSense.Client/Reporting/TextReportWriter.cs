using System;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace EcoWattSense.Client.Reporting;

/// <summary>A human-readable copy of the latest completed report, not an upload queue.</summary>
public sealed class TextReportWriter
{
    public string FilePath { get; }

    public TextReportWriter(string directory)
    {
        if (!Path.IsPathFullyQualified(directory))
            throw new ArgumentException("Report directory must be absolute.", nameof(directory));
        FilePath = Path.Combine(directory, "latest-report.txt");
    }

    public void Save(DeviceReport report)
    {
        ArgumentNullException.ThrowIfNull(report);
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        File.WriteAllText(FilePath, Format(report), new UTF8Encoding(false));
    }

    public static string Format(DeviceReport report)
    {
        ArgumentNullException.ThrowIfNull(report);
        using var json = JsonDocument.Parse(ReportJson.Serialize(report));
        string formattedJson = JsonSerializer.Serialize(json.RootElement, new JsonSerializerOptions { WriteIndented = true });
        return string.Join(Environment.NewLine,
            "ECOWATTSENSE - DEVICE MONITORING REPORT",
            "=====================================",
            "Latest completed observation interval", "",
            $"Device ID:             {report.DeviceId:D}",
            $"Device type:           {report.DeviceType}",
            $"Operating system:      {report.Os}",
            $"Interval end (UTC):    {report.Timestamp.ToString("yyyy-MM-dd HH:mm:ss.fffffff 'UTC'", CultureInfo.InvariantCulture)}",
            $"Interval duration:     {report.IntervalSeconds} seconds", "",
            "MEASUREMENTS",
            "------------",
            $"Activity state:        {report.State} ({(int)report.State})",
            $"Time since user input: {report.IdleSeconds} seconds",
            $"Average CPU usage:     {report.CpuPercent.ToString("F2", CultureInfo.InvariantCulture)}%",
            $"Screen locked:         {(report.ScreenLocked ? "Yes" : "No")}",
            $"Power source:          {(report.OnBattery ? "Battery" : "Mains")}", "",
            "CPU usage is weighted across the complete reporting interval.",
            "Activity state, inactivity, lock and power describe the closing observation.",
            "State at interval end is provisional, not the dominant state over the interval.",
            "This file confirms local generation, not server acceptance.",
            "Skipped intervals do not update this file. Check the timestamp for freshness.", "",
            "REPORT JSON",
            "-----------",
            formattedJson, "");
    }
}
