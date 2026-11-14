// StatusReader.cs: reads and checks status.json, the file the service writes for the viewer (SPEC 2.2 and 5.7).
// Who uses it: the viewer window (cards Level, Last report, Calibrated) and Test-StatusReader.ps1.
// The file lives in <data>\status\ and every user may read it, but the content is still treated as untrusted:
// the size is capped, every field is type-checked, the level and the send result must be one of the known words,
// and dates must parse. A missing, unreadable or broken file gives StatusInfo.Missing; it never throws.
#nullable enable
using System;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>The checked content of status.json.</summary>
/// <param name="Valid">False when the file is missing, unreadable or not a JSON object.</param>
/// <param name="Level">"L1" to "L4", or null when missing or not a known word.</param>
/// <param name="ModelGeneratedUtc">When the calibration model was made, or null.</param>
/// <param name="LastReportUtc">When the last report was built, or null.</param>
/// <param name="SendResult">One of the known send-result words, or null when missing or unknown.</param>
public sealed record StatusInfo(bool Valid, string? Level, DateTimeOffset? ModelGeneratedUtc,
    DateTimeOffset? LastReportUtc, string? SendResult)
{
    /// <summary>The value for a file that is missing or broken.</summary>
    public static readonly StatusInfo Missing = new(false, null, null, null, null);
}

/// <summary>Reads status.json.</summary>
public static class StatusReader
{
    /// <summary>The largest file that is read, in bytes. The real file is under 1 KB.</summary>
    public const int MaxBytes = 65536;

    private static readonly string[] Levels = { "L1", "L2", "L3", "L4" };

    private static readonly string[] SendResults =
        { "ok", "disabled", "unauthorised", "rejected", "server_error", "network", "never" };

    /// <summary>Reads and checks the file.</summary>
    /// <param name="path">The full path of status.json.</param>
    /// <returns>The checked content, or <see cref="StatusInfo.Missing"/> when the file is missing, too big, unreadable or broken.</returns>
    public static StatusInfo Read(string path)
    {
        string? text = TryReadText(path);
        return text is null ? StatusInfo.Missing : Parse(text);
    }

    /// <summary>Reads the file text with a size limit.</summary>
    /// <param name="path">The full path of status.json.</param>
    /// <returns>The text, or null when the file is missing, too big or cannot be opened. The service renames a new file over the old
    /// one, so a failed read is normal for a moment and the next change event reads again.</returns>
    internal static string? TryReadText(string path)
    {
        try { return ReadLimited(path); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    /// <summary>Opens the file for sharing and reads it when it is small enough.</summary>
    /// <param name="path">The full path of status.json.</param>
    /// <returns>The text, or null when the file is larger than <see cref="MaxBytes"/>.</returns>
    /// <exception cref="IOException">The file is missing or in use.</exception>
    /// <exception cref="UnauthorizedAccessException">The user may not read the file.</exception>
    private static string? ReadLimited(string path)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length > MaxBytes) return null;
        using var reader = new StreamReader(stream, Encoding.UTF8);
        return reader.ReadToEnd();
    }

    /// <summary>Checks the JSON text.</summary>
    /// <param name="text">The file content.</param>
    /// <returns>The checked content, or Missing when the text is not a JSON object.</returns>
    public static StatusInfo Parse(string text)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(text);
            return FromRoot(document.RootElement);
        }
        catch (JsonException)
        {
            return StatusInfo.Missing;
        }
    }

    /// <summary>Builds the checked content from the root element.</summary>
    /// <param name="root">The root JSON value.</param>
    /// <returns>The content, or Missing when the root is not an object.</returns>
    private static StatusInfo FromRoot(JsonElement root)
    {
        if (root.ValueKind != JsonValueKind.Object) return StatusInfo.Missing;
        return new StatusInfo(
            true,
            Known(GetString(root, "level"), Levels),
            GetDate(root, "model_generated_utc"),
            GetDate(root, "last_report_utc"),
            Known(GetString(root, "last_send_result"), SendResults));
    }

    /// <summary>Reads a string property.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The text, or null when the property is missing or not a string.</returns>
    private static string? GetString(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        return value.ValueKind == JsonValueKind.String ? value.GetString() : null;
    }

    /// <summary>Reads a date property.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The date in UTC, or null when the property is missing, not a string or not a date.</returns>
    private static DateTimeOffset? GetDate(JsonElement root, string name)
    {
        string? text = GetString(root, name);
        if (text is null) return null;
        bool parsed = DateTimeOffset.TryParse(text, CultureInfo.InvariantCulture,
            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal, out DateTimeOffset date);
        return parsed ? date : null;
    }

    /// <summary>Accepts a word only when it is in the list.</summary>
    /// <param name="word">The text from the file, or null.</param>
    /// <param name="allowed">The known words.</param>
    /// <returns>The word, or null when it is missing or not in the list.</returns>
    private static string? Known(string? word, string[] allowed)
        => word is not null && Array.IndexOf(allowed, word) >= 0 ? word : null;
}
