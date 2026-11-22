// DiagFileName.cs: tells whether a file name is the name of a diagnostics file: diag-yyyy-MM-dd.jsonl.
// Who uses it: DiagFileReader (which files to read) and DiagExportPlan (which files go into the ZIP).
// The date in the name is UTC. A name with another shape is ignored, so a stray file in logs\ is never read as data.
#nullable enable
using System;
using System.Globalization;
using System.IO;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Checks the names of the diagnostics files.</summary>
public static class DiagFileName
{
    private const string Prefix = "diag-";
    private const string Suffix = ".jsonl";
    private const string DateFormat = "yyyy-MM-dd";

    /// <summary>Tells whether a path is a diagnostics file.</summary>
    /// <param name="path">The file path.</param>
    /// <returns>True when the file name is diag-yyyy-MM-dd.jsonl with a real date.</returns>
    public static bool IsDiagFile(string path)
    {
        string name = Path.GetFileName(path);
        bool shape = name.Length == Prefix.Length + DateFormat.Length + Suffix.Length
            && name.StartsWith(Prefix, StringComparison.Ordinal)
            && name.EndsWith(Suffix, StringComparison.Ordinal);
        return shape && DateTime.TryParseExact(name.Substring(Prefix.Length, DateFormat.Length), DateFormat,
            CultureInfo.InvariantCulture, DateTimeStyles.None, out _);
    }
}
