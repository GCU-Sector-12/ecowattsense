// ScanResult.cs: reads the scanner file capabilities.json (SPEC 3.5) into the few facts Setup shows and uses.
// Who uses it: the install engine after ews-scan.exe ran, and the window to show the result.
using System.Text.Json;

namespace EcoWattSense.Setup;

/// <summary>What the scan found.</summary>
/// <param name="Level">Measurement level L1 to L4.</param>
/// <param name="Calibration">needed, not_needed or not_possible.</param>
/// <param name="DeviceType">laptop or desktop.</param>
/// <param name="SourcesFound">Number of sources with present true.</param>
/// <param name="HasBattery">True when the battery source is present.</param>
public sealed record ScanResult(string Level, string Calibration, string DeviceType, int SourcesFound, bool HasBattery)
{
    /// <summary>True when a calibration is needed and a battery exists to do it with.</summary>
    public bool CalibrationOffered => Calibration == "needed" && HasBattery;

    /// <summary>One line for the screen and nothing secret in it.</summary>
    public string Summary =>
        $"Sources found: {SourcesFound}. Measurement level: {Level}. Calibration: {Calibration.Replace('_', ' ')}.";

    /// <summary>
    /// Parses the file text. Returns null when the text is not valid JSON or lacks the level; an unknown value in
    /// another field becomes a safe default.
    /// </summary>
    public static ScanResult? Parse(string? text)
    {
        if (string.IsNullOrWhiteSpace(text)) return null;
        JsonDocument? document = TryOpen(text);
        if (document == null) return null;
        using JsonDocument opened = document;
        JsonElement root = opened.RootElement;
        string level = ReadString(root, "level", "");
        if (level.Length == 0) return null;
        JsonElement sources = FindArray(root, "sources");
        int found = sources.ValueKind == JsonValueKind.Array ? sources.EnumerateArray().Count(IsPresent) : 0;
        bool battery = sources.ValueKind == JsonValueKind.Array && sources.EnumerateArray().Any(IsPresentBattery);
        return new ScanResult(level, ReadString(root, "calibration", "not_possible"), ReadString(root, "device_type", "desktop"), found, battery);
    }

    private static JsonDocument? TryOpen(string text)
    {
        try { return JsonDocument.Parse(text); }
        catch (JsonException) { return null; }
    }

    private static string ReadString(JsonElement root, string name, string fallback)
    {
        bool found = root.ValueKind == JsonValueKind.Object && root.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.String;
        return found ? root.GetProperty(name).GetString() ?? fallback : fallback;
    }

    private static JsonElement FindArray(JsonElement root, string name)
    {
        bool found = root.ValueKind == JsonValueKind.Object && root.TryGetProperty(name, out _);
        return found ? root.GetProperty(name) : default;
    }

    private static bool IsPresent(JsonElement source)
    {
        bool found = source.ValueKind == JsonValueKind.Object && source.TryGetProperty("present", out _);
        return found && source.GetProperty("present").ValueKind == JsonValueKind.True;
    }

    private static bool IsPresentBattery(JsonElement source) => IsPresent(source) && ReadString(source, "id", "") == "battery";
}
