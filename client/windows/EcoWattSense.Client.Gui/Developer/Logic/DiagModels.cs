// DiagModels.cs: the plain data types that the Developer page readers hand to the window and to the export.
// Who uses it: every file in this folder, the Developer page of the window, and the tests.
// They hold no logic, so the window can bind to them and the tests can build them directly.
#nullable enable
using System;
using System.Collections.Generic;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>One valid line of a diagnostics file after flattening.</summary>
/// <param name="Kind">The value of "t" (tick, report, send, event, ...).</param>
/// <param name="Time">The value of "ts", in UTC.</param>
/// <param name="Cells">The flattened name and value pairs, in the order of the line. Names and values are already cleaned and cut.</param>
public sealed record DiagRow(string Kind, DateTimeOffset Time, IReadOnlyList<KeyValuePair<string, string>> Cells);

/// <summary>A table ready for the grid.</summary>
/// <param name="Columns">The column names. The first columns are the ones the caller asked for.</param>
/// <param name="Rows">The rows, newest first. Every row has one text for each column ("" for a missing value).</param>
/// <param name="WidthChars">A column width in characters for each column, from the header and the first rows.</param>
public sealed record DiagTable(IReadOnlyList<string> Columns, IReadOnlyList<string[]> Rows, IReadOnlyList<int> WidthChars)
{
    /// <summary>A table with no columns and no rows.</summary>
    public static DiagTable Empty { get; } = new(Array.Empty<string>(), Array.Empty<string[]>(), Array.Empty<int>());
}

/// <summary>What the reader found in the diag-*.jsonl files.</summary>
public sealed class DiagReadResult
{
    /// <summary>The "tick" rows of the last 24 hours, oldest first.</summary>
    public List<DiagRow> Readings { get; } = new();

    /// <summary>The "report" rows, oldest first.</summary>
    public List<DiagRow> Reports { get; } = new();

    /// <summary>The "send" and "event" rows, oldest first.</summary>
    public List<DiagRow> SendsEvents { get; } = new();

    /// <summary>The number of lines (and files) that could not be read.</summary>
    public int SkippedLines { get; set; }

    /// <summary>The number of lines that were read or skipped. Zero means no diagnostics yet.</summary>
    public int TotalLines { get; set; }

    /// <summary>True when the logs folder exists but could not be listed.</summary>
    public bool FolderUnreadable { get; set; }
}

/// <summary>One "name, value" row of the Sources and Status tabs.</summary>
/// <param name="Name">The flattened name.</param>
/// <param name="Value">The cleaned value.</param>
public sealed record NameValueRow(string Name, string Value);

/// <summary>The rows of one JSON file.</summary>
/// <param name="Available">False when the file is missing, too big, unreadable or not a JSON object. That means "unknown", not "empty".</param>
/// <param name="Rows">The rows, in file order, at most 5000.</param>
public sealed record JsonFileResult(bool Available, IReadOnlyList<NameValueRow> Rows);

/// <summary>The last lines of service.log.</summary>
/// <param name="Available">False when the file is missing or cannot be read.</param>
/// <param name="Lines">At most 300 lines, newest first.</param>
/// <param name="SkippedLines">The number of lines that were too long to show.</param>
public sealed record LogTailResult(bool Available, IReadOnlyList<string> Lines, int SkippedLines);

/// <summary>One file (or one masked text) that goes into the diagnostics ZIP.</summary>
/// <param name="ZipPath">The name inside the ZIP, with "/" as the separator.</param>
/// <param name="SourcePath">The file to copy, or null when Content is given.</param>
/// <param name="Content">The text to write instead of a file (the masked status), or null.</param>
public sealed record ExportEntry(string ZipPath, string? SourcePath, byte[]? Content);

/// <summary>The list of files that go into the ZIP.</summary>
/// <param name="Entries">The files that exist now.</param>
/// <param name="MissingCount">How many of the fixed files were missing, or could not be masked, and so were left out.</param>
public sealed record ExportPlan(IReadOnlyList<ExportEntry> Entries, int MissingCount);

/// <summary>The result of writing the ZIP.</summary>
/// <param name="Saved">True when the ZIP file was written.</param>
/// <param name="Written">The number of files in the ZIP (without export-notes.txt).</param>
/// <param name="LeftOut">The number of files that were left out.</param>
public sealed record ExportResult(bool Saved, int Written, int LeftOut);

/// <summary>Everything the Developer page shows, read at one moment.</summary>
public sealed class DiagSnapshot
{
    /// <summary>The Readings table.</summary>
    public DiagTable Readings { get; init; } = DiagTable.Empty;

    /// <summary>The Reports table.</summary>
    public DiagTable Reports { get; init; } = DiagTable.Empty;

    /// <summary>The Sends and events table.</summary>
    public DiagTable SendsEvents { get; init; } = DiagTable.Empty;

    /// <summary>The line counts of the diag files.</summary>
    public DiagReadResult Counts { get; init; } = new();

    /// <summary>The rows of capabilities.service.json.</summary>
    public JsonFileResult CapabilitiesService { get; init; } = new(false, Array.Empty<NameValueRow>());

    /// <summary>The rows of capabilities.json.</summary>
    public JsonFileResult Capabilities { get; init; } = new(false, Array.Empty<NameValueRow>());

    /// <summary>The rows of status.json.</summary>
    public JsonFileResult Status { get; init; } = new(false, Array.Empty<NameValueRow>());

    /// <summary>The last lines of service.log.</summary>
    public LogTailResult Log { get; init; } = new(false, Array.Empty<string>(), 0);
}
