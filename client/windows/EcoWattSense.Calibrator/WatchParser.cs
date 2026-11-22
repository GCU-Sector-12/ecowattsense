// WatchParser.cs: turns the text lines of "ews-scan.exe --watch" into WatchHeader and WatchSample, and checks them (spec 1.6 and 2.1).
// Who uses it: WatchSession. Every line comes from another program, so it is untrusted: at most 64 KB, lists at most 256 entries,
// numbers range-checked, unknown fields ignored, JSON nested at most 8 deep. A bad line gives null and a fixed reason word, never text of the line.
using System.Text;
using System.Text.Json;

namespace EnergyCalibrator;

/// <summary>The fixed words that say why a line was refused. They are safe for logs.</summary>
internal static class WatchReasons
{
    /// <summary>Not valid JSON or not an object.</summary>
    public const string BadJson = "bad_json";
    /// <summary>The line is longer than 64 KB.</summary>
    public const string TooLong = "too_long";
    /// <summary>The first line has no "watch" field.</summary>
    public const string NotHeader = "not_header";
    /// <summary>The "watch" field is not 1.</summary>
    public const string BadVersion = "bad_watch_version";
    /// <summary>A header after the first line.</summary>
    public const string SecondHeader = "second_header";
    /// <summary>A field is missing, of the wrong type or outside its range.</summary>
    public const string BadValue = "bad_value";
    /// <summary>A list is longer than 256 entries or does not have the length of the header list.</summary>
    public const string BadLength = "bad_length";
    /// <summary>The time does not move forward.</summary>
    public const string BadTime = "bad_time";
}

/// <summary>A line that must be refused. It carries only a fixed reason word.</summary>
/// <param name="reason">One of the WatchReasons words.</param>
internal sealed class WatchFormatException(string reason) : Exception(reason)
{
    /// <summary>The fixed reason word.</summary>
    public string Reason { get; } = reason;
}

/// <summary>Parses and checks the lines of the stream. A check that fails throws WatchFormatException inside this class; the two public methods turn it into a reason word.</summary>
internal static class WatchParser
{
    /// <summary>Largest line in bytes.</summary>
    public const int MaxLineBytes = 64 * 1024;

    /// <summary>Largest list.</summary>
    public const int MaxEntries = 256;

    private const int MaxJsonDepth = 8;
    private const int MaxNameChars = 128;
    private const int MaxVendorChars = 32;
    private const double MaxMilliwatts = 2_000_000;
    private const double MaxMillijoules = 1e15;
    private const long MaxTimeMs = 9_000_000_000_000_000;

    /// <summary>Parses the first line.</summary>
    /// <param name="line">The line.</param>
    /// <param name="reason">Why the line was refused; empty when it was accepted.</param>
    /// <returns>The header, or null.</returns>
    public static WatchHeader? ParseHeader(string line, out string reason)
    {
        reason = "";
        try { return HeaderFrom(Root(line)); }
        catch (WatchFormatException ex) { reason = ex.Reason; return null; }
    }

    /// <summary>Parses one line after the header.</summary>
    /// <param name="line">The line.</param>
    /// <param name="header">The header of this stream.</param>
    /// <param name="previousTimeMs">The time of the previous line, or -1.</param>
    /// <param name="reason">Why the line was refused; empty when it was accepted.</param>
    /// <returns>The sample, or null.</returns>
    public static WatchSample? ParseSample(string line, WatchHeader header, long previousTimeMs, out string reason)
    {
        reason = "";
        try { return SampleFrom(Root(line), header, previousTimeMs); }
        catch (WatchFormatException ex) { reason = ex.Reason; return null; }
    }

    /// <summary>Checks the size of a line and parses it. The JSON is copied out of its document, so the document is not kept.</summary>
    /// <param name="line">The line.</param>
    /// <returns>A clone of the root object.</returns>
    /// <exception cref="WatchFormatException">The line is too long, not JSON, too deep, or not an object.</exception>
    private static JsonElement Root(string line)
    {
        if (Encoding.UTF8.GetByteCount(line) > MaxLineBytes) { throw new WatchFormatException(WatchReasons.TooLong); }
        using JsonDocument document = Open(line);
        if (document.RootElement.ValueKind != JsonValueKind.Object) { throw new WatchFormatException(WatchReasons.BadJson); }
        return document.RootElement.Clone();
    }

