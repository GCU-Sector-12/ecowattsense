// DiagZipWriter.cs: writes the diagnostics ZIP from an export plan (spec D2, section 2.3).
// Who uses it: DiagExporter (the Export button) and the tests. It writes only the file the user chose.
// Failure rules:
//   - the target cannot be created (folder missing, access denied, a folder with that name): Saved is false and nothing is left behind;
//   - a source file that vanished or cannot be read after the plan was made: it is left out and counted. The file is read
//     whole into memory before its ZIP entry is made, so a failed read never leaves half an entry;
//   - an error while writing: the partly written target is deleted, Saved is false;
//   - no file at all could be added: Saved is false and the target is deleted, because a ZIP with only the notes helps nobody.
// export-notes.txt holds fixed words and the two counts, nothing else.
#nullable enable
using System.IO;
using System.IO.Compression;
using System.Text;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Writes the ZIP file.</summary>
public static class DiagZipWriter
{
    private static readonly ExportResult Failed = new(false, 0, 0);

    /// <summary>Writes the ZIP. An existing target is replaced.</summary>
    /// <param name="plan">The files to write.</param>
    /// <param name="targetPath">The full path of the ZIP file.</param>
    /// <returns>The result. It never throws for file problems.</returns>
    public static ExportResult Write(ExportPlan plan, string targetPath)
    {
        FileStream? stream = TryCreate(targetPath);
        if (stream is null) return Failed;
        ExportResult result = WriteAndClose(plan, stream);
        if (!result.Saved) TryDelete(targetPath);
        return result;
    }

    /// <summary>Creates the target file.</summary>
    /// <param name="path">The target path.</param>
    /// <returns>The open stream, or null when the file cannot be created. An existing file is then left as it was.</returns>
    private static FileStream? TryCreate(string path)
    {
        try { return new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None); }
        catch (Exception exception) when (IsFileProblem(exception)) { return null; }
    }

    /// <summary>Fills the ZIP and closes the stream.</summary>
    /// <param name="plan">The files to write.</param>
    /// <param name="stream">The target stream. It is closed here.</param>
    /// <returns>The result. Failed when a write error happened.</returns>
    private static ExportResult WriteAndClose(ExportPlan plan, FileStream stream)
    {
        try { return Fill(plan, stream); }
        catch (Exception exception) when (IsFileProblem(exception)) { return Failed; }
        finally { stream.Dispose(); }
    }

    /// <summary>Adds every entry and the notes.</summary>
    /// <param name="plan">The files to write.</param>
    /// <param name="stream">The target stream. It stays open.</param>
    /// <returns>The counts, or a failed result when no file could be added.</returns>
    /// <exception cref="IOException">A write error.</exception>
    private static ExportResult Fill(ExportPlan plan, FileStream stream)
    {
        using var zip = new ZipArchive(stream, ZipArchiveMode.Create, leaveOpen: true);
        int written = 0;
        foreach (ExportEntry entry in plan.Entries) written += TryAdd(zip, entry) ? 1 : 0;
        int leftOut = plan.MissingCount + plan.Entries.Count - written;
        if (written == 0) return Failed;
        AddText(zip, "export-notes.txt", NotesText(written, leftOut));
        return new ExportResult(true, written, leftOut);
    }

    /// <summary>Adds one entry.</summary>
    /// <param name="zip">The archive.</param>
    /// <param name="entry">The entry.</param>
    /// <returns>False when the source could not be read. It is then left out.</returns>
    private static bool TryAdd(ZipArchive zip, ExportEntry entry)
    {
        byte[]? data = entry.Content ?? SafeFile.TryReadBytes(entry.SourcePath ?? "", DiagLimits.MaxFileBytes);
        if (data is null) return false;
        AddBytes(zip, entry.ZipPath, data);
        return true;
    }

    /// <summary>Gives the text of export-notes.txt.</summary>
    /// <param name="written">The files written.</param>
    /// <param name="leftOut">The files left out.</param>
    /// <returns>Fixed words and the two counts.</returns>
    private static string NotesText(int written, int leftOut)
        => "EcoWattSense diagnostics export\nfiles_written: " + written + "\nfiles_left_out: " + leftOut + "\n";

    /// <summary>Adds a text entry in UTF-8.</summary>
    /// <param name="zip">The archive.</param>
    /// <param name="name">The name inside the ZIP.</param>
    /// <param name="text">The text.</param>
    private static void AddText(ZipArchive zip, string name, string text) => AddBytes(zip, name, new UTF8Encoding(false).GetBytes(text));

    /// <summary>Adds an entry with the given bytes.</summary>
    /// <param name="zip">The archive.</param>
    /// <param name="name">The name inside the ZIP.</param>
    /// <param name="data">The bytes.</param>
    private static void AddBytes(ZipArchive zip, string name, byte[] data)
    {
        ZipArchiveEntry entry = zip.CreateEntry(name, CompressionLevel.Optimal);
        using Stream target = entry.Open();
        target.Write(data, 0, data.Length);
    }

    /// <summary>Deletes a partly written target. A failure is ignored.</summary>
    /// <param name="path">The target path.</param>
    private static void TryDelete(string path)
    {
        try { File.Delete(path); }
        catch (Exception exception) when (IsFileProblem(exception)) { }
    }

    /// <summary>Tells whether an exception is a usual file or ZIP problem.</summary>
    /// <param name="exception">The exception.</param>
    /// <returns>True for IO, access, bad data, bad path and unsupported path errors.</returns>
    private static bool IsFileProblem(Exception exception)
        => exception is IOException or UnauthorizedAccessException or InvalidDataException or ArgumentException or NotSupportedException;
}
