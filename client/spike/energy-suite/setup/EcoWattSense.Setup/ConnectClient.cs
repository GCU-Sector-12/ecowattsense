// ConnectClient.cs: the "Connect" button of the Server page (SPEC section 10, Connect): the requests and the wait.
// Who uses it: SetupWindow calls RunAsync; the tests call it with a scripted HTTP handler, a fake browser, a fake delay and a
// fake clock. The rules (bodies, answers, lines) are in ConnectFlow.
// What it does: C1 asks for a code, the browser opens the Connect page, then C4 is asked until the admin approved, the code
// expired or the token was cancelled. The wait is one async loop with the delays of the contract: no timer runs when nothing
// waits and the window stays usable. HTTP rules are those of the connection test and SPEC 5.5: HTTPS only (HTTP only for
// loopback), the certificate is always checked, no redirects, 10 second timeout, the answer is read up to 64 KB.
// The log gets `connect_start` and `connect_result <word>` only: never the code, the request id, the key or the address.
using System.Net.Http;
using System.Text;

namespace EcoWattSense.Setup;

/// <summary>The input of one connect run.</summary>
/// <param name="Address">The server address as typed.</param>
/// <param name="DeviceIdText">The text of device-id.txt, or an empty text when the file is missing.</param>
/// <param name="Name">The computer name as on the page.</param>
/// <param name="DeviceType">laptop or desktop.</param>
public sealed record ConnectRequest(string Address, string DeviceIdText, string Name, string DeviceType);

/// <summary>The output of one connect run.</summary>
/// <param name="Result">The result: its word goes to the log and its text to the window.</param>
/// <param name="Key">The client key when Result is Connected, otherwise an empty text. The caller keeps it like a typed key.</param>
public sealed record ConnectOutcome(ConnectResult Result, string Key = "");

/// <summary>Asks the server for a code and waits for the admin to approve it.</summary>
public sealed class ConnectClient
{
    private const int MaxAnswerBytes = 65536;

    private readonly HttpMessageHandler _handler;
    private readonly IBrowserOps _browser;
    private readonly SetupLog _log;
    private readonly Func<TimeSpan, CancellationToken, Task> _delay;
    private readonly TimeProvider _time;

    /// <summary>The answer of one request: a status (0 when there was no answer) and the body text.</summary>
    private sealed record HttpReply(int Status, string Body);

    /// <summary>Creates the client.</summary>
    /// <param name="handler">The HTTP handler. The real one never follows redirects and always checks the certificate.</param>
    /// <param name="browser">Opens the Connect page.</param>
    /// <param name="log">The install log.</param>
    /// <param name="delay">Waits the given time; ends with an exception when the token is cancelled. Null uses Task.Delay.</param>
    /// <param name="time">The clock that measures the lifetime. Null uses the system clock.</param>
    public ConnectClient(HttpMessageHandler handler, IBrowserOps browser, SetupLog log, Func<TimeSpan, CancellationToken, Task>? delay = null, TimeProvider? time = null)
    {
        _handler = handler;
        _browser = browser;
        _log = log;
        _delay = delay ?? ((span, token) => Task.Delay(span, token));
        _time = time ?? TimeProvider.System;
    }

    /// <summary>Runs the whole flow and writes two lines to the log. Never throws: every failure becomes a result.</summary>
    /// <param name="request">The address, the computer id, the name and the device type.</param>
    /// <param name="onCode">Called once, when the code is known, with the lines to show.</param>
    /// <param name="token">Cancel it to stop the wait (Cancel button, a new address, leaving the page).</param>
    /// <returns>The outcome. The key is set only for Connected.</returns>
    public async Task<ConnectOutcome> RunAsync(ConnectRequest request, Action<ConnectCode> onCode, CancellationToken token)
    {
        _log.Line("connect_start", "started", 0);
        ConnectOutcome outcome = await Execute(request, onCode, token);
        _log.Line("connect_result", outcome.Result.Word, 0);
        return outcome;
    }

    /// <summary>Checks the input, sends C1 and goes on.</summary>
    private async Task<ConnectOutcome> Execute(ConnectRequest request, Action<ConnectCode> onCode, CancellationToken token)
    {
        string? deviceId = ConnectFlow.ParseDeviceId(request.DeviceIdText);
        if (deviceId == null) return new ConnectOutcome(ConnectFlow.NoId);
        Uri? address = ConnectionTester.ParseAddress(request.Address.Trim());
        if (address == null) return new ConnectOutcome(ConnectFlow.BadAddress);
        string body = ConnectFlow.BuildStartBody(deviceId, request.Name, request.DeviceType);
        HttpReply reply = await Post(address, "/api/connect/requests", body, token);
        return await AfterStart(address, reply, onCode, token);
    }

