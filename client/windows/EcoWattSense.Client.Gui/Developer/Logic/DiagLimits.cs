// DiagLimits.cs: the limits of the Developer page (spec D2, section 2.2, "everything read is untrusted").
// Who uses it: the readers, the table builder and the export in this folder, and the tests.
// The service writes the files, but the client treats every byte of them as untrusted, so each size, count and depth
// that the page accepts has one named limit here.
#nullable enable

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>The limits for everything the Developer page reads.</summary>
public static class DiagLimits
{
    /// <summary>The longest line that is read, in bytes. A longer line is skipped and counted.</summary>
    public const int MaxLineBytes = 64 * 1024;

    /// <summary>The most rows in one table. The newest rows are kept.</summary>
    public const int MaxRows = 5000;

    /// <summary>The most columns in one table, and the most cells in one row.</summary>
    public const int MaxColumns = 400;

    /// <summary>The most characters in one cell. A longer text is cut and ends with "...".</summary>
    public const int MaxCellChars = 200;

    /// <summary>The deepest nesting of JSON objects and arrays. A deeper line is skipped and counted.</summary>
    public const int MaxDepth = 6;

    /// <summary>The largest file that is read, in bytes (64 MB). A larger file is not read.</summary>
    public const long MaxFileBytes = 64L * 1024 * 1024;

    /// <summary>The number of lines of service.log that the Log tab shows.</summary>
    public const int LogLines = 300;

    /// <summary>The hours of "tick" lines that the Readings tab shows.</summary>
    public const int ReadingHours = 24;
}
