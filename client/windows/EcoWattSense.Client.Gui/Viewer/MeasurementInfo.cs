// MeasurementInfo.cs: the "Last measurement" line of the Service card and the new-row trigger of its dot.
// Who uses it: the viewer window (MainWindow.ServiceCard.cs) and Test-MeasurementPulse.ps1.
// It is pure: history rows in, text or a yes/no out. The window decides whether it may animate.
#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using EcoWattSense.Client.Gui.History;

namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>Texts and times taken from the last history row.</summary>
public static class MeasurementInfo
{
    /// <summary>Finds the end time of the newest row.</summary>
    /// <param name="rows">The history rows in any order.</param>
    /// <returns>The end time (UTC) of the newest row, or null when there are no rows.</returns>
    public static DateTime? LastEndUtc(IReadOnlyList<HistoryRow> rows)
        => rows.Count == 0 ? null : rows.Max(row => row.EndUtc);

    /// <summary>Gives the Last measurement line.</summary>
    /// <param name="lastEndUtc">The end time of the newest row (UTC), or null.</param>
    /// <param name="zone">The time zone for the shown time (local time in the app).</param>
    /// <returns>For example "Last measurement 10:05"; "Last measurement unknown" for null.</returns>
    public static string Line(DateTime? lastEndUtc, TimeZoneInfo zone)
    {
        if (lastEndUtc is not { } end) return "Last measurement unknown";
        DateTime local = TimeZoneInfo.ConvertTimeFromUtc(DateTime.SpecifyKind(end, DateTimeKind.Utc), zone);
        return "Last measurement " + local.ToString("HH:mm", CultureInfo.InvariantCulture);
    }
}

/// <summary>Tells when a refresh brought a new last history row.</summary>
public sealed class NewRowTracker
{
    private DateTime? seen;

    /// <summary>Records the newest row time of a refresh.</summary>
    /// <param name="lastEndUtc">The end time of the newest row, or null when there are no rows.</param>
    /// <returns>
    /// True when the time is later than the last one seen. The first time seen is only recorded (opening the window is not
    /// a new measurement). A null time changes nothing, so a failed or empty read never makes the next read look new.
    /// </returns>
    public bool Observe(DateTime? lastEndUtc)
    {
        if (lastEndUtc is not { } end) return false;
        DateTime? previous = seen;
        if (previous is null || end > previous) seen = end;
        return previous is { } before && end > before;
    }
}
