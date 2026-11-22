// ConnectChecks.cs: tests of the "Connect" button logic (ConnectFlow and ConnectClient) with a scripted HTTP handler.
// Nothing leaves the computer: the handler answers from memory, the browser and the clock are fakes, and the wait never
// sleeps. Run by scripts\Test-Setup.ps1 through Program.Main.
using System.Net;
using System.Net.Http;
using System.Text.Json;
using EcoWattSense.Setup;
using static SetupTests.Harness;

namespace SetupTests;

/// <summary>One answer of the scripted handler: a status and a body, or an exception to throw.</summary>
/// <param name="Status">The HTTP status.</param>
/// <param name="Body">The answer body.</param>
/// <param name="Error">The exception to throw instead of answering, or null.</param>
public sealed record ScriptedReply(int Status, string Body, Exception? Error = null);

/// <summary>One request, as the handler saw it.</summary>
/// <param name="Method">The HTTP method.</param>
/// <param name="Path">The path of the address.</param>
/// <param name="Query">The query of the address, with the question mark.</param>
/// <param name="Body">The body text, or an empty text.</param>
/// <param name="HasKey">True when the request had an X-EWS-Key header.</param>
public sealed record SeenRequest(string Method, string Path, string Query, string Body, bool HasKey);

/// <summary>A handler that answers by path from a script. The last answer of a path is repeated.</summary>
public sealed class ScriptedHandler : HttpMessageHandler
{
    private readonly Dictionary<string, Queue<ScriptedReply>> _script = [];

    /// <summary>Every request in the order it came.</summary>
    public List<SeenRequest> Seen { get; } = [];

    /// <summary>Adds an answer for a path.</summary>
    public void Add(string path, int status, string body = "") => Enqueue(path, new ScriptedReply(status, body));

    /// <summary>Adds a failure for a path.</summary>
    public void AddFailure(string path, Exception error) => Enqueue(path, new ScriptedReply(0, "", error));

    private void Enqueue(string path, ScriptedReply reply)
    {
        if (!_script.ContainsKey(path)) _script[path] = new Queue<ScriptedReply>();
        _script[path].Enqueue(reply);
    }

    /// <summary>Records the request and gives the next answer of its path.</summary>
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
    {
        string body = request.Content?.ReadAsStringAsync().GetAwaiter().GetResult() ?? "";
        Uri uri = request.RequestUri!;
        Seen.Add(new SeenRequest(request.Method.Method, uri.AbsolutePath, uri.Query, body, request.Headers.Contains("X-EWS-Key")));
        Queue<ScriptedReply> queue = _script[uri.AbsolutePath];
        ScriptedReply reply = queue.Count > 1 ? queue.Dequeue() : queue.Peek();
        if (reply.Error != null) return Task.FromException<HttpResponseMessage>(reply.Error);
        return Task.FromResult(new HttpResponseMessage((HttpStatusCode)reply.Status) { Content = new StringContent(reply.Body) });
    }
}

/// <summary>A browser that remembers the addresses and answers a fixed code.</summary>
public sealed class FakeBrowser : IBrowserOps
{
    /// <summary>The code to answer; 0 is success.</summary>
    public int Answer { get; set; }

    /// <summary>The addresses that were opened.</summary>
    public List<string> Opened { get; } = [];

    /// <summary>Records the address.</summary>
    public int Open(string url)
    {
        Opened.Add(url);
        return Answer;
    }
}

/// <summary>A clock that only moves when the test says so.</summary>
public sealed class FakeClock : TimeProvider
{
    private long _ticks;

    /// <summary>One timestamp tick is one tick of a TimeSpan.</summary>
    public override long TimestampFrequency => TimeSpan.TicksPerSecond;

    /// <summary>Moves the clock.</summary>
    public void Advance(TimeSpan span) => _ticks += span.Ticks;

    /// <summary>Gives the fake time.</summary>
    public override long GetTimestamp() => _ticks;
}

