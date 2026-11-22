// DiagLineParser.cs: checks and flattens one line of a diag-*.jsonl file (spec D2, section 2.2).
// Who uses it: DiagAccumulator (every line of the files) and the tests.
// A line is valid when it is a JSON object, nested at most 6 levels, with a text "t" (the kind) and a text "ts" in the
// form 2026-11-22T10:31:00Z. Anything else is not a diagnostics line. The caller skips it and counts it.
#nullable enable
using System;
using System.Globalization;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Parses the lines of the diagnostics file.</summary>
public static class DiagLineParser
{
    private const string TimeFormat = "yyyy-MM-dd'T'HH:mm:ss'Z'";
    private const int MaxKindChars = 40;
    private static readonly JsonDocumentOptions Options = new() { MaxDepth = DiagLimits.MaxDepth };

    /// <summary>Parses one line.</summary>
    /// <param name="line">The text of one line, without the line end.</param>
    /// <returns>The row, or null when the line is not valid. It never throws for bad input.</returns>
    public static DiagRow? Parse(string line)
    {
        string text = line.TrimStart('﻿');
        if (text.Length > DiagLimits.MaxLineBytes) return null;
        using JsonDocument? document = TryOpen(text);
        return document is null ? null : ToRow(document.RootElement);
    }

    /// <summary>Parses the text as JSON.</summary>
    /// <param name="text">The line.</param>
    /// <returns>The document, or null when the text is not JSON or is nested too deep.</returns>
    private static JsonDocument? TryOpen(string text)
    {
        try { return JsonDocument.Parse(text, Options); }
        catch (JsonException) { return null; }
    }

    /// <summary>Builds the row from a parsed object.</summary>
    /// <param name="root">The top value of the line.</param>
    /// <returns>The row, or null when it is not an object with "t" and "ts" as texts and "ts" in the right form.</returns>
    private static DiagRow? ToRow(JsonElement root)
    {
        if (root.ValueKind != JsonValueKind.Object) return null;
        string? kind = TextOf(root, "t");
        string? stamp = TextOf(root, "ts");
        if (kind is null || stamp is null) return null;
        if (!TryParseTime(stamp, out DateTimeOffset time)) return null;
        return new DiagRow(TextCleaner.Clean(kind, MaxKindChars), time, JsonFlattener.Flatten(root));
    }

    /// <summary>Gives a property as text.</summary>
    /// <param name="root">The object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The text, or null when the property is missing or not a string.</returns>
    private static string? TextOf(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        return value.ValueKind == JsonValueKind.String ? value.GetString() : null;
    }

    /// <summary>Parses the UTC time of "ts".</summary>
    /// <param name="text">The text.</param>
    /// <param name="time">The time in UTC.</param>
    /// <returns>True when the text has exactly the form of the service.</returns>
    private static bool TryParseTime(string text, out DateTimeOffset time)
        => DateTimeOffset.TryParseExact(text, TimeFormat, CultureInfo.InvariantCulture,
            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal, out time);
}