    /// <summary>Parses the text with the depth limit.</summary>
    /// <param name="line">The line.</param>
    /// <returns>The document.</returns>
    /// <exception cref="WatchFormatException">The text is not JSON or is nested too deep.</exception>
    private static JsonDocument Open(string line)
    {
        try { return JsonDocument.Parse(line, new JsonDocumentOptions { MaxDepth = MaxJsonDepth }); }
        catch (JsonException) { throw new WatchFormatException(WatchReasons.BadJson); }
    }

    /// <summary>Builds the header from the first object.</summary>
    /// <param name="root">The object.</param>
    /// <returns>The header.</returns>
    /// <exception cref="WatchFormatException">Something is wrong with the object.</exception>
    private static WatchHeader HeaderFrom(JsonElement root)
    {
        CheckVersion(root);
        return new WatchHeader(
            RequiredInt(root, "period_ms", 500, 10_000),
            Entries(root, "emi", true).Select(ChannelFrom).ToList(),
            Entries(root, "nvml", false).Select(NvmlFrom).ToList(),
            Entries(root, "gpu_use", false).Select(GpuUseFrom).ToList(),
            MachineFrom(Section(root, "machine")),
            BatteryFrom(Section(root, "battery")),
            DisplayFrom(Section(root, "display")));
    }

    /// <summary>Checks that the object has "watch": 1.</summary>
    /// <param name="root">The object.</param>
    /// <exception cref="WatchFormatException">The field is missing (not a header) or is not 1.</exception>
    private static void CheckVersion(JsonElement root)
    {
        if (!root.TryGetProperty("watch", out JsonElement version)) { throw new WatchFormatException(WatchReasons.NotHeader); }
        bool one = version.ValueKind == JsonValueKind.Number && version.TryGetInt32(out int number) && number == 1;
        if (!one) { throw new WatchFormatException(WatchReasons.BadVersion); }
    }

    /// <summary>Builds one energy channel.</summary>
    /// <param name="entry">The object.</param>
    /// <returns>The channel.</returns>
    private static WatchChannel ChannelFrom(JsonElement entry)
    {
        string kind = Word(entry, "kind");
        return new WatchChannel(RequiredText(entry, "name", MaxNameChars), WatchKinds.Known.Contains(kind) ? kind : WatchKinds.Other, OptionalInt(entry, "device", 0, 65_535) ?? 0, OptionalInt(entry, "index", 0, 65_535) ?? 0);
    }

    /// <summary>Builds one NVML device.</summary>
    /// <param name="entry">The object.</param>
    /// <returns>The device.</returns>
    private static WatchNvml NvmlFrom(JsonElement entry)
    {
        return new WatchNvml(RequiredText(entry, "name", MaxNameChars), OptionalInt(entry, "adapter", 0, MaxEntries), Flag(entry, "power"), Flag(entry, "energy"));
    }

    /// <summary>Builds one adapter of the use list.</summary>
    /// <param name="entry">The object.</param>
    /// <returns>The adapter.</returns>
    private static WatchGpuUse GpuUseFrom(JsonElement entry)
    {
        return new WatchGpuUse(RequiredInt(entry, "adapter", 0, MaxEntries), Word(entry, "role"), OptionalInt(entry, "vendor_id", 0, 65_535) ?? 0);
    }

    /// <summary>Builds the processor fields. A field that is missing, of the wrong type or out of range is left out.</summary>
    /// <param name="section">The object, or an empty object.</param>
    /// <returns>The fields.</returns>
    private static WatchMachine MachineFrom(JsonElement section)
    {
        return new WatchMachine(Vendor(section), OptionalInt(section, "cpu_family", 0, 1023), OptionalInt(section, "cpu_model", 0, 4095), OptionalInt(section, "physical_cores", 1, 1024));
    }

