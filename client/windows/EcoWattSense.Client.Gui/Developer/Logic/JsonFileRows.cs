// JsonFileRows.cs: reads one JSON file and turns it into "name, value" rows for the Sources and Status tabs (spec D2, 2.2).
// Who uses it: DiagSnapshotLoader (capabilities.service.json, capabilities.json, status.json) and the tests.
// The file is untrusted: at most 64 MB, nested at most 6 levels, at most 5000 rows, each name and value cut to 200
// characters. A file that is missing, locked, too big, broken or not an object gives Available = false. That means
// "unknown", so the page never shows an empty table as if the file held nothing.
#nullable enable
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Reads a JSON file as flat rows.</summary>
public static class JsonFileRows
{
    private static readonly JsonDocumentOptions Options = new() { MaxDepth = DiagLimits.MaxDepth };
    private static readonly JsonFileResult Unknown = new(false, new List<NameValueRow>());

    /// <summary>Reads the file.</summary>
    /// <param name="path">The full path.</param>
    /// <returns>The rows in file order, or an unavailable result when the file cannot be used.</returns>
    public static JsonFileResult Read(string path)
    {
        byte[]? bytes = SafeFile.TryReadBytes(path, DiagLimits.MaxFileBytes);
        if (bytes is null) return Unknown;
        using JsonDocument? document = TryOpen(bytes);
        if (document is null || document.RootElement.ValueKind != JsonValueKind.Object) return Unknown;
        return new JsonFileResult(true, ToRows(document.RootElement));
    }

    /// <summary>Parses the bytes as JSON.</summary>
    /// <param name="bytes">The file content.</param>
    /// <returns>The document, or null when it is not JSON, is empty or is nested too deep.</returns>
    private static JsonDocument? TryOpen(byte[] bytes)
    {
        try { return JsonDocument.Parse(bytes, Options); }
        catch (JsonException) { return null; }
    }

    /// <summary>Flattens an object into rows.</summary>
    /// <param name="root">The top object.</param>
    /// <returns>At most 5000 rows.</returns>
    private static List<NameValueRow> ToRows(JsonElement root)
        => JsonFlattener.Flatten(root, DiagLimits.MaxRows).Select(c => new NameValueRow(c.Key, c.Value)).ToList();
}
