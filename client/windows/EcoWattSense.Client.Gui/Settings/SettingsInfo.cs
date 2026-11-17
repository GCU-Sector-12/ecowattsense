// SettingsInfo.cs: the settings of the service that the Settings page shows (SPEC 5.6 and 5.7).
// Who uses it: the Settings page (MainWindow.Settings.cs) and Test-SettingsInfo.ps1.
// The service writes these fields into status.json. The file is untrusted: every field is type-checked and
// range-checked here. A missing field (an older service) or a field of the wrong type is null, which the page
// shows as "unknown". The key itself is never in status.json; only the bool "server_key_set" is.
#nullable enable
using System;
using System.Linq;
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
/// <param name="SettingsSource">"server" or "local", or null (an older service).</param>
/// <param name="SettingsVersion">The version of the settings read from the server, or null.</param>
/// <param name="LastSettingsUtc">When the settings were last checked on the server (UTC), or null.</param>
/// <param name="LastPingUtc">When the server was last asked (UTC), or null.</param>
/// <param name="LastPingResult">The result word of the last ask, "ok" when it worked, or null.</param>
/// <param name="DeviceName">The computer name the service holds ("" means none), or null (an older service).</param>
/// <param name="ServerDeviceName">The name the server keeps, or null when none or unknown.</param>
/// <param name="NameSync">"ok", "pending" or "failed", or null (an older service).</param>
public sealed record SettingsInfo(string? ServiceVersion, string? ServerUrl, bool? KeySet, bool? ServerEnabled,
    string? DeviceType, int? IntervalSeconds, int? IdleSeconds, int? UnusedSeconds,
    string? SettingsSource = null, int? SettingsVersion = null, DateTime? LastSettingsUtc = null,
    DateTime? LastPingUtc = null, string? LastPingResult = null,
    string? DeviceName = null, string? ServerDeviceName = null, string? NameSync = null)
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

    /// <summary>The longest computer name accepted, in characters (api-contract 5.3).</summary>
    public const int MaxNameLength = 60;

    /// <summary>Reads the computer name Setup saved in device-name.txt.</summary>
    /// <param name="path">The full path of device-name.txt.</param>
    /// <returns>The name, or null when the file is missing, unreadable or holds no usable name.</returns>
    public static string? ReadComputerName(string path) => ParseComputerName(Viewer.StatusReader.TryReadText(path));

    /// <summary>Checks the text of device-name.txt. The file is untrusted: only the first line is used.</summary>
    /// <param name="text">The file text, or null.</param>
    /// <returns>The cleaned first line, or null when it is empty, longer than <see cref="MaxNameLength"/> or has control characters.</returns>
    public static string? ParseComputerName(string? text)
    {
        string name = (text ?? "").Split('\n')[0].Trim();
        bool usable = name.Length > 0 && name.Length <= MaxNameLength && !name.Any(char.IsControl);
        return usable ? name : null;
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
            GetSeconds(root, "unused_threshold_seconds"),
            GetText(root, "settings_source", MaxVersionLength),
            GetVersionNumber(root, "settings_version"),
            GetUtcTime(root, "last_settings_utc"),
            GetUtcTime(root, "last_ping_utc"),
            GetText(root, "last_ping_result", MaxVersionLength),
            GetName(root, "device_name"),
            GetName(root, "server_device_name"),
            GetNameSync(root));
    }

    /// <summary>Reads a computer name field. The file is untrusted, so the length and the characters are checked.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The name (may be empty), or null when the property is missing, JSON null, not a string, longer than <see cref="MaxNameLength"/> characters or has control characters.</returns>
    private static string? GetName(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        if (value.ValueKind != JsonValueKind.String) return null;
        string text = value.GetString() ?? "";
        bool usable = text.EnumerateRunes().Count() <= MaxNameLength && !text.EnumerateRunes().Any(System.Text.Rune.IsControl);
        return usable ? text : null;
    }

    /// <summary>Reads the name_sync word.</summary>
    /// <param name="root">The JSON object.</param>
    /// <returns>"ok", "pending" or "failed", otherwise null.</returns>
    private static string? GetNameSync(JsonElement root)
    {
        string? word = GetText(root, "name_sync", MaxVersionLength);
        return word is "ok" or "pending" or "failed" ? word : null;
    }

    /// <summary>Reads a whole, non-negative version number.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The number, or null when the property is missing, not a whole number or negative.</returns>
    private static int? GetVersionNumber(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        if (value.ValueKind != JsonValueKind.Number) return null;
        bool whole = value.TryGetInt32(out int number);
        return whole && number >= 0 ? number : null;
    }

    /// <summary>Reads a time written like 2026-11-15T21:26:42Z.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The time as UTC, or null when the property is missing, not a string or not in that form.</returns>
    private static DateTime? GetUtcTime(JsonElement root, string name)
    {
        string? text = GetText(root, name, MaxVersionLength);
        if (text is null) return null;
        const string format = "yyyy-MM-dd'T'HH:mm:ss'Z'";
        var style = System.Globalization.DateTimeStyles.AssumeUniversal | System.Globalization.DateTimeStyles.AdjustToUniversal;
        bool good = DateTime.TryParseExact(text, format, System.Globalization.CultureInfo.InvariantCulture, style, out DateTime time);
        return good ? time : null;
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

    /// <summary>Gives the computer name line.</summary>
    /// <param name="name">The checked name from <see cref="SettingsReader.ReadComputerName"/>, or null.</param>
    /// <returns>The name, or "Not set" when there is none.</returns>
    public static string ComputerName(string? name) => string.IsNullOrEmpty(name) ? "Not set" : name;

    /// <summary>Gives the service version line.</summary>
    /// <param name="version">The version text, or null.</param>
    /// <returns>The version, or "unknown" for null or an empty text.</returns>
    public static string Version(string? version) => string.IsNullOrEmpty(version) ? Unknown : version;

    /// <summary>The line under the settings that says where they come from.</summary>
    public const string LocalSettings = "Local settings (not read from the server yet)";

    /// <summary>Gives the lines that say where the settings come from (one or two lines).</summary>
    /// <param name="info">The checked fields.</param>
    /// <param name="zone">The time zone for the clock times; the computer's own zone in the page.</param>
    /// <returns>The server line, plus a second line when the last ask failed; the local line when the source is not "server" or its fields are missing or wrong.</returns>
    public static string SettingsSourceLines(SettingsInfo info, TimeZoneInfo zone)
    {
        bool fromServer = info.SettingsSource == "server" && info.SettingsVersion is not null && info.LastSettingsUtc is not null;
        if (!fromServer) return LocalSettings;
        string checkedAt = ClockTime(info.LastSettingsUtc, zone);
        string line = $"Settings from the server, version {info.SettingsVersion}, checked {checkedAt}";
        bool pingFailed = info.LastPingResult is not null && info.LastPingResult != "ok";
        return pingFailed ? line + "\n" + PingFailedLine(info.LastPingUtc, zone) : line;
    }

    /// <summary>Gives the line for a failed ask of the server.</summary>
    /// <param name="pingUtc">When the server was last asked (UTC), or null.</param>
    /// <param name="zone">The time zone for the clock time.</param>
    /// <returns>"Server not reached at the last check (HH:mm)."; without the brackets when the time is unknown.</returns>
    private static string PingFailedLine(DateTime? pingUtc, TimeZoneInfo zone)
        => pingUtc is null
            ? "Server not reached at the last check."
            : $"Server not reached at the last check ({ClockTime(pingUtc, zone)}).";

    /// <summary>Turns a UTC time into local HH:mm.</summary>
    /// <param name="utc">The UTC time, or null.</param>
    /// <param name="zone">The time zone.</param>
    /// <returns>The hours and minutes in that zone; "unknown" for null.</returns>
    private static string ClockTime(DateTime? utc, TimeZoneInfo zone)
        => utc is { } time ? TimeZoneInfo.ConvertTimeFromUtc(time, zone).ToString("HH:mm", System.Globalization.CultureInfo.InvariantCulture) : Unknown;
}