    /// <summary>Builds the battery fields. A field that is missing, of the wrong type or out of range is left out.</summary>
    /// <param name="section">The object, or an empty object.</param>
    /// <returns>The fields.</returns>
    private static WatchBattery BatteryFrom(JsonElement section)
    {
        return new WatchBattery(OptionalInt(section, "design_mwh", 1, 10_000_000), OptionalInt(section, "full_mwh", 1, 10_000_000), OptionalInt(section, "cycle_count", 0, 100_000));
    }

    /// <summary>Builds the display fields. A field that is missing, of the wrong type or out of range is left out.</summary>
    /// <param name="section">The object, or an empty object.</param>
    /// <returns>The fields.</returns>
    private static WatchDisplay DisplayFrom(JsonElement section)
    {
        return new WatchDisplay(OptionalInt(section, "displays", 0, 64), OptionalInt(section, "external_displays", 0, 64), OptionalNumber(section, "refresh_hz", 1, 1000));
    }

    /// <summary>Builds the sample of one line.</summary>
    /// <param name="root">The object.</param>
    /// <param name="header">The header of the stream.</param>
    /// <param name="previousTimeMs">The time of the previous line, or -1.</param>
    /// <returns>The sample.</returns>
    /// <exception cref="WatchFormatException">Something is wrong with the line.</exception>
    private static WatchSample SampleFrom(JsonElement root, WatchHeader header, long previousTimeMs)
    {
        if (root.TryGetProperty("watch", out _)) { throw new WatchFormatException(WatchReasons.SecondHeader); }
        long time = RequiredLong(root, "t_ms", 0, MaxTimeMs);
        if (time <= previousTimeMs) { throw new WatchFormatException(WatchReasons.BadTime); }
        bool[] energy = header.Nvml.Select(device => device.Energy).ToArray();
        return new WatchSample(time, Whole(root, "emi_uj", header.Emi.Count), Numbers(root, "nvml_mw", header.Nvml.Count, MaxMilliwatts, null), Numbers(root, "nvml_mj", header.Nvml.Count, MaxMillijoules, energy), Numbers(root, "gpu_pct", header.GpuUse.Count, 100, null));
    }

    /// <summary>Reads a list of whole non-negative numbers or nulls.</summary>
    /// <param name="root">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="length">The length the list must have.</param>
    /// <returns>The values.</returns>
    private static List<long?> Whole(JsonElement root, string name, int length)
    {
        return List(root, name, length).Select(WholeOrNull).ToList();
    }

    /// <summary>Reads a list of numbers or nulls.</summary>
    /// <param name="root">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="length">The length the list must have.</param>
    /// <param name="max">The largest value.</param>
    /// <param name="allowed">For each entry, whether a value is allowed (false forces unknown); null allows all.</param>
    /// <returns>The values.</returns>
    private static List<double?> Numbers(JsonElement root, string name, int length, double max, bool[]? allowed)
    {
        List<JsonElement> items = List(root, name, length);
        List<double?> values = items.Select(item => NumberOrNull(item, max)).ToList();
        return allowed == null ? values : Mask(values, allowed);
    }

    /// <summary>Makes the entries unknown where a value is not allowed.</summary>
    /// <param name="values">The values.</param>
    /// <param name="allowed">For each entry, whether its value is kept.</param>
    /// <returns>The values with the not allowed ones set to null.</returns>
    private static List<double?> Mask(List<double?> values, bool[] allowed)
    {
        List<double?> masked = new();
        for (int i = 0; i < values.Count; i++) { masked.Add(allowed[i] ? values[i] : null); }
        return masked;
    }

    /// <summary>Takes the list of a field and checks its length.</summary>
    /// <param name="root">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="length">The length the list must have.</param>
    /// <returns>The entries.</returns>
    /// <exception cref="WatchFormatException">The field is missing or not a list (bad value), or the length is wrong (bad length).</exception>
    private static List<JsonElement> List(JsonElement root, string name, int length)
    {
        if (!root.TryGetProperty(name, out JsonElement list) || list.ValueKind != JsonValueKind.Array) { throw new WatchFormatException(WatchReasons.BadValue); }
        if (list.GetArrayLength() > MaxEntries || list.GetArrayLength() != length) { throw new WatchFormatException(WatchReasons.BadLength); }
        return list.EnumerateArray().ToList();
    }

