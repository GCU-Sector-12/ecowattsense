// LogTailReader.cs: reads the last 300 lines of <data>\logs\service.log for the Log tab (spec D2, section 2.2).
// Who uses it: DiagSnapshotLoader and the tests.
// The service caps this file at 1 MB, so only the last 1 MB is read. When the read starts inside the file, the first
// (cut) line is dropped. Lines are cleaned (no control characters) and cut at 2000 characters; a line over 64 KB is
// skipped and counted. A missing or unreadable file gives Available = false ("unknown"); an empty file gives no lines.
#nullable enable
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Reads the end of service.log.</summary>
public static class LogTailReader
{
    private const long TailBytes = 1024 * 1024;
    private const int MaxLineChars = 2000;
    private static readonly LogTailResult Unavailable = new(false, new List<string>(), 0);

    /// <summary>Reads the last lines of the log.</summary>
    /// <param name="path">The full path of service.log.</param>
    /// <returns>At most 300 lines, newest first.</returns>
    public static LogTailResult Read(string path)
    {
        try { return ReadTail(path); }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException) { return Unavailable; }
    }

    /// <summary>Opens the file for shared reading and collects the last lines.</summary>
    /// <param name="path">The full path.</param>
    /// <returns>The result. Unavailable when the file is over 64 MB.</returns>
    /// <exception cref="IOException">The file is missing or cannot be read.</exception>
    /// <exception cref="UnauthorizedAccessException">Access is denied.</exception>
    private static LogTailResult ReadTail(string path)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length > DiagLimits.MaxFileBytes) return Unavailable;
        long start = Math.Max(0, stream.Length - TailBytes);
        stream.Seek(start, SeekOrigin.Begin);
        var reader = new BoundedLineReader(stream, DiagLimits.MaxLineBytes);
        if (start > 0) reader.Next();
        var tail = new Tail();
        while (tail.Take(reader.Next())) { }
        return tail.ToResult();
    }

    /// <summary>Keeps the newest lines.</summary>
    private sealed class Tail
    {
        private readonly Queue<string> lines = new();
        private int skipped;

        /// <summary>Takes one reader result.</summary>
        /// <param name="read">The result.</param>
        /// <returns>False at the end of the file.</returns>
        public bool Take(LineRead read)
        {
            if (read.Kind == LineKind.End) return false;
            if (read.Kind == LineKind.TooLong) skipped++;
            if (read.Kind == LineKind.Line) Add(read.Text);
            return true;
        }

        /// <summary>Adds a non-blank line and drops the oldest one past the limit.</summary>
        /// <param name="text">The line.</param>
        private void Add(string text)
        {
            if (string.IsNullOrWhiteSpace(text)) return;
            lines.Enqueue(TextCleaner.Clean(text, MaxLineChars));
            if (lines.Count > DiagLimits.LogLines) lines.Dequeue();
        }

        /// <summary>Builds the result with the newest line first.</summary>
        /// <returns>The result.</returns>
        public LogTailResult ToResult() => new(true, lines.Reverse().ToList(), skipped);
    }
}
