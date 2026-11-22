// DiagFileReader.cs: reads the diag-yyyy-MM-dd.jsonl files of the service from <data>\logs\ (spec D2, section 2.2).
// Who uses it: DiagSnapshotLoader (the Readings, Reports and Sends and events tabs) and the tests.
// It runs off the window thread. Failure rules (a failed or empty read is "unknown", never "nothing exists"):
//   - no logs folder: no rows and no error, because the service has not written anything yet;
//   - a folder that cannot be listed: FolderUnreadable, so the page says so;
//   - a file over 64 MB, or one that cannot be opened or read to the end: counted as one skipped line;
//   - a file that vanishes while it is read (the service deletes old days): ignored, it is gone;
//   - a file the service holds open: read with shared access;
//   - a file that grows while it is read: the rows read so far are kept and the next change event reads again.
#nullable enable
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Reads the diagnostics files.</summary>
public static class DiagFileReader
{
    /// <summary>Reads all the diagnostics files, oldest day first.</summary>
    /// <param name="logsFolder">The logs folder.</param>
    /// <param name="nowUtc">The time now, for the 24 hour limit of the "tick" lines.</param>
    /// <returns>The rows of each kind (oldest first) and the line counts.</returns>
    public static DiagReadResult Read(string logsFolder, DateTimeOffset nowUtc)
    {
        var result = new DiagReadResult();
        var accumulator = new DiagAccumulator(result, nowUtc.AddHours(-DiagLimits.ReadingHours));
        List<string>? files = ListFiles(logsFolder, result);
        if (files is null) return result;
        foreach (string file in files) ReadFile(file, accumulator);
        return result;
    }

    /// <summary>Lists the diagnostics files, oldest day first.</summary>
    /// <param name="folder">The logs folder.</param>
    /// <param name="result">Gets FolderUnreadable when the listing fails.</param>
    /// <returns>The paths. Empty when the folder is missing. Null when the folder cannot be listed.</returns>
    private static List<string>? ListFiles(string folder, DiagReadResult result)
    {
        if (!Directory.Exists(folder)) return new List<string>();
        try { return Directory.EnumerateFiles(folder, "diag-*.jsonl").Where(DiagFileName.IsDiagFile).Order(StringComparer.Ordinal).ToList(); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            result.FolderUnreadable = true;
            return null;
        }
    }

    /// <summary>Reads one file and turns a file problem into a count.</summary>
    /// <param name="path">The file.</param>
    /// <param name="accumulator">Where the lines go.</param>
    private static void ReadFile(string path, DiagAccumulator accumulator)
    {
        try { ReadLines(path, accumulator); }
        catch (FileNotFoundException) { }
        catch (DirectoryNotFoundException) { }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException) { accumulator.CountSkipped(); }
    }

    /// <summary>Opens a file for shared reading and feeds its lines to the accumulator.</summary>
    /// <param name="path">The file.</param>
    /// <param name="accumulator">Where the lines go.</param>
    /// <exception cref="IOException">The file cannot be opened or read.</exception>
    /// <exception cref="UnauthorizedAccessException">Access is denied.</exception>
    private static void ReadLines(string path, DiagAccumulator accumulator)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length > DiagLimits.MaxFileBytes)
        {
            accumulator.CountSkipped();
            return;
        }
        var reader = new BoundedLineReader(stream, DiagLimits.MaxLineBytes);
        while (accumulator.Take(reader.Next())) { }
    }
}
