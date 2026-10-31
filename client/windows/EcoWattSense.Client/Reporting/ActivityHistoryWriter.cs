#nullable enable
using System;
using System.IO;
using System.Text.Json;
using EcoWattSense.Client.Monitoring;

namespace EcoWattSense.Client.Reporting;

/// <summary>Local five-second observations. Unknown readings remain JSON null.</summary>
public sealed class ActivityHistoryWriter
{
    public string FilePath { get; }

    public ActivityHistoryWriter(string directory)
    {
        if (!Path.IsPathFullyQualified(directory))
            throw new ArgumentException("History:Directory must be an absolute directory.");
        Directory.CreateDirectory(directory);
        FilePath = Path.Combine(directory, "activity-history.json");
    }

    public void Save(Guid deviceId, string deviceType, DateTime timestamp, double? inactivity,
        double? cpuPercent, ActivityState? state, bool? locked, bool? onBattery)
    {
        if (timestamp.Kind != DateTimeKind.Utc) throw new ArgumentException("History timestamp must be UTC.");
        var observation = new
        {
            device_id = deviceId, timestamp, device_type = deviceType, os = "windows",
            idle_seconds = inactivity is { } idle && double.IsFinite(idle) && idle >= 0 ? inactivity : null,
            cpu_percent = cpuPercent is { } cpu && double.IsFinite(cpu) && cpu >= 0 && cpu <= 100 ? cpuPercent : null,
            state, state_name = state?.ToString(), screen_locked = locked, on_battery = onBattery
        };

        // A second process cannot overwrite concurrent updates. Invalid existing JSON is
        // preserved and reported, never replaced with a fresh empty history.
        using var gate = new FileStream(FilePath + ".lock", FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
        JsonDocument? previous = null;
        string temporary = FilePath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            try
            {
                using var input = File.OpenRead(FilePath);
                previous = JsonDocument.Parse(input);
                if (previous.RootElement.ValueKind != JsonValueKind.Array)
                    throw new JsonException("Activity history must be a JSON array; existing data was preserved.");
            }
            catch (FileNotFoundException) { }

            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                using (var writer = new Utf8JsonWriter(stream, new JsonWriterOptions { Indented = true }))
                {
                    writer.WriteStartArray();
                    if (previous is not null)
                        foreach (var item in previous.RootElement.EnumerateArray()) item.WriteTo(writer);
                    JsonSerializer.Serialize(writer, observation);
                    writer.WriteEndArray();
                }
                stream.Flush(true);
            }
            File.Move(temporary, FilePath, overwrite: true);
        }
        finally
        {
            previous?.Dispose();
            try { File.Delete(temporary); }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }
}
