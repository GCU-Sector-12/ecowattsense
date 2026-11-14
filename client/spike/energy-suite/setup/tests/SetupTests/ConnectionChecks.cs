// ConnectionChecks.cs: tests of the "Test connection" button logic (ConnectionTester) with a fake HTTP handler.
// Nothing leaves the computer: the handler answers from memory. Run by scripts\Test-Setup.ps1 through Program.Main.
using System.Net;
using System.Net.Http;
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>A handler that answers with a status or throws, and remembers the last request.</summary>
public sealed class FakeHandler : HttpMessageHandler
{
    /// <summary>The status to answer with.</summary>
    public HttpStatusCode Status { get; set; } = HttpStatusCode.OK;

    /// <summary>The exception to throw instead of answering, or null.</summary>
    public Exception? Throw { get; set; }

    /// <summary>The last request, or null.</summary>
    public HttpRequestMessage? Last { get; private set; }

    /// <summary>The key header of the last request, or null.</summary>
    public string? LastKey { get; private set; }

    /// <summary>Records the request and answers.</summary>
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        Last = request;
        LastKey = request.Headers.TryGetValues("X-EWS-Key", out IEnumerable<string>? values) ? values.First() : null;
        if (Throw != null) return Task.FromException<HttpResponseMessage>(Throw);
        return Task.FromResult(new HttpResponseMessage(Status));
    }
}

/// <summary>The checks of the connection test.</summary>
public static class ConnectionChecks
{
    private const string Good = "https://server.example.org";

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        StatusChecks();
        ErrorChecks();
        InputChecks();
        KeyChecks();
    }

    private static TestResult Test(FakeHandler handler, string address, string typed, string stored, FakeWorld world)
    {
        ConnectionTester tester = new(handler, () => stored, new SetupLog(world.Services, Kit.Layout));
        return tester.RunAsync(address, typed).GetAwaiter().GetResult();
    }

    private static TestResult Test(FakeHandler handler, string address, string typed) => Test(handler, address, typed, "", new FakeWorld());

    private static void StatusChecks()
    {
        Check(Test(new FakeHandler(), Good, "k").Text == "Connected.", "test connection: 200 is connected");
        Check(Test(new FakeHandler { Status = HttpStatusCode.Unauthorized }, Good, "k").Text == "The server refused the key.", "test connection: 401 is refused");
        Check(Test(new FakeHandler { Status = HttpStatusCode.Forbidden }, Good, "k").Text == "The server refused the key.", "test connection: 403 is refused");
        Check(Test(new FakeHandler { Status = HttpStatusCode.InternalServerError }, Good, "k").Text == "Server answered 500.", "test connection: another status is named");
        Check(Test(new FakeHandler { Status = HttpStatusCode.Redirect }, Good, "k").Text == "Server answered 302.", "test connection: a redirect is an answer, not followed");
    }

    private static void ErrorChecks()
    {
        FakeHandler dns = new() { Throw = new HttpRequestException(HttpRequestError.NameResolutionError, "x") };
        FakeHandler tls = new() { Throw = new HttpRequestException(HttpRequestError.SecureConnectionError, "x") };
        FakeHandler down = new() { Throw = new HttpRequestException(HttpRequestError.ConnectionError, "x") };
        FakeHandler slow = new() { Throw = new TaskCanceledException() };
        Check(Test(dns, Good, "k").Text == "Address not found.", "test connection: DNS failure");
        Check(Test(tls, Good, "k").Text == "Certificate problem.", "test connection: TLS failure");
        Check(Test(down, Good, "k").Text == "Server not reachable.", "test connection: network failure");
        Check(Test(slow, Good, "k").Text == "Server not reachable.", "test connection: timeout");
    }

    private static void InputChecks()
    {
        FakeHandler handler = new();
        Check(Test(handler, "http://server.example.org", "k").Text == "Use an https address.", "test connection: http to a remote host is refused");
        Check(Test(handler, "ftp://server.example.org", "k").Text == "Use an https address." && Test(handler, "nonsense", "k").Text == "Use an https address.", "test connection: other schemes and nonsense");
        Check(handler.Last == null, "test connection: no request for a bad address");
        Check(Test(handler, "http://127.0.0.1:8000", "k").Text == "Connected." && Test(handler, "http://localhost:8000", "k").Text == "Connected.", "test connection: http is allowed for loopback");
        Check(Test(handler, "", "k").Text == "Enter the address and the key." && Test(handler, Good, "").Text == "Enter the address and the key.", "test connection: missing address or key");
        Check(Test(handler, Good + "/", "k").Text == "Connected." && handler.Last?.RequestUri?.AbsolutePath == "/api/settings", "test connection: the path is /api/settings");
    }

    private static void KeyChecks()
    {
        FakeWorld world = new();
        FakeHandler handler = new();
        Test(handler, Good, "", "stored-key", world);
        Check(handler.LastKey == "stored-key", "test connection: the stored key is used when the field is empty");
        Test(handler, Good, "typed-key", "stored-key", world);
        Check(handler.LastKey == "typed-key", "test connection: the typed key wins");
        string log = Kit.LogText(world);
        Check(log.Contains("test_connection connected 0") && !log.Contains("typed-key") && !log.Contains("stored-key") && !log.Contains("server.example.org"), "test connection: one log line, no key and no address");
    }
}
