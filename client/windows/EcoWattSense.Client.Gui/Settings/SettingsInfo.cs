// SettingsInfo.cs: the settings of the service that the Settings page shows (SPEC 5.6 and 5.7).
// Who uses it: the Settings page (MainWindow.Settings.cs) and Test-SettingsInfo.ps1.
// The service writes these fields into status.json. The file is untrusted: every field is type-checked and
// range-checked here. A missing field (an older service) or a field of the wrong type is null, which the page
// shows as "unknown". The key itself is never in status.json; only the bool "server_key_set" is.
#nullable enable
using System;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Settings;

/// <summary>The checked settings fields of status.json. A null value means "unknown".</summary>
/// <param name="ServiceVersion">The version text of the service, or null.</param>
/// <param name="ServerUrl">The server address; an empty string means "not set"; null means unknown.</param>
/// <param name="KeySet">True when the service holds a server key; null when unknown.</param>
/// <param name="ServerEnabled">True when sending is on; null when unknown.</param>
/// <param name="DeviceType">"laptop" or "desktop", or null.</param>
/// <param name="IntervalSeconds">The report interval in seconds, or null.</param>
/// <param name="IdleSeconds">The idle threshold in seconds, or null.</param>
/// <param name="UnusedSeconds">The unused threshold in seconds, or null.</param>
public sealed record SettingsInfo(string? ServiceVersion, string? ServerUrl, bool? KeySet, bool? ServerEnabled,
    string? DeviceType, int? IntervalSeconds, int? IdleSeconds, int? UnusedSeconds)
{
    /// <summary>The value for a file that is missing, broken or has none of the fields.</summary>
    public static readonly SettingsInfo Unknown = new(null, null, null, null, null, null, null, null);
}

/// <summary>Reads the settings fields from status.json.</summary>
public static class SettingsReader
{
    /// <summary>The longest text accepted for the URL, in characters.</summary>
    public const int MaxUrlLength = 2048;

    /// <summary>The longest text accepted for the version and the device type, in characters.</summary>
    public const int MaxVersionLength = 32;

    /// <summary>The largest number of seconds accepted for an interval or threshold (seven days).</summary>
    public const int MaxSeconds = 7 * 86400;

    /// <summary>Reads the file with the same size limit and failure handling as the status reader.</summary>
    /// <param name="path">The full path of status.json.</param>
    /// <returns>The checked fields, or <see cref="SettingsInfo.Unknown"/> when the file is missing, too big or broken.</returns>
    public static SettingsInfo Read(string path)
    {
        string? text = Viewer.StatusReader.TryReadText(path);
        return text is null ? SettingsInfo.Unknown : Parse(text);
    }

    /// <summary>Checks the JSON text.</summary>
    /// <param name="text">The file content.</param>
    /// <returns>The checked fields, or Unknown when the text is not a JSON object.</returns>
    public static SettingsInfo Parse(string text)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(text);
            return FromRoot(document.RootElement);
        }
        catch (JsonException)
        {
            return SettingsInfo.Unknown;
        }
    }

    /// <summary>Builds the fields from the root element.</summary>
    /// <param name="root">The root JSON value.</param>
    /// <returns>The fields, or Unknown when the root is not an object.</returns>
    private static SettingsInfo FromRoot(JsonElement root)
    {
        if (root.ValueKind != JsonValueKind.Object) return SettingsInfo.Unknown;
        return new SettingsInfo(
            GetText(root, "service_version", MaxVersionLength),
            GetText(root, "server_url", MaxUrlLength),
            GetBool(root, "server_key_set"),
            GetBool(root, "server_enabled"),
            Known(GetText(root, "device_type", MaxVersionLength)),
            GetSeconds(root, "interval_seconds"),
            GetSeconds(root, "idle_threshold_seconds"),
            GetSeconds(root, "unused_threshold_seconds"));
    }

    /// <summary>Reads a string property with a length limit.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <param name="maxLength">The longest accepted text.</param>
    /// <returns>The text, or null when the property is missing, not a string or too long. An empty string is kept.</returns>
    private static string? GetText(JsonElement root, string name, int maxLength)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        string? text = value.ValueKind == JsonValueKind.String ? value.GetString() : null;
        return text is not null && text.Length <= maxLength ? text : null;
    }

    /// <summary>Reads a boolean property.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>True or false, or null when the property is missing or not a JSON boolean.</returns>
    private static bool? GetBool(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        return value.ValueKind switch
        {
            JsonValueKind.True => true,
            JsonValueKind.False => false,
            _ => null
        };
    }

    /// <summary>Reads a whole number of seconds.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The seconds, or null when the property is missing, not a whole number, below 1 or above <see cref="MaxSeconds"/>.</returns>
    private static int? GetSeconds(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        if (value.ValueKind != JsonValueKind.Number) return null;
        bool whole = value.TryGetInt32(out int seconds);
        return whole && seconds >= 1 && seconds <= MaxSeconds ? seconds : null;
    }

    /// <summary>Accepts only the two device types the service knows.</summary>
    /// <param name="word">The text from the file, or null.</param>
    /// <returns>The word when it is "laptop" or "desktop", otherwise null.</returns>
    private static string? Known(string? word) => word is "laptop" or "desktop" ? word : null;
}

/// <summary>Builds the texts of the Settings page.</summary>
public static class SettingsText
{
    /// <summary>The text of a value that is not known.</summary>
    public const string Unknown = "unknown";

    /// <summary>Gives the server address line.</summary>
    /// <param name="url">The URL from <see cref="SettingsInfo.ServerUrl"/>.</param>
    /// <returns>The URL, "Not set" for an empty address, or "unknown" for null.</returns>
    public static string Address(string? url) => url switch
    {
        null => Unknown,
        "" => "Not set",
        _ => url
    };

    /// <summary>Gives the key line. It never shows more than whether a key exists.</summary>
    /// <param name="keySet">True when a key is stored, false when not, null when unknown.</param>
    /// <returns>"Server key is set", "Server key is not set" or "unknown".</returns>
    public static string Key(bool? keySet) => keySet switch
    {
        true => "Server key is set",
        false => "Server key is not set",
        _ => Unknown
    };

    /// <summary>Gives the sending line.</summary>
    /// <param name="enabled">True when sending is on, false when off, null when unknown.</param>
    /// <returns>"On", "Off" or "unknown".</returns>
    public static string Sending(bool? enabled) => enabled switch
    {
        true => "On",
        false => "Off",
        _ => Unknown
    };

    /// <summary>Gives the device type line.</summary>
    /// <param name="deviceType">"laptop", "desktop" or null.</param>
    /// <returns>"Laptop", "Desktop" or "unknown".</returns>
    public static string Device(string? deviceType) => deviceType switch
    {
        "laptop" => "Laptop",
        "desktop" => "Desktop",
        _ => Unknown
    };

    /// <summary>Gives a number of seconds as minutes.</summary>
    /// <param name="seconds">The seconds, or null when unknown.</param>
    /// <returns>For example "5 min" or "2.5 min"; "unknown" for null.</returns>
    public static string Minutes(int? seconds)
        => seconds is { } value
            ? (value / 60.0).ToString("0.##", System.Globalization.CultureInfo.InvariantCulture) + " min"
            : Unknown;

    /// <summary>Gives the service version line.</summary>
    /// <param name="version">The version text, or null.</param>
    /// <returns>The version, or "unknown" for null or an empty text.</returns>
    public static string Version(string? version) => string.IsNullOrEmpty(version) ? Unknown : version;
}
