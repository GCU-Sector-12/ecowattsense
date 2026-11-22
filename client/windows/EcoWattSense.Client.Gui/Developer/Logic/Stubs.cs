// Stubs.cs: signatures only, so the tests compile and fail first (red). Removed in the next commit.
#nullable enable
using System;
using System.Collections.Generic;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Stub.</summary>
public static class TextCleaner { /// <summary>Stub.</summary>
    public static string Clean(string text, int maxChars) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class JsonFlattener { /// <summary>Stub.</summary>
    public static List<KeyValuePair<string, string>> Flatten(JsonElement root) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagLineParser { /// <summary>Stub.</summary>
    public static DiagRow? Parse(string line) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagTableBuilder { /// <summary>Stub.</summary>
    public static DiagTable Build(IReadOnlyList<DiagRow> rows, params string[] firstColumns) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagFileReader { /// <summary>Stub.</summary>
    public static DiagReadResult Read(string logsFolder, DateTimeOffset nowUtc) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class JsonFileRows { /// <summary>Stub.</summary>
    public static JsonFileResult Read(string path) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class LogTailReader { /// <summary>Stub.</summary>
    public static LogTailResult Read(string path) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DeveloperModeFile {
    /// <summary>Stub.</summary>
    public static string DefaultPath => throw new NotImplementedException();
    /// <summary>Stub.</summary>
    public static bool IsOn(string path) => throw new NotImplementedException();
    /// <summary>Stub.</summary>
    public static bool TrySet(string path, bool on) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class StatusMasker { /// <summary>Stub.</summary>
    public static byte[]? Mask(byte[] json) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagExportPlan { /// <summary>Stub.</summary>
    public static ExportPlan Build(string dataFolder) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagZipWriter { /// <summary>Stub.</summary>
    public static ExportResult Write(ExportPlan plan, string targetPath) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagText {
    /// <summary>Stub.</summary>
    public static string CountLine(int rows, int skipped, bool noDiagnostics, bool unreadable) => throw new NotImplementedException();
    /// <summary>Stub.</summary>
    public static string ExportFileName(DateTime localNow) => throw new NotImplementedException(); }

/// <summary>Stub.</summary>
public static class DiagSnapshotLoader { /// <summary>Stub.</summary>
    public static DiagSnapshot Load(string dataFolder, DateTimeOffset nowUtc) => throw new NotImplementedException(); }
