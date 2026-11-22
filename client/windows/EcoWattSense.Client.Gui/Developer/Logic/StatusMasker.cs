// StatusMasker.cs: removes the private fields from status.json before it goes into the diagnostics ZIP (spec D2, 2.3).
// Who uses it: DiagExportPlan and the tests.
// The fields server_url, device_name and server_device_name are dropped (any letter case). Every other field is copied
// as it is, numbers with their exact digits. A file that is not a JSON object cannot be masked safely, so the result is
// null and the file stays out of the ZIP. The unmasked text is never returned.
#nullable enable
using System;
using System.IO;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.Developer;

/// <summary>Masks status.json.</summary>
public static class StatusMasker
{
    private static readonly string[] Hidden = { "server_url", "device_name", "server_device_name" };
    private static readonly JsonDocumentOptions Options = new() { MaxDepth = 16 };

    /// <summary>Masks the file content.</summary>
    /// <param name="json">The bytes of status.json.</param>
    /// <returns>The masked JSON as UTF-8 bytes, or null when the input is not a JSON object.</returns>
    public static byte[]? Mask(byte[] json)
    {
        using JsonDocument? document = TryOpen(json);
        if (document is null || document.RootElement.ValueKind != JsonValueKind.Object) return null;
        return WriteMasked(document.RootElement);
    }

    /// <summary>Parses the bytes.</summary>
    /// <param name="json">The bytes.</param>
    /// <returns>The document, or null when the bytes are not JSON.</returns>
    private static JsonDocument? TryOpen(byte[] json)
    {
        try { return JsonDocument.Parse(json, Options); }
        catch (JsonException) { return null; }
    }

    /// <summary>Writes the object without the hidden fields.</summary>
    /// <param name="root">The top object.</param>
    /// <returns>The JSON bytes.</returns>
    private static byte[] WriteMasked(JsonElement root)
    {
        var buffer = new MemoryStream();
        using var writer = new Utf8JsonWriter(buffer);
        writer.WriteStartObject();
        foreach (JsonProperty property in root.EnumerateObject()) WriteProperty(writer, property);
        writer.WriteEndObject();
        writer.Flush();
        return buffer.ToArray();
    }

    /// <summary>Writes one property unless it is hidden.</summary>
    /// <param name="writer">The JSON writer.</param>
    /// <param name="property">The property.</param>
    private static void WriteProperty(Utf8JsonWriter writer, JsonProperty property)
    {
        if (Array.Exists(Hidden, h => h.Equals(property.Name, StringComparison.OrdinalIgnoreCase))) return;
        property.WriteTo(writer);
    }
}
