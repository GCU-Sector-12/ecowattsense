// DeviceNameSync.cs: the "Computer name" field of the Server page (SPEC section 10).
// Who uses it: InstallEngine (after a good install) and MaintenanceEngine (after a good settings save) call Apply; the
// window uses DeviceNameRules to check and to prefill the field; the tests call it with a fake HTTP handler.
// What it does: writes the name to <data>\Client\device-name.txt (the client shows it), and when an address and a key are
// set, sends PUT <address>/api/devices/<device_id> (docs/design/api-contract.md 5.3). The transport rules are those of the
// connection test: HTTPS only (HTTP only for loopback), certificate checked, no redirects, 10 second timeout.
// A failure never fails the install. The log gets only `device_name <result word>`: no name, no address, no key.
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.Json;

namespace EcoWattSense.Setup;

/// <summary>The rules for a computer name (api-contract 5.3).</summary>
public static class DeviceNameRules
{
    /// <summary>The longest name in characters.</summary>
    public const int MaxLength = 60;

    /// <summary>Removes the spaces at both ends.</summary>
    /// <param name="typed">The text as typed.</param>
    public static string Clean(string typed) => typed.Trim();

    /// <summary>Tells whether the name is allowed: at most 60 characters and no control characters.</summary>
    /// <param name="typed">The text as typed; it is cleaned first.</param>
    public static bool IsValid(string typed)
    {
        string name = Clean(typed);
        return name.Length <= MaxLength && !name.Any(char.IsControl);
    }

    /// <summary>Gets the name from the text of device-name.txt: the first line, cleaned.</summary>
    /// <param name="fileText">The file text, or null when the file does not exist.</param>
    /// <returns>The name, or an empty text.</returns>
    public static string FromFile(string? fileText)
    {
        string first = (fileText ?? "").Split('\n')[0];
        return Clean(first);
    }
}

/// <summary>Saves the computer name and sends it to the server.</summary>
public sealed class DeviceNameSync
{
    private readonly SetupServices _services;
    private readonly SetupLayout _layout;
    private readonly HttpMessageHandler _handler;
    private readonly Func<string> _storedKey;
    private readonly SetupLog _log;

    /// <summary>Creates the sync.</summary>
    /// <param name="services">The file operations.</param>
    /// <param name="layout">The folders.</param>
    /// <param name="handler">The HTTP handler. The real one never follows redirects and always checks the certificate.</param>
    /// <param name="storedKey">Gives the stored server key, or an empty text when none can be read.</param>
    /// <param name="log">The install log.</param>
    public DeviceNameSync(SetupServices services, SetupLayout layout, HttpMessageHandler handler, Func<string> storedKey, SetupLog log)
    {
        _services = services;
        _layout = layout;
        _handler = handler;
        _storedKey = storedKey;
        _log = log;
    }

    /// <summary>The JSON body of the PUT request, in the order of the contract example.</summary>
    /// <param name="name">The cleaned name; empty gives null.</param>
    /// <param name="deviceType">laptop or desktop.</param>
    public static string BuildBody(string name, string deviceType)
    {
        Dictionary<string, string?> fields = new()
        {
            ["device_name"] = name.Length == 0 ? null : name,
            ["os"] = "windows",
            ["device_type"] = deviceType,
        };
        return JsonSerializer.Serialize(fields);
    }

    /// <summary>Saves the name to the file and sends it when an address and a key are known. Never throws.</summary>
    /// <param name="typedName">The name as typed; empty removes the name.</param>
    /// <param name="typedUrl">The typed address; empty uses the stored one.</param>
    /// <param name="typedKey">The typed key; empty uses the stored one.</param>
    /// <param name="deviceType">laptop or desktop, sent as device_type.</param>
    /// <returns>Ok with an empty Message when all is done; otherwise not Ok and one line for the person.</returns>
    public StepOutcome Apply(string typedName, string typedUrl, string typedKey, string deviceType)
    {
        if (!DeviceNameRules.IsValid(typedName)) return Finish("invalid", "The computer name was not saved: use at most 60 characters and no control characters.");
        string name = DeviceNameRules.Clean(typedName);
        int code = SaveFile(name);
        if (code != 0) return Finish("save_failed", "The computer name could not be saved.");
        string url = typedUrl.Trim().Length > 0 ? typedUrl.Trim() : IniEditor.Get(_services.Files.ReadText(_layout.IniFile), "server_url");
        string key = typedKey.Length > 0 ? typedKey : _storedKey();
        if (url.Length == 0 || key.Length == 0) return Finish("saved", "");
        TestResult result = SendAsync(url, key, name, deviceType).GetAwaiter().GetResult();
        string reason = result.Text.TrimEnd('.');
        string line = $"The computer name could not be sent: {char.ToLowerInvariant(reason[0])}{reason[1..]}.";
        return result.Word == "sent" ? Finish("sent", "") : Finish(result.Word, line, result.Code);
    }

    /// <summary>Writes the name, or removes the file for an empty name.</summary>
    private int SaveFile(string name)
    {
        if (name.Length == 0) return _services.Files.DeleteFile(_layout.DeviceNameFile);
        return _services.Files.WriteText(_layout.DeviceNameFile, name + "\n");
    }

    /// <summary>Logs the result word and gives the outcome. A line means a problem, an empty line means all done.</summary>
    private StepOutcome Finish(string word, string line, int code = 0)
    {
        _log.Line("device_name", word, code);
        return new StepOutcome(line.Length == 0, line);
    }

    /// <summary>Reads the computer id, checks the address and sends. Each failure kind becomes a result.</summary>
    private async Task<TestResult> SendAsync(string address, string key, string name, string deviceType)
    {
        bool known = Guid.TryParse((_services.Files.ReadText(_layout.DeviceIdFile) ?? "").Trim(), out Guid id);
        if (!known) return new TestResult("no_id", "This computer has no id yet.");
        Uri? uri = ConnectionTester.ParseAddress(address);
        if (uri == null) return new TestResult("scheme", "Use an https address.");
        try
        {
            return await Put(uri, key, id, BuildBody(name, deviceType));
        }
        catch (HttpRequestException error)
        {
            return ConnectionTester.FromError(error);
        }
        catch (Exception)
        {
            return new TestResult("unreachable", "Server not reachable.");
        }
    }

    private async Task<TestResult> Put(Uri uri, string key, Guid id, string body)
    {
        using HttpClient client = new(_handler, false) { Timeout = ConnectionTester.Timeout };
        using HttpRequestMessage request = new(HttpMethod.Put, new Uri(uri, "/api/devices/" + id.ToString("D")));
        request.Content = new StringContent(body, Encoding.UTF8, "application/json");
        request.Headers.TryAddWithoutValidation("X-EWS-Key", key);
        using HttpResponseMessage response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        return response.StatusCode == HttpStatusCode.OK ? new TestResult("sent", "Sent.") : ConnectionTester.FromStatus(response.StatusCode);
    }
}
