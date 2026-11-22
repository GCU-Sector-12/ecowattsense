// SafeFile.cs: reads a whole file with a size limit and without throwing for the usual file problems.
// Who uses it: JsonFileRows, DeveloperModeFile, DiagExportPlan and DiagZipWriter.
// A missing file, a locked file, a denied file and a file over the limit all give null. The caller treats null as
// "unknown", never as "empty". The file is opened with shared read, so the service can keep writing it.
#nullable enable
using System;
using System.IO;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Small helpers for reading files that another program writes.</summary>
public static class SafeFile
{
    /// <summary>Reads a whole file.</summary>
    /// <param name="path">The full path.</param>
    /// <param name="maxBytes">The largest size that is read.</param>
    /// <returns>The bytes, or null when the file is missing, too big, locked, denied or changed while it was read.</returns>
    public static byte[]? TryReadBytes(string path, long maxBytes)
    {
        try { return ReadBytes(path, maxBytes); }
        catch (Exception exception) when (IsFileProblem(exception)) { return null; }
    }

    /// <summary>Gives the size of a file.</summary>
    /// <param name="path">The full path.</param>
    /// <returns>The size in bytes, or -1 when the file is missing or cannot be read.</returns>
    public static long Length(string path)
    {
        try { return new FileInfo(path).Length; }
        catch (Exception exception) when (IsFileProblem(exception)) { return -1; }
    }

    /// <summary>Tells whether an exception is one of the usual file problems.</summary>
    /// <param name="exception">The exception.</param>
    /// <returns>True for IO, access, bad path and unsupported path errors.</returns>
    private static bool IsFileProblem(Exception exception)
        => exception is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException;

    /// <summary>Opens the file for shared reading and reads it.</summary>
    /// <param name="path">The full path.</param>
    /// <param name="maxBytes">The largest size that is read.</param>
    /// <returns>The bytes, or null when the file is larger than the limit.</returns>
    /// <exception cref="IOException">The file is missing, locked or shrank during the read.</exception>
    private static byte[]? ReadBytes(string path, long maxBytes)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        if (stream.Length > maxBytes) return null;
        var buffer = new byte[stream.Length];
        int count = stream.ReadAtLeast(buffer, buffer.Length, throwOnEndOfStream: false);
        Array.Resize(ref buffer, count);
        return buffer;
    }
}