    /// <summary>Reads the C1 answer; with a good one shows the code, opens the browser and waits for the approval.</summary>
    private async Task<ConnectOutcome> AfterStart(Uri address, HttpReply reply, Action<ConnectCode> onCode, CancellationToken token)
    {
        if (token.IsCancellationRequested) return new ConnectOutcome(ConnectFlow.Cancelled);
        ConnectResult? failed = ConnectFlow.ForStartStatus(reply.Status);
        if (failed != null) return new ConnectOutcome(failed);
        ConnectStart? start = ConnectFlow.ParseStart(reply.Body);
        if (start == null) return new ConnectOutcome(ConnectFlow.NoAnswer);
        onCode(new ConnectCode(ConnectFlow.CodeLine(start.UserCode), ConnectFlow.HintLine(address)));
        _browser.Open(ConnectFlow.PageAddress(address, start.UserCode));
        return await WaitForApproval(address, start, token);
    }

    /// <summary>The one wait loop: each round ends the wait with an outcome or goes on.</summary>
    private async Task<ConnectOutcome> WaitForApproval(Uri address, ConnectStart start, CancellationToken token)
    {
        long begin = _time.GetTimestamp();
        TimeSpan lifetime = TimeSpan.FromSeconds(start.ExpiresInSeconds);
        ConnectOutcome? outcome = null;
        while (outcome == null) outcome = await Poll(address, start, begin, lifetime, token);
        return outcome;
    }

    /// <summary>One round: the lifetime check, the wait, one C4. Gives null while the request is pending.</summary>
    private async Task<ConnectOutcome?> Poll(Uri address, ConnectStart start, long begin, TimeSpan lifetime, CancellationToken token)
    {
        TimeSpan elapsed = _time.GetElapsedTime(begin);
        if (elapsed >= lifetime) return new ConnectOutcome(ConnectFlow.Expired);
        bool woke = await Sleep(ConnectFlow.NextDelay(elapsed), token);
        if (!woke) return new ConnectOutcome(ConnectFlow.Cancelled);
        HttpReply reply = await Post(address, "/api/connect/claim", ConnectFlow.BuildClaimBody(start.RequestId), token);
        return ReadClaim(reply, token);
    }

    /// <summary>Waits. Gives false when the token was cancelled.</summary>
    private async Task<bool> Sleep(TimeSpan span, CancellationToken token)
    {
        try
        {
            await _delay(span, token);
            return !token.IsCancellationRequested;
        }
        catch (OperationCanceledException)
        {
            return false;
        }
    }

    /// <summary>Turns a C4 answer into an outcome; null means the request is still pending.</summary>
    private static ConnectOutcome? ReadClaim(HttpReply reply, CancellationToken token)
    {
        if (token.IsCancellationRequested) return new ConnectOutcome(ConnectFlow.Cancelled);
        ConnectResult? failed = ConnectFlow.ForClaimStatus(reply.Status);
        if (failed != null) return new ConnectOutcome(failed);
        ClaimAnswer answer = ConnectFlow.ParseClaim(reply.Body);
        return answer.Kind switch
        {
            ClaimKind.Pending => null,
            ClaimKind.Approved => new ConnectOutcome(ConnectFlow.Connected, answer.Key),
            _ => new ConnectOutcome(ConnectFlow.NoAnswer),
        };
    }

    /// <summary>Sends a POST with a JSON body and no key. Any failure, a time-out and a cancel included, is a reply with status 0.</summary>
    private async Task<HttpReply> Post(Uri address, string path, string body, CancellationToken token)
    {
        try
        {
            return await SendPost(address, path, body, token);
        }
        catch (Exception)
        {
            return new HttpReply(0, "");
        }
    }

    private async Task<HttpReply> SendPost(Uri address, string path, string body, CancellationToken token)
    {
        using HttpClient client = new(_handler, false) { Timeout = ConnectionTester.Timeout };
        using HttpRequestMessage request = new(HttpMethod.Post, new Uri(address, path));
        request.Content = new StringContent(body, Encoding.UTF8, "application/json");
        using HttpResponseMessage response = await client.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, token);
        string text = await ReadLimited(response.Content, token);
        return new HttpReply((int)response.StatusCode, text);
    }

    /// <summary>Reads the answer up to 64 KB; more is ignored (and a cut JSON text is not understood).</summary>
    private static async Task<string> ReadLimited(HttpContent content, CancellationToken token)
    {
        using Stream stream = await content.ReadAsStreamAsync(token);
        byte[] buffer = new byte[MaxAnswerBytes];
        int total = await stream.ReadAtLeastAsync(buffer, buffer.Length, false, token);
        return Encoding.UTF8.GetString(buffer, 0, total);
    }
}
