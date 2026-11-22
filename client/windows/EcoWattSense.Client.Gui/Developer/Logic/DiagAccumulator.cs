// DiagAccumulator.cs: sorts the lines of the diag files into the three row lists and counts the lines that failed.
// Who uses it: DiagFileReader only.
// Rules: a blank line is ignored. A line that is too long, or ends with a line feed and is not a valid diagnostics line,
// is skipped and counted. A last line with no line feed that does not parse is a line the service is still writing, so
// it is ignored and not counted. A "tick" older than 24 hours is dropped at once, so memory stays small. A list that
// grows past twice the row limit loses its oldest rows.
#nullable enable
using System;
using System.Collections.Generic;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Collects the rows of the diag files.</summary>
public sealed class DiagAccumulator
{
    private readonly DiagReadResult result;
    private readonly DateTimeOffset readingsFrom;

    /// <summary>Creates an accumulator.</summary>
    /// <param name="result">Where the rows and counts go.</param>
    /// <param name="readingsFrom">The oldest time a "tick" may have to be kept.</param>
    public DiagAccumulator(DiagReadResult result, DateTimeOffset readingsFrom)
    {
        this.result = result;
        this.readingsFrom = readingsFrom;
    }

    /// <summary>Takes one result of the line reader.</summary>
    /// <param name="read">What the reader returned.</param>
    /// <returns>False at the end of the file, true while there is more.</returns>
    public bool Take(LineRead read)
    {
        if (read.Kind == LineKind.End) return false;
        if (read.Kind == LineKind.TooLong) CountSkipped();
        if (read.Kind == LineKind.Line) TakeText(read);
        return true;
    }

    /// <summary>Counts one line, or one file, that could not be read.</summary>
    public void CountSkipped()
    {
        result.SkippedLines++;
        result.TotalLines++;
    }

    /// <summary>Parses a line and sorts it.</summary>
    /// <param name="read">A line result.</param>
    private void TakeText(LineRead read)
    {
        if (string.IsNullOrWhiteSpace(read.Text)) return;
        DiagRow? row = DiagLineParser.Parse(read.Text);
        if (row is null)
        {
            CountBadLine(read.Terminated);
            return;
        }
        result.TotalLines++;
        Route(row);
    }

    /// <summary>Counts a bad line, unless it is the unfinished last line.</summary>
    /// <param name="terminated">True when the line ended with a line feed.</param>
    private void CountBadLine(bool terminated)
    {
        if (terminated) CountSkipped();
    }

    /// <summary>Puts a row into the list of its kind. Other kinds are valid but not shown.</summary>
    /// <param name="row">The row.</param>
    private void Route(DiagRow row)
    {
        switch (row.Kind)
        {
            case "tick": AddTick(row); break;
            case "report": Append(result.Reports, row); break;
            case "send":
            case "event": Append(result.SendsEvents, row); break;
        }
    }

    /// <summary>Keeps a tick only when it is inside the last 24 hours.</summary>
    /// <param name="row">The tick row.</param>
    private void AddTick(DiagRow row)
    {
        if (row.Time >= readingsFrom) Append(result.Readings, row);
    }

    /// <summary>Adds a row and trims the oldest rows when the list is more than twice the limit.</summary>
    /// <param name="list">The list.</param>
    /// <param name="row">The row.</param>
    private static void Append(List<DiagRow> list, DiagRow row)
    {
        list.Add(row);
        if (list.Count > 2 * DiagLimits.MaxRows) list.RemoveRange(0, list.Count - DiagLimits.MaxRows);
    }
}