    /// <summary>Reads a whole number that is not negative, or null.</summary>
    /// <param name="item">The value.</param>
    /// <returns>The number, or null for a JSON null.</returns>
    /// <exception cref="WatchFormatException">The value is neither null nor a whole non-negative number.</exception>
    private static long? WholeOrNull(JsonElement item)
    {
        if (item.ValueKind == JsonValueKind.Null) { return null; }
        if (item.ValueKind != JsonValueKind.Number || !item.TryGetInt64(out long number) || number < 0) { throw new WatchFormatException(WatchReasons.BadValue); }
        return number;
    }

    /// <summary>Reads a number between 0 and a maximum, or null.</summary>
    /// <param name="item">The value.</param>
    /// <param name="max">The largest value.</param>
    /// <returns>The number, or null for a JSON null.</returns>
    /// <exception cref="WatchFormatException">The value is neither null nor a number in range.</exception>
    private static double? NumberOrNull(JsonElement item, double max)
    {
        if (item.ValueKind == JsonValueKind.Null) { return null; }
        if (item.ValueKind != JsonValueKind.Number || !item.TryGetDouble(out double number) || double.IsNaN(number) || number < 0 || number > max) { throw new WatchFormatException(WatchReasons.BadValue); }
        return number;
    }

    /// <summary>Takes the entries of a list field of the header.</summary>
    /// <param name="root">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="required">True when the field must be there.</param>
    /// <returns>The entries, which are all objects; empty when an optional field is missing.</returns>
    /// <exception cref="WatchFormatException">The field is of the wrong type or too long, an entry is not an object, or a required field is missing.</exception>
    private static List<JsonElement> Entries(JsonElement root, string name, bool required)
    {
        if (!root.TryGetProperty(name, out JsonElement list)) { return required ? throw new WatchFormatException(WatchReasons.BadValue) : []; }
        if (list.ValueKind != JsonValueKind.Array) { throw new WatchFormatException(WatchReasons.BadValue); }
        if (list.GetArrayLength() > MaxEntries) { throw new WatchFormatException(WatchReasons.BadLength); }
        List<JsonElement> entries = list.EnumerateArray().ToList();
        if (entries.Any(IsNotObject)) { throw new WatchFormatException(WatchReasons.BadValue); }
        return entries;
    }

    /// <summary>Tells whether a value is not a JSON object.</summary>
    /// <param name="value">The value.</param>
    /// <returns>True when it is not an object.</returns>
    private static bool IsNotObject(JsonElement value) => value.ValueKind != JsonValueKind.Object;

    /// <summary>Takes an optional object field.</summary>
    /// <param name="root">The object.</param>
    /// <param name="name">The field name.</param>
    /// <returns>The object, or an empty object when the field is missing or not an object.</returns>
    private static JsonElement Section(JsonElement root, string name)
    {
        bool found = root.TryGetProperty(name, out JsonElement section) && section.ValueKind == JsonValueKind.Object;
        return found ? section : EmptyObject;
    }

    private static readonly JsonElement EmptyObject = JsonDocument.Parse("{}").RootElement.Clone();

    /// <summary>Reads a required whole number in a range.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="min">Smallest value.</param>
    /// <param name="max">Largest value.</param>
    /// <returns>The number.</returns>
    /// <exception cref="WatchFormatException">The field is missing, not a whole number or out of range.</exception>
    private static int RequiredInt(JsonElement obj, string name, int min, int max)
    {
        return OptionalInt(obj, name, min, max) ?? throw new WatchFormatException(WatchReasons.BadValue);
    }

    /// <summary>Reads a required whole number in a range.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="min">Smallest value.</param>
    /// <param name="max">Largest value.</param>
    /// <returns>The number.</returns>
    /// <exception cref="WatchFormatException">The field is missing, not a whole number or out of range.</exception>
    private static long RequiredLong(JsonElement obj, string name, long min, long max)
    {
        bool ok = obj.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.Number && value.TryGetInt64(out long number) && number >= min && number <= max;
        if (!ok) { throw new WatchFormatException(WatchReasons.BadValue); }
        return value.GetInt64();
    }

