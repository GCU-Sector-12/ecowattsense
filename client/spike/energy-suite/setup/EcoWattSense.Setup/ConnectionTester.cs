// ConnectionTester.cs: the "Test connection" button of the Server page (SPEC section 10).
// Who uses it: SetupWindow calls RunAsync on a worker thread; the tests call it with a fake HTTP handler.
// What it does: GET <address>/api/settings with the header X-EWS-Key and turns the answer into one short result.
// Rules are the same as the service sends with (SPEC 5.5): HTTPS only (HTTP only for loopback), the certificate is
// always checked, no redirects, 10 second timeout. The key and the address are never written to the log: the log
// line holds only the fixed result word.
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;

namespace EcoWattSense.Setup;

/// <summary>The result of one connection test: a fixed log word, the text for the window and a code for the log.</summary>
/// <param name="Word">Fixed word for install.log, for example connected or refused.</param>
/// <param name="Text">The line shown under the button.</param>
/// <param name="Code">The HTTP status for an unexpected answer, otherwise 0.</param>
public sealed record TestResult(string Word, string Text, int Code = 0);

/// <summary>Sends the test request and maps every outcome to a TestResult.</summary>
public sealed class ConnectionTester
{
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(10);

    private readonly HttpMessageHandler _handler;
    private readonly Func<string> _storedKey;
    private readonly SetupLog _log;

    /// <summary>Creates the tester.</summary>
    /// <param name="handler">The HTTP handler. The real one never follows redirects and always checks the certificate.</param>
    /// <param name="storedKey">Gives the stored server key, or an empty text when none can be read.</param>
    /// <param name="log">The install log; it gets one line per test.</param>
    public ConnectionTester(HttpMessageHandler handler, Func<string> storedKey, SetupLog log)
    {
        _handler = handler;
        _storedKey = storedKey;
        _log = log;
    }

    /// <summary>Creates the handler for real use: no redirects, default certificate checks.</summary>
    public static HttpMessageHandler CreateRealHandler() => new SocketsHttpHandler { AllowAutoRedirect = false };

    /// <summary>Tests the connection and writes one line to the log.</summary>
    /// <param name="address">The server address as typed.</param>
    /// <param name="typedKey">The typed key. When empty, the stored key is used.</param>
    /// <returns>The result. Never throws: every failure becomes a result.</returns>
    public async Task<TestResult> RunAsync(string address, string typedKey)
    {
        string key = typedKey.Length > 0 ? typedKey : _storedKey();
        TestResult result = await Evaluate(address.Trim(), key);
        _log.Line("test_connection", result.Word, result.Code);
        return result;
    }

    /// <summary>Checks the input, then sends the request.</summary>
    private async Task<TestResult> Evaluate(string address, string key)
    {
        if (address.Length == 0 || key.Length == 0) return new TestResult("missing", "Enter the address and the key.");
        Uri? uri = ParseAddress(address);
        if (uri == null) return new TestResult("scheme", "Use an https address.");
        return await Send(uri, key);
    }

    /// <summary>Returns the address when it is https, or http to a loopback host; otherwise null.</summary>
    private static Uri? ParseAddress(string address)
    {
        bool parsed = Uri.TryCreate(address, UriKind.Absolute, out Uri? uri);
        if (!parsed || uri == null) return null;
        bool secure = uri.Scheme == Uri.UriSchemeHttps;
        bool loopbackHttp = uri.Scheme == Uri.UriSchemeHttp && IsLoopback(uri);
        return secure || loopbackHttp ? uri : null;
    }

    private static bool IsLoopback(Uri uri) => uri.IsLoopback || uri.Host.Equals("localhost", StringComparison.OrdinalIgnoreCase);

    /// <summary>Sends the request. Each failure kind is mapped to a result; a message text is never used.</summary>
    private async Task<TestResult> Send(Uri uri, string key)
    {
        try
        {
            return await SendRequest(uri, key);
        }
        catch (HttpRequestException error)
        {
            return FromError(error);
        }
        catch (Exception)
        {
            return new TestResult("unreachable", "Server not reachable.");
        }
    }

    private async Task<TestResult> SendRequest(Uri uri, string key)
    {
        using HttpClient client = new(_handler, false) { Timeout = Timeout };
        using HttpRequestMessage request = new(HttpMethod.Get, new Uri(uri, "/api/settings"));
        request.Headers.TryAddWithoutValidation("X-EWS-Key", key);
        using HttpResponseMessage response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        return FromStatus(response.StatusCode);
    }

    /// <summary>Maps an HTTP status to a result.</summary>
    private static TestResult FromStatus(HttpStatusCode status) => status switch
    {
        HttpStatusCode.OK => new TestResult("connected", "Connected."),
        HttpStatusCode.Unauthorized or HttpStatusCode.Forbidden => new TestResult("refused", "The server refused the key."),
        _ => new TestResult("other", $"Server answered {(int)status}.", (int)status),
    };

    /// <summary>Maps a failed request to a result by its error kind (DNS, TLS, other).</summary>
    private static TestResult FromError(HttpRequestException error) => error.HttpRequestError switch
    {
        HttpRequestError.NameResolutionError => new TestResult("dns", "Address not found."),
        HttpRequestError.SecureConnectionError => new TestResult("certificate", "Certificate problem."),
        _ => new TestResult("unreachable", "Server not reachable."),
    };
}

/// <summary>Reads the stored server key from config\key.bin the way the service does (DPAPI, machine scope).</summary>
public static class StoredKeyReader
{
    /// <summary>Reads and decrypts the key file. Setup runs elevated, so it may read the file.</summary>
    /// <param name="keyFile">The path of key.bin.</param>
    /// <returns>The key, or an empty text when the file is missing or cannot be decrypted (unknown, not "no key").</returns>
    public static string Read(string keyFile)
    {
        try
        {
            byte[] blob = File.ReadAllBytes(keyFile);
            byte[] plain = ProtectedData.Unprotect(blob, null, DataProtectionScope.LocalMachine);
            return Encoding.ASCII.GetString(plain);
        }
        catch (Exception)
        {
            return "";
        }
    }
}