/// <summary>Everything one connect run needs: the world, the scripted server, the fakes and the record of the waits.</summary>
public sealed class ConnectRig
{
    /// <summary>A computer id that is a valid GUID.</summary>
    public const string Id = "6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12";

    /// <summary>A server address.</summary>
    public const string Server = "https://server.example.org";

    /// <summary>The secret request id of the script.</summary>
    public const string RequestId = "rq-0123456789abcdefghijk";

    /// <summary>The user code of the script.</summary>
    public const string Code = "ABCD-EFGH";

    /// <summary>The client key of the script.</summary>
    public const string Key = "CLIENT-KEY-98765";

    /// <summary>The fake computer with its log.</summary>
    public FakeWorld World { get; } = new();

    /// <summary>The server.</summary>
    public ScriptedHandler Handler { get; } = new();

    /// <summary>The browser.</summary>
    public FakeBrowser Browser { get; } = new();

    /// <summary>The clock.</summary>
    public FakeClock Clock { get; } = new();

    /// <summary>Every wait the client asked for.</summary>
    public List<TimeSpan> Delays { get; } = [];

    /// <summary>The code lines the client gave to the window.</summary>
    public List<ConnectCode> Codes { get; } = [];

    /// <summary>The token of the run; Cancel it to stop the wait.</summary>
    public CancellationTokenSource Cancel { get; } = new();

    /// <summary>The number of the wait after which the run is cancelled; 0 never.</summary>
    public int CancelAtDelay { get; set; }

    /// <summary>A C1 answer with the script values.</summary>
    public static string StartBody(int seconds = 600) =>
        $$"""{"request_id":"{{RequestId}}","user_code":"{{Code}}","expires_in_seconds":{{seconds}}}""";

    /// <summary>A C4 answer that says approved.</summary>
    public static string ApprovedBody => $$"""{"status":"approved","client_key":"{{Key}}"}""";

    /// <summary>A C4 answer that says pending.</summary>
    public const string PendingBody = """{"status":"pending"}""";

    /// <summary>Scripts a good C1 answer.</summary>
    public void GoodStart(int seconds = 600) => Handler.Add("/api/connect/requests", 201, StartBody(seconds));

    private Task Sleep(TimeSpan span, CancellationToken token)
    {
        Delays.Add(span);
        Clock.Advance(span);
        if (Delays.Count == CancelAtDelay) Cancel.Cancel();
        return Task.CompletedTask;
    }

    /// <summary>The text of the install log.</summary>
    public string Log => Kit.LogText(World);

    /// <summary>Runs one connect.</summary>
    public ConnectOutcome Run(string idText = Id + "\n", string address = Server, string name = "Lab 1")
    {
        SetupLog log = new(World.Services, Kit.Layout);
        ConnectClient client = new(Handler, Browser, log, Sleep, Clock);
        ConnectRequest request = new(address, idText, name, "laptop");
        return client.RunAsync(request, Codes.Add, Cancel.Token).GetAwaiter().GetResult();
    }
}