    /// <summary>Reads an optional whole number in a range.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="min">Smallest value.</param>
    /// <param name="max">Largest value.</param>
    /// <returns>The number, or null when the field is missing, of the wrong type or out of range.</returns>
    private static int? OptionalInt(JsonElement obj, string name, int min, int max)
    {
        bool ok = obj.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out int number) && number >= min && number <= max;
        return ok ? value.GetInt32() : null;
    }

    /// <summary>Reads an optional number in a range.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="min">Smallest value.</param>
    /// <param name="max">Largest value.</param>
    /// <returns>The number, or null when the field is missing, of the wrong type or out of range.</returns>
    private static double? OptionalNumber(JsonElement obj, string name, double min, double max)
    {
        bool ok = obj.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.Number && value.TryGetDouble(out double number) && number >= min && number <= max;
        return ok ? value.GetDouble() : null;
    }

    /// <summary>Reads a true or false field.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <returns>True only when the field is the JSON value true.</returns>
    private static bool Flag(JsonElement obj, string name) => obj.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.True;

    /// <summary>Reads a required text and cleans it.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <param name="maxChars">The most characters to keep.</param>
    /// <returns>The cleaned text.</returns>
    /// <exception cref="WatchFormatException">The field is missing or not a text.</exception>
    private static string RequiredText(JsonElement obj, string name, int maxChars)
    {
        bool ok = obj.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.String;
        if (!ok) { throw new WatchFormatException(WatchReasons.BadValue); }
        return Clean(value.GetString() ?? "", maxChars, IsPrintable);
    }

    /// <summary>Reads an optional text that is a word of lower-case letters and underscores.</summary>
    /// <param name="obj">The object.</param>
    /// <param name="name">The field name.</param>
    /// <returns>The word; empty when the field is missing or not a text.</returns>
    private static string Word(JsonElement obj, string name)
    {
        bool ok = obj.TryGetProperty(name, out JsonElement value) && value.ValueKind == JsonValueKind.String;
        return ok ? Clean(value.GetString() ?? "", 32, IsWordChar) : "";
    }

    /// <summary>Reads the vendor text of the processor: letters, digits, space, dot, underscore and minus only.</summary>
    /// <param name="section">The machine object.</param>
    /// <returns>The vendor, or null when it is missing, not a text or empty after cleaning.</returns>
    private static string? Vendor(JsonElement section)
    {
        bool ok = section.TryGetProperty("cpu_vendor", out JsonElement value) && value.ValueKind == JsonValueKind.String;
        string text = ok ? Clean(value.GetString() ?? "", MaxVendorChars, IsVendorChar) : "";
        return text.Length == 0 ? null : text;
    }

    /// <summary>Tells whether a character is printable (not a control character).</summary>
    /// <param name="c">The character.</param>
    /// <returns>True when it may stay in a name.</returns>
    private static bool IsPrintable(char c) => !char.IsControl(c);

    /// <summary>Tells whether a character is a lower-case letter or an underscore.</summary>
    /// <param name="c">The character.</param>
    /// <returns>True when it may stay in a word.</returns>
    private static bool IsWordChar(char c) => char.IsAsciiLetterLower(c) || c == '_';

    /// <summary>Tells whether a character may stay in a vendor text.</summary>
    /// <param name="c">The character.</param>
    /// <returns>True for letters, digits, space, dot, underscore and minus.</returns>
    private static bool IsVendorChar(char c) => char.IsAsciiLetterOrDigit(c) || c == ' ' || c == '.' || c == '_' || c == '-';

    /// <summary>Keeps the characters that a rule accepts and cuts the text.</summary>
    /// <param name="text">The text.</param>
    /// <param name="maxChars">The most characters to keep.</param>
    /// <param name="keep">Tells whether a character stays.</param>
    /// <returns>The cleaned text.</returns>
    private static string Clean(string text, int maxChars, Func<char, bool> keep)
    {
        string kept = new(text.Where(keep).ToArray());
        return kept.Length > maxChars ? kept[..maxChars] : kept;
    }
}
