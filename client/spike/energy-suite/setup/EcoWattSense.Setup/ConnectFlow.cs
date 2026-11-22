// ConnectFlow.cs: the pure rules of the "Connect" button of the Server page (SPEC section 10, Connect).
// Who uses it: ConnectClient sends the requests and uses these rules to build the bodies, to read the answers and to choose
// the result line; the window only shows the lines. No window code and no network code is here, so the tests run on any system.
// The answers of the server are untrusted text: every field is checked for its type and its length, and a bad answer is
// never repaired. The code, the request id and the key are never logged.
using System.Text.Json;

namespace EcoWattSense.Setup;

/// <summary>What C1 answered: the secret request id, the code to show and the lifetime.</summary>
/// <param name="RequestId">The secret id that only Setup keeps (20 to 100 characters).</param>
/// <param name="UserCode">The code the admin sees, for example ABCD-EFGH.</param>
/// <param name="ExpiresInSeconds">How long the request lives (1 to 3600).</param>
public sealed record ConnectStart(string RequestId, string UserCode, int ExpiresInSeconds);

/// <summary>The kinds of C4 answer.</summary>
public enum ClaimKind
{
    /// <summary>The answer is not understood.</summary>
    Bad,
    /// <summary>The admin has not approved yet.</summary>
    Pending,
    /// <summary>The admin approved and the answer holds the key.</summary>
    Approved,
}

/// <summary>A C4 answer.</summary>
/// <param name="Kind">The kind.</param>
/// <param name="Key">The client key, only for Approved.</param>
public sealed record ClaimAnswer(ClaimKind Kind, string Key = "");

/// <summary>A result of Connect: a fixed log word and the line for the window.</summary>
/// <param name="Word">The fixed word for install.log.</param>
/// <param name="Text">The line under the button; empty clears the line.</param>
public sealed record ConnectResult(string Word, string Text);

/// <summary>The two lines the window shows once the code is known.</summary>
/// <param name="CodeLine">For example "Code: ABCD-EFGH".</param>
/// <param name="HintLine">What to do in the dashboard.</param>
public sealed record ConnectCode(string CodeLine, string HintLine);

/// <summary>The rules of Connect.</summary>
public static class ConnectFlow
{
    /// <summary>The letters and digits of a user code (no I, O, 0, 1).</summary>
    public const string CodeAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

    /// <summary>The longest client key accepted, in characters.</summary>
    public const int MaxKeyLength = 200;

    /// <summary>The first part of the wait: a question every 2 seconds.</summary>
    public static readonly TimeSpan FastPhase = TimeSpan.FromSeconds(30);

    /// <summary>The time between questions in the first 30 seconds.</summary>
    public static readonly TimeSpan FastDelay = TimeSpan.FromSeconds(2);

    /// <summary>The time between questions after the first 30 seconds.</summary>
    public static readonly TimeSpan SlowDelay = TimeSpan.FromSeconds(5);

    /// <summary>The key was received.</summary>
    public static readonly ConnectResult Connected = new("connected", "Connected.");

    /// <summary>No answer, a time-out or an answer that is not understood.</summary>
    public static readonly ConnectResult NoAnswer = new("no_answer", "The server did not answer.");

    /// <summary>The server has no Connect (C1 404 or 405).</summary>
    public static readonly ConnectResult NotSupported = new("not_supported", "This server does not support Connect. Type the key.");

    /// <summary>The server holds too many requests (C1 429).</summary>
    public static readonly ConnectResult TooMany = new("too_many", "Too many requests. Try again in a few minutes.");

    /// <summary>The server has no client key (C1 503).</summary>
    public static readonly ConnectResult NoKey = new("no_key", "The server has no client key set.");

    /// <summary>The code is not valid any more (C4 404 or the lifetime is over).</summary>
    public static readonly ConnectResult Expired = new("expired", "The code has expired. Click Connect again.");

    /// <summary>The admin cancelled, left the page or changed the address; the line is cleared.</summary>
    public static readonly ConnectResult Cancelled = new("cancelled", "");

    /// <summary>The computer has no device id file yet. The text is the one of the computer name step.</summary>
    public static readonly ConnectResult NoId = new("no_id", "This computer has no id yet.");

    /// <summary>The address is not https and not loopback. The text is the one of the connection test.</summary>
    public static readonly ConnectResult BadAddress = new("scheme", "Use an https address.");

    /// <summary>The JSON body of C1, in the order of the contract.</summary>
    /// <param name="deviceId">The device id in "D" format.</param>
    /// <param name="name">The computer name as on the page; it is trimmed and an empty name becomes null.</param>
    /// <param name="deviceType">laptop or desktop.</param>
    public static string BuildStartBody(string deviceId, string name, string deviceType)
    {
        string clean = DeviceNameRules.Clean(name);
        Dictionary<string, string?> fields = new()
        {
            ["device_id"] = deviceId,
            ["device_name"] = clean.Length == 0 ? null : clean,
            ["os"] = "windows",
            ["device_type"] = deviceType,
        };
        return JsonSerializer.Serialize(fields);
    }

    /// <summary>The JSON body of C4. The secret goes in the body, never in the address.</summary>
    /// <param name="requestId">The request id from C1.</param>
    public static string BuildClaimBody(string requestId) =>
        JsonSerializer.Serialize(new Dictionary<string, string> { ["request_id"] = requestId });