/// <summary>The checks of the Connect button.</summary>
public static class ConnectChecks
{
    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        BodyChecks();
        StartAnswerChecks();
        ClaimAnswerChecks();
        StatusChecks();
        ScheduleChecks();
        AddressChecks();
        FlowChecks();
        FailureChecks();
        WaitChecks();
        SecretChecks();
    }

    private static JsonElement Parse(string json) => JsonDocument.Parse(json).RootElement;

    private static void BodyChecks()
    {
        string body = ConnectFlow.BuildStartBody(ConnectRig.Id, "Lab 1 PC", "laptop");
        Check(body == $$"""{"device_id":"{{ConnectRig.Id}}","device_name":"Lab 1 PC","os":"windows","device_type":"laptop"}""", "connect body: the four fields in the order of the contract");
        JsonElement empty = Parse(ConnectFlow.BuildStartBody(ConnectRig.Id, "", "desktop"));
        Check(empty.GetProperty("device_name").ValueKind == JsonValueKind.Null, "connect body: an empty name is null");
        JsonElement blanks = Parse(ConnectFlow.BuildStartBody(ConnectRig.Id, "   ", "desktop"));
        Check(blanks.GetProperty("device_name").ValueKind == JsonValueKind.Null, "connect body: a name of spaces is null");
        JsonElement trimmed = Parse(ConnectFlow.BuildStartBody(ConnectRig.Id, "  Lab 1  ", "desktop"));
        Check(trimmed.GetProperty("device_name").GetString() == "Lab 1", "connect body: the name is trimmed");
        string quoted = "He said \"hi\" \\ and left";
        JsonElement withQuotes = Parse(ConnectFlow.BuildStartBody(ConnectRig.Id, quoted, "laptop"));
        Check(withQuotes.GetProperty("device_name").GetString() == quoted, "connect body: quotes and a backslash survive");
        string polish = "Komputer Zażółć gęślą jaźń";
        JsonElement withPolish = Parse(ConnectFlow.BuildStartBody(ConnectRig.Id, polish, "laptop"));
        Check(withPolish.GetProperty("device_name").GetString() == polish, "connect body: non-ASCII letters survive");
        Check(ConnectFlow.BuildClaimBody(ConnectRig.RequestId) == $$"""{"request_id":"{{ConnectRig.RequestId}}"}""", "claim body: only the request id");
        Check(ConnectFlow.ParseDeviceId(ConnectRig.Id.ToUpperInvariant() + "\r\n") == ConnectRig.Id, "device id: read from the file text, lower case");
        Check(ConnectFlow.ParseDeviceId("") == null && ConnectFlow.ParseDeviceId(null) == null && ConnectFlow.ParseDeviceId("not a guid") == null, "device id: a missing or bad file gives null");
        Check(ConnectFlow.ParseDeviceId("00000000-0000-0000-0000-000000000000") == null, "device id: the empty GUID is not an id");
    }

    private static string StartJson(string id, string code, string seconds) =>
        $$"""{"request_id":"{{id}}","user_code":"{{code}}","expires_in_seconds":{{seconds}}}""";

    private static bool StartRejected(string json) => ConnectFlow.ParseStart(json) == null;

    private static void StartAnswerChecks()
    {
        ConnectStart? good = ConnectFlow.ParseStart(ConnectRig.StartBody());
        Check(good == new ConnectStart(ConnectRig.RequestId, ConnectRig.Code, 600), "connect start: a good answer is read");
        Check(StartRejected("<html>502</html>") && StartRejected("") && ConnectFlow.ParseStart(null) == null, "connect start: not JSON, empty and null are bad");
        Check(StartRejected("[1,2]") && StartRejected("\"text\"") && StartRejected("42"), "connect start: JSON that is not an object is bad");
        Check(StartRejected("""{"user_code":"ABCD-EFGH","expires_in_seconds":600}""") && StartRejected("""{"request_id":"rq-0123456789abcdefghijk","expires_in_seconds":600}""") && StartRejected("""{"request_id":"rq-0123456789abcdefghijk","user_code":"ABCD-EFGH"}"""), "connect start: a missing field is bad");
        Check(StartRejected("""{"request_id":12345678901234567890,"user_code":"ABCD-EFGH","expires_in_seconds":600}""") && StartRejected("""{"request_id":"rq-0123456789abcdefghijk","user_code":null,"expires_in_seconds":600}"""), "connect start: a wrong type is bad");
        Check(StartRejected(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "\"600\"")) && StartRejected(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "600.5")), "connect start: seconds must be a whole number");
        Check(ConnectFlow.ParseStart(StartJson(new string('a', 20), "ABCD-EFGH", "600")) != null && ConnectFlow.ParseStart(StartJson(new string('a', 100), "ABCD-EFGH", "600")) != null, "connect start: a request id of 20 and 100 characters is good");
        Check(StartRejected(StartJson(new string('a', 19), "ABCD-EFGH", "600")) && StartRejected(StartJson(new string('a', 101), "ABCD-EFGH", "600")), "connect start: a request id of 19 and 101 characters is bad");
        Check(ConnectFlow.ParseStart(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "1")) != null && ConnectFlow.ParseStart(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "3600")) != null, "connect start: seconds 1 and 3600 are good");
        Check(StartRejected(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "0")) && StartRejected(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "3601")) && StartRejected(StartJson("rq-0123456789abcdefghijk", "ABCD-EFGH", "-5")), "connect start: seconds 0, 3601 and below zero are bad");
        UserCodeChecks();
    }

    private static void UserCodeChecks()
    {
        string[] bad = ["abcd-efgh", "ABCDEFGH", "ABCD-EFG", "ABCD-EFGHJ", "ABCI-EFGH", "ABCD-EFGO", "ABC0-EFGH", "ABCD-EFG1", "ABC-DEFGH", "ABCD EFGH", "ABCD-EF\nH", "", "ABCD--EFG"];
        string[] rejected = bad.Where(code => ConnectFlow.ParseStart(StartJson("rq-0123456789abcdefghijk", code, "600")) != null).ToArray();
        Check(rejected.Length == 0, "connect start: a user code must be 8 allowed letters with one hyphen, bad ones left: " + string.Join(",", rejected));
        Check(ConnectFlow.ParseStart(StartJson("rq-0123456789abcdefghijk", "23456789".Insert(4, "-"), "600")) != null, "connect start: the digits 2 to 9 are allowed");
    }

    private static void ClaimAnswerChecks()
    {
        Check(ConnectFlow.ParseClaim(ConnectRig.PendingBody) == new ClaimAnswer(ClaimKind.Pending), "connect claim: pending is read");
        Check(ConnectFlow.ParseClaim(ConnectRig.ApprovedBody) == new ClaimAnswer(ClaimKind.Approved, ConnectRig.Key), "connect claim: approved gives the key");
        Check(ConnectFlow.ParseClaim("not json").Kind == ClaimKind.Bad && ConnectFlow.ParseClaim("").Kind == ClaimKind.Bad && ConnectFlow.ParseClaim(null).Kind == ClaimKind.Bad, "connect claim: not JSON is bad");
        Check(ConnectFlow.ParseClaim("[]").Kind == ClaimKind.Bad && ConnectFlow.ParseClaim("{}").Kind == ClaimKind.Bad, "connect claim: no status is bad");
        Check(ConnectFlow.ParseClaim("""{"status":"denied"}""").Kind == ClaimKind.Bad && ConnectFlow.ParseClaim("""{"status":1}""").Kind == ClaimKind.Bad, "connect claim: another status or a wrong type is bad");
        Check(ConnectFlow.ParseClaim("""{"status":"approved"}""").Kind == ClaimKind.Bad, "connect claim: approved without a key is bad");
        Check(ConnectFlow.ParseClaim("""{"status":"approved","client_key":""}""").Kind == ClaimKind.Bad, "connect claim: an empty key is bad");
        Check(ConnectFlow.ParseClaim("""{"status":"approved","client_key":null}""").Kind == ClaimKind.Bad && ConnectFlow.ParseClaim("""{"status":"approved","client_key":12345}""").Kind == ClaimKind.Bad, "connect claim: a key of the wrong type is bad");
        Check(ConnectFlow.ParseClaim($$"""{"status":"approved","client_key":"{{new string('k', 200)}}"}""").Kind == ClaimKind.Approved, "connect claim: a key of 200 characters is good");
        Check(ConnectFlow.ParseClaim($$"""{"status":"approved","client_key":"{{new string('k', 201)}}"}""").Kind == ClaimKind.Bad, "connect claim: a key of 201 characters is bad");
        Check(ConnectFlow.ParseClaim("""{"status":"approved","client_key":"abc\ndef"}""").Kind == ClaimKind.Bad, "connect claim: a key with a line break is bad (it would add a line to the configure input)");
        Check(ConnectFlow.ParseClaim("""{"status":"pending","client_key":"x"}""") == new ClaimAnswer(ClaimKind.Pending), "connect claim: pending never carries a key");
    }

    private static void StatusChecks()
    {
        Check(ConnectFlow.ForStartStatus(201) == null, "connect status: C1 201 goes on");
        Check(ConnectFlow.ForStartStatus(404) == ConnectFlow.NotSupported && ConnectFlow.ForStartStatus(405) == ConnectFlow.NotSupported, "connect status: C1 404 and 405 are not supported");
        Check(ConnectFlow.NotSupported.Text == "This server does not support Connect. Type the key.", "connect status: the not supported line");
        Check(ConnectFlow.ForStartStatus(429) == ConnectFlow.TooMany && ConnectFlow.TooMany.Text == "Too many requests. Try again in a few minutes.", "connect status: C1 429 is too many");
        Check(ConnectFlow.ForStartStatus(503) == ConnectFlow.NoKey && ConnectFlow.NoKey.Text == "The server has no client key set.", "connect status: C1 503 is no client key");
        int[] others = [0, 200, 301, 400, 401, 422, 500, 502];
        Check(others.All(status => ConnectFlow.ForStartStatus(status) == ConnectFlow.NoAnswer), "connect status: any other C1 status is no answer");
        Check(ConnectFlow.NoAnswer.Text == "The server did not answer.", "connect status: the no answer line");
        Check(ConnectFlow.ForClaimStatus(200) == null, "connect status: C4 200 goes on");
        Check(ConnectFlow.ForClaimStatus(404) == ConnectFlow.Expired && ConnectFlow.Expired.Text == "The code has expired. Click Connect again.", "connect status: C4 404 is expired");
        int[] claimOthers = [0, 201, 401, 429, 500, 503];
        Check(claimOthers.All(status => ConnectFlow.ForClaimStatus(status) == ConnectFlow.NoAnswer), "connect status: any other C4 status is no answer");
        Check(ConnectFlow.Connected.Text == "Connected." && ConnectFlow.Cancelled.Text == "", "connect status: the connected and cancelled lines");
    }

    private static void ScheduleChecks()
    {
        Check(ConnectFlow.NextDelay(TimeSpan.Zero) == TimeSpan.FromSeconds(2), "connect schedule: 2 seconds at the start");
        Check(ConnectFlow.NextDelay(TimeSpan.FromSeconds(29.9)) == TimeSpan.FromSeconds(2), "connect schedule: still 2 seconds just before 30 seconds");
        Check(ConnectFlow.NextDelay(TimeSpan.FromSeconds(30)) == TimeSpan.FromSeconds(5), "connect schedule: 5 seconds from 30 seconds on");
        Check(ConnectFlow.NextDelay(TimeSpan.FromSeconds(500)) == TimeSpan.FromSeconds(5), "connect schedule: 5 seconds late in the lifetime");
    }

    private static void AddressChecks()
    {
        Uri server = new("https://server.example.org/");
        Check(ConnectFlow.PageAddress(server, "ABCD-EFGH") == "https://server.example.org/connect?code=ABCD-EFGH", "connect address: the page with the code");
        Check(ConnectFlow.PageAddress(new Uri("https://server.example.org:8443/some/path?x=1"), "ABCD-EFGH") == "https://server.example.org:8443/connect?code=ABCD-EFGH", "connect address: port kept, path and query dropped");
        Check(ConnectFlow.PageAddress(server, "A B&C=D#E") == "https://server.example.org/connect?code=A%20B%26C%3DD%23E", "connect address: the code is encoded");
        Check(ConnectFlow.PageAddress(new Uri("http://127.0.0.1:8000"), "ABCD-EFGH") == "http://127.0.0.1:8000/connect?code=ABCD-EFGH", "connect address: loopback http");
        Check(ConnectFlow.ManualAddress(server) == "https://server.example.org/connect", "connect address: the page without the code");
        Check(ConnectFlow.CodeLine("ABCD-EFGH") == "Code: ABCD-EFGH", "connect text: the code line");
        Check(ConnectFlow.HintLine(server) == "Approve this computer in the dashboard. If the browser did not open, open https://server.example.org/connect and type the code.", "connect text: the hint line");
    }

    private static void FlowChecks()
    {
        ConnectRig rig = new();
        rig.GoodStart();
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.PendingBody);
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.PendingBody);
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.ApprovedBody);
        ConnectOutcome outcome = rig.Run();
        Check(outcome.Result == ConnectFlow.Connected && outcome.Key == ConnectRig.Key, "connect flow: approved gives Connected and the key");
        Check(rig.Handler.Seen.Count == 4 && rig.Handler.Seen[0].Path == "/api/connect/requests" && rig.Handler.Seen.Skip(1).All(seen => seen.Path == "/api/connect/claim"), "connect flow: one C1 and then C4 until approved");
        Check(rig.Handler.Seen.All(seen => seen.Method == "POST" && seen.Query == "" && !seen.HasKey), "connect flow: POST only, no key header, nothing in the query");
        JsonElement first = Parse(rig.Handler.Seen[0].Body);
        Check(first.GetProperty("device_id").GetString() == ConnectRig.Id && first.GetProperty("device_name").GetString() == "Lab 1" && first.GetProperty("os").GetString() == "windows" && first.GetProperty("device_type").GetString() == "laptop", "connect flow: C1 carries id, name, os and device type");
        Check(rig.Handler.Seen[1].Body == $$"""{"request_id":"{{ConnectRig.RequestId}}"}""", "connect flow: C4 carries the request id in the body");
        Check(rig.Codes.Count == 1 && rig.Codes[0].CodeLine == "Code: ABCD-EFGH" && rig.Codes[0].HintLine.Contains("https://server.example.org/connect and type the code."), "connect flow: the window gets the code line and the hint once");
        Check(rig.Browser.Opened.SequenceEqual(["https://server.example.org/connect?code=ABCD-EFGH"]), "connect flow: the browser opens the page with the code once");
        Check(rig.Delays.Count == 3 && rig.Delays.All(delay => delay == TimeSpan.FromSeconds(2)), "connect flow: a wait of 2 seconds before each C4");
        Check(rig.Log.Contains("connect_start started 0") && rig.Log.Contains("connect_result connected 0"), "connect flow: the log has the start and the result word");
        BrowserChecks();
    }

    private static void BrowserChecks()
    {
        ConnectRig rig = new();
        rig.Browser.Answer = -1;
        rig.GoodStart();
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.ApprovedBody);
        ConnectOutcome outcome = rig.Run();
        Check(outcome.Result == ConnectFlow.Connected && rig.Browser.Opened.Count == 1, "connect flow: a browser that cannot start is not an error");
    }

    private static ConnectOutcome StartFails(int status, string body = "")
    {
        ConnectRig rig = new();
        rig.Handler.Add("/api/connect/requests", status, body);
        ConnectOutcome outcome = rig.Run();
        Check(rig.Handler.Seen.Count == 1 && rig.Browser.Opened.Count == 0 && rig.Codes.Count == 0, "connect failure: after a bad C1 there is no C4, no browser and no code (status " + status + ")");
        return outcome;
    }

    private static void FailureChecks()
    {
        Check(StartFails(404).Result == ConnectFlow.NotSupported && StartFails(405).Result == ConnectFlow.NotSupported, "connect failure: C1 404 and 405 give the not supported line");
        Check(StartFails(429).Result == ConnectFlow.TooMany && StartFails(503).Result == ConnectFlow.NoKey, "connect failure: C1 429 and 503 give their lines");
        Check(StartFails(500).Result == ConnectFlow.NoAnswer && StartFails(422).Result == ConnectFlow.NoAnswer, "connect failure: C1 500 and 422 give no answer");
        Check(StartFails(201, "<html>").Result == ConnectFlow.NoAnswer && StartFails(201, "{}").Result == ConnectFlow.NoAnswer, "connect failure: a C1 201 with a bad body gives no answer");
        Check(StartFails(200, ConnectRig.StartBody()).Result == ConnectFlow.NoAnswer, "connect failure: only 201 is a good C1 answer");
        NetworkChecks();
        ClaimFailureChecks();
        InputChecks();
    }

    private static void NetworkChecks()
    {
        ConnectRig down = new();
        down.Handler.AddFailure("/api/connect/requests", new HttpRequestException(HttpRequestError.ConnectionError, "x"));
        Check(down.Run().Result == ConnectFlow.NoAnswer, "connect failure: C1 network error gives no answer");
        ConnectRig slow = new();
        slow.Handler.AddFailure("/api/connect/requests", new TaskCanceledException());
        Check(slow.Run().Result == ConnectFlow.NoAnswer, "connect failure: a C1 time-out gives no answer, not a cancel");
        ConnectRig dropped = new();
        dropped.GoodStart();
        dropped.Handler.AddFailure("/api/connect/claim", new HttpRequestException(HttpRequestError.ConnectionError, "x"));
        ConnectOutcome outcome = dropped.Run();
        Check(outcome.Result == ConnectFlow.NoAnswer && outcome.Key == "", "connect failure: C4 network error gives no answer");
    }

    private static ConnectOutcome ClaimAnswers(int status, string body)
    {
        ConnectRig rig = new();
        rig.GoodStart();
        rig.Handler.Add("/api/connect/claim", status, body);
        return rig.Run();
    }

    private static void ClaimFailureChecks()
    {
        Check(ClaimAnswers(404, """{"detail":"Connect request not found"}""").Result == ConnectFlow.Expired, "connect failure: C4 404 gives the expired line");
        Check(ClaimAnswers(500, "").Result == ConnectFlow.NoAnswer && ClaimAnswers(429, "").Result == ConnectFlow.NoAnswer, "connect failure: C4 500 and 429 give no answer");
        Check(ClaimAnswers(200, "<html>").Result == ConnectFlow.NoAnswer && ClaimAnswers(200, "{}").Result == ConnectFlow.NoAnswer, "connect failure: a C4 200 that is not understood gives no answer");
        ConnectOutcome emptyKey = ClaimAnswers(200, """{"status":"approved","client_key":""}""");
        Check(emptyKey.Result == ConnectFlow.NoAnswer && emptyKey.Key == "", "connect failure: an approved answer with an empty key gives no answer and no key");
        ConnectRig tooBig = new();
        tooBig.GoodStart();
        tooBig.Handler.Add("/api/connect/claim", 200, "{\"status\":\"pending\",\"pad\":\"" + new string('x', 70000) + "\"}");
        Check(tooBig.Run().Result == ConnectFlow.NoAnswer, "connect failure: an answer over 64 KB is cut and not understood");
    }

    private static void InputChecks()
    {
        ConnectRig noId = new();
        Check(noId.Run(idText: "").Result == ConnectFlow.NoId && noId.Handler.Seen.Count == 0, "connect input: no device id means no request");
        Check(ConnectFlow.NoId.Text == "This computer has no id yet.", "connect input: the no id line is the one of the name sync");
        ConnectRig http = new();
        Check(http.Run(address: "http://server.example.org").Result == ConnectFlow.BadAddress && http.Handler.Seen.Count == 0, "connect input: http to a remote host sends nothing");
        Check(ConnectFlow.BadAddress.Text == "Use an https address.", "connect input: the address line is the one of the connection test");
        ConnectRig spaces = new();
        spaces.GoodStart();
        spaces.Handler.Add("/api/connect/claim", 200, ConnectRig.ApprovedBody);
        Check(spaces.Run(address: "  https://server.example.org/  ").Result == ConnectFlow.Connected, "connect input: the address is trimmed and a trailing slash is fine");
        ConnectRig empty = new();
        empty.GoodStart();
        empty.Handler.Add("/api/connect/claim", 200, ConnectRig.ApprovedBody);
        empty.Run(name: "");
        Check(Parse(empty.Handler.Seen[0].Body).GetProperty("device_name").ValueKind == JsonValueKind.Null, "connect input: an empty name goes as null");
    }

    private static void WaitChecks()
    {
        ConnectRig forever = new();
        forever.GoodStart(60);
        forever.Handler.Add("/api/connect/claim", 200, ConnectRig.PendingBody);
        ConnectOutcome outcome = forever.Run();
        Check(outcome.Result == ConnectFlow.Expired && outcome.Key == "", "connect wait: pending for the whole lifetime gives the expired line");
        TimeSpan[] expected = [.. Enumerable.Repeat(TimeSpan.FromSeconds(2), 15), .. Enumerable.Repeat(TimeSpan.FromSeconds(5), 6)];
        Check(forever.Delays.SequenceEqual(expected), "connect wait: 2 seconds for the first 30 seconds, then 5 seconds, until the lifetime is over");
        Check(forever.Handler.Seen.Count == 1 + expected.Length, "connect wait: one C4 for each wait and none after the lifetime");
        Check(forever.Log.Contains("connect_result expired 0"), "connect wait: the log has the expired word");
        CancelChecks();
    }

    private static void CancelChecks()
    {
        ConnectRig rig = new() { CancelAtDelay = 2 };
        rig.GoodStart();
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.PendingBody);
        ConnectOutcome outcome = rig.Run();
        Check(outcome.Result == ConnectFlow.Cancelled && outcome.Key == "", "connect cancel: the result is Cancelled and has no key");
        Check(rig.Handler.Seen.Count == 2 && rig.Delays.Count == 2, "connect cancel: no C4 is sent after the cancel");
        Check(rig.Log.Contains("connect_result cancelled 0"), "connect cancel: the log has the cancelled word");
        ConnectRig early = new() { CancelAtDelay = 1 };
        early.GoodStart();
        early.Handler.Add("/api/connect/claim", 200, ConnectRig.ApprovedBody);
        Check(early.Run().Result == ConnectFlow.Cancelled && early.Handler.Seen.Count == 1, "connect cancel: a cancel during the first wait sends no C4 and drops no key");
        ConnectRig before = new();
        before.Cancel.Cancel();
        before.GoodStart();
        Check(before.Run().Result == ConnectFlow.Cancelled, "connect cancel: a token that is already cancelled gives Cancelled, not a failure line");
    }

    private static void SecretChecks()
    {
        ConnectRig rig = new();
        rig.GoodStart();
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.PendingBody);
        rig.Handler.Add("/api/connect/claim", 200, ConnectRig.ApprovedBody);
        ConnectOutcome outcome = rig.Run();
        string log = rig.Log;
        Check(!log.Contains(ConnectRig.Code) && !log.Contains(ConnectRig.RequestId) && !log.Contains(ConnectRig.Key), "connect log: no code, no request id, no key");
        Check(!log.Contains("server.example.org") && !log.Contains(ConnectRig.Id) && !log.Contains("Lab 1"), "connect log: no address, no device id, no name");
        string[] words = Kit.LogWords(rig.World).ToArray();
        Check(words.SequenceEqual(["connect_start started", "connect_result connected"]), "connect log: exactly the start and the result");
        Check(!outcome.Result.Text.Contains(ConnectRig.Key) && !outcome.Result.Text.Contains("server.example.org"), "connect result: the line has no key and no address");
        bool urlsClean = rig.Handler.Seen.All(seen => !seen.Path.Contains(ConnectRig.RequestId) && !seen.Query.Contains(ConnectRig.RequestId));
        Check(urlsClean, "connect request: the request id is never in an address");
    }
}
