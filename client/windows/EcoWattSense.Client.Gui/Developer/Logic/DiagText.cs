// DiagText.cs: the fixed texts of the Developer page that depend on counts, and the export file name.
// Who uses it: MainWindow.Developer.cs and the tests. Keeping them here lets the tests check the words without WPF.
#nullable enable
using System;
using System.Globalization;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Texts of the Developer page.</summary>
public static class DiagText
{
    /// <summary>The line shown when the export worked.</summary>
    public const string ExportSaved = "Saved.";

    /// <summary>The line shown when the export did not work.</summary>
    public const string ExportFailed = "The export failed.";

    /// <summary>The line above a Sources or Status table when the file cannot be used.</summary>
    public const string FileUnavailable = "The file is missing or could not be read.";

    /// <summary>The note under the "Developer mode" check box when the file could not be changed.</summary>
    public const string ModeNotSaved = "This setting could not be saved.";

    /// <summary>Gives the line above a diagnostics table.</summary>
    /// <param name="rows">The number of rows in the table.</param>
    /// <param name="skipped">The number of lines that could not be read.</param>
    /// <param name="noDiagnostics">True when no diagnostics line was found at all (no file, or empty files).</param>
    /// <param name="unreadable">True when the diagnostics folder could not be listed.</param>
    /// <returns>For example "5 rows", or "5 rows. 2 lines could not be read.".</returns>
    public static string CountLine(int rows, int skipped, bool noDiagnostics, bool unreadable)
    {
        if (unreadable) return "The diagnostics folder could not be read.";
        if (noDiagnostics) return "No diagnostics yet. The service writes one line each minute.";
        string text = Count(rows, "row");
        return skipped > 0 ? text + ". " + Count(skipped, "line") + " could not be read." : text;
    }

    /// <summary>Gives the line above the Sources and Status tables.</summary>
    /// <param name="available">False when the file is missing or could not be read.</param>
    /// <param name="rows">The number of rows.</param>
    /// <returns>The count, or the unavailable text.</returns>
    public static string FileLine(bool available, int rows) => available ? Count(rows, "row") : FileUnavailable;

    /// <summary>Gives the line above the Log tab.</summary>
    /// <param name="available">False when the log is missing or could not be read.</param>
    /// <param name="lines">The number of lines shown.</param>
    /// <param name="skipped">The number of lines too long to show.</param>
    /// <returns>The count, with the skipped lines when there are any.</returns>
    public static string LogLine(bool available, int lines, int skipped)
    {
        if (!available) return FileUnavailable;
        string text = Count(lines, "line");
        return skipped > 0 ? text + ". " + Count(skipped, "line") + " could not be read." : text;
    }

    /// <summary>Gives the name the Save dialog proposes.</summary>
    /// <param name="localNow">The local time now.</param>
    /// <returns>EcoWattSense-diagnostics-yyyyMMdd-HHmm.zip.</returns>
    public static string ExportFileName(DateTime localNow)
        => "EcoWattSense-diagnostics-" + localNow.ToString("yyyyMMdd-HHmm", CultureInfo.InvariantCulture) + ".zip";

    /// <summary>Writes a number and a noun, with an "s" when the number is not 1.</summary>
    /// <param name="count">The number.</param>
    /// <param name="noun">The noun, for example "row".</param>
    /// <returns>For example "1 row" or "5 rows".</returns>
    private static string Count(int count, string noun)
        => count.ToString(CultureInfo.InvariantCulture) + " " + noun + (count == 1 ? "" : "s");
}