    /// <summary>Reads the device id from the text of device-id.txt.</summary>
    /// <param name="fileText">The file text, or null when the file does not exist.</param>
    /// <returns>The id in lower case "D" format, or null when the text is not one GUID (or the empty GUID).</returns>
    public static string? ParseDeviceId(string? fileText)
    {
        bool known = Guid.TryParse((fileText ?? "").Trim(), out Guid id);
        return known && id != Guid.Empty ? id.ToString("D") : null;
    }

    /// <summary>Reads and checks a C1 answer.</summary>
    /// <param name="json">The body text.</param>
    /// <returns>The values, or null when the body is not JSON or any field is missing, of the wrong type or out of range.</returns>
    public static ConnectStart? ParseStart(string? json)
    {
        JsonElement? root = ReadRoot(json);
        if (root == null) return null;
        string? id = ReadString(root.Value, "request_id");
        string? code = ReadString(root.Value, "user_code");
        int? seconds = ReadInt(root.Value, "expires_in_seconds");
        bool good = id is { Length: >= 20 and <= 100 } && code != null && IsUserCode(code) && seconds is >= 1 and <= 3600;
        return good ? new ConnectStart(id!, code!, seconds!.Value) : null;
    }

    /// <summary>Reads and checks a C4 answer with the status 200.</summary>
    /// <param name="json">The body text.</param>
    /// <returns>Pending, Approved with the key, or Bad when anything is not as the contract says.</returns>
    public static ClaimAnswer ParseClaim(string? json)
    {
        ClaimAnswer bad = new(ClaimKind.Bad);
        JsonElement? root = ReadRoot(json);
        if (root == null) return bad;
        string? status = ReadString(root.Value, "status");
        if (status == "pending") return new ClaimAnswer(ClaimKind.Pending);
        if (status != "approved") return bad;
        string? key = ReadString(root.Value, "client_key");
        return IsKey(key) ? new ClaimAnswer(ClaimKind.Approved, key!) : bad;
    }

    /// <summary>Chooses the result for the status of C1.</summary>
    /// <param name="status">The HTTP status, or 0 when there was no answer.</param>
    /// <returns>null for 201 (go on), otherwise the result to show.</returns>
    public static ConnectResult? ForStartStatus(int status) => status switch
    {
        201 => null,
        404 or 405 => NotSupported,
        429 => TooMany,
        503 => NoKey,
        _ => NoAnswer,
    };

    /// <summary>Chooses the result for the status of C4.</summary>
    /// <param name="status">The HTTP status, or 0 when there was no answer.</param>
    /// <returns>null for 200 (read the body), otherwise the result to show.</returns>
    public static ConnectResult? ForClaimStatus(int status) => status switch
    {
        200 => null,
        404 => Expired,
        _ => NoAnswer,
    };

    /// <summary>How long to wait before the next C4.</summary>
    /// <param name="elapsed">The time since the code was received.</param>
    /// <returns>2 seconds in the first 30 seconds, then 5 seconds.</returns>
    public static TimeSpan NextDelay(TimeSpan elapsed) => elapsed < FastPhase ? FastDelay : SlowDelay;

    /// <summary>The address of the Connect page without the code, as the admin can type it.</summary>
    /// <param name="address">The server address; only scheme, host and port are used, as in every other call of Setup.</param>
    public static string ManualAddress(Uri address) => address.GetLeftPart(UriPartial.Authority) + "/connect";

    /// <summary>The address the browser opens: the Connect page with the code in the query.</summary>
    /// <param name="address">The server address.</param>
    /// <param name="code">The user code; it is encoded.</param>
    public static string PageAddress(Uri address, string code) => ManualAddress(address) + "?code=" + Uri.EscapeDataString(code);

    /// <summary>The line that shows the code.</summary>
    /// <param name="code">The user code.</param>
    public static string CodeLine(string code) => "Code: " + code;

    /// <summary>The line that says what to do in the dashboard.</summary>
    /// <param name="address">The server address.</param>
    public static string HintLine(Uri address) =>
        $"Approve this computer in the dashboard. If the browser did not open, open {ManualAddress(address)} and type the code.";

    /// <summary>Tells whether the text is a user code: 8 allowed characters with one hyphen after the fourth.</summary>
    private static bool IsUserCode(string code)
    {
        if (code.Length != 9 || code[4] != '-') return false;
        return (code[..4] + code[5..]).All(letter => CodeAlphabet.Contains(letter));
    }

    /// <summary>Tells whether the text can be a client key: not empty, at most 200 characters, no control character.</summary>
    private static bool IsKey(string? key) => key is { Length: >= 1 and <= MaxKeyLength } && !key.Any(char.IsControl);

    /// <summary>Parses the body. The element is cloned because the document is disposed before the caller reads it.</summary>
    private static JsonElement? ReadRoot(string? json)
    {
        if (string.IsNullOrEmpty(json)) return null;
        try
        {
            using JsonDocument document = JsonDocument.Parse(json);
            return document.RootElement.Clone();
        }
        catch (JsonException)
        {
            return null;
        }
    }

    /// <summary>Reads a property that must be a string; null when the object has none or it is of another type.</summary>
    private static string? ReadString(JsonElement root, string name)
    {
        if (root.ValueKind != JsonValueKind.Object) return null;
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        return value.ValueKind == JsonValueKind.String ? value.GetString() : null;
    }

    /// <summary>Reads a property that must be a whole number; null when the object has none or it is of another type.</summary>
    private static int? ReadInt(JsonElement root, string name)
    {
        if (root.ValueKind != JsonValueKind.Object) return null;
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        if (value.ValueKind != JsonValueKind.Number) return null;
        return value.TryGetInt32(out int number) ? number : null;
    }
}
