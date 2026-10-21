$ErrorActionPreference = 'Stop'
$harness = Join-Path ([IO.Path]::GetTempPath()) ('ServerChecks-' + [guid]::NewGuid().ToString('N') + '.cs')
try {
@'
#nullable enable
using System;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using EcoWattSense.Client.Communication;
using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;

public sealed class FakeServerHandler : HttpMessageHandler
{
    public int Calls;
    public Func<HttpRequestMessage, CancellationToken, Task<HttpResponseMessage>> Reply = null!;
    protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken token)
    {
        Calls++;
        return Reply(request, token);
    }
}
public static class ServerChecks
{
    private static int checks;
    private static readonly ServerOptions Options = new(true, "https://example.invalid", "dummy-test-key");
    private static readonly DeviceReport Report = new(Guid.NewGuid(), DateTime.UtcNow, 300,
        ActivityState.Active, 12, 37.5, false, false, "laptop");
    private const string Settings = "{\"report_interval_seconds\":15,\"idle_threshold_seconds\":10,\"unused_threshold_seconds\":20,\"unrelated\":true}";
    private static string Result(string status) => "{\"results\":[{\"index\":0,\"status\":\"" + status + "\",\"error\":null}]}";
    private static void Check(bool condition, string message) { if (!condition) throw new Exception(message); checks++; }
    private static FakeServerHandler Fake(int code, string body) => new() {
        Reply = (_, _) => Task.FromResult(new HttpResponseMessage((HttpStatusCode)code) { Content = new StringContent(body) }) };
    private static ServerClient Client(FakeServerHandler handler) => new(Options, handler, TimeSpan.FromMilliseconds(200));
    private static async Task InvalidSettings(int code, string body)
    {
        using var client = Client(Fake(code, body));
        bool failed = false;
        try { await client.GetSettingsAsync(default); }
        catch (InvalidOperationException e) { failed = true; Check(!e.ToString().Contains("dummy-test-key"), "Secret in diagnostic."); }
        Check(failed, "Invalid settings/status accepted: " + code);
    }
    public static async Task<int> Run()
    {
        using (var handler = ServerClient.CreateHandler())
            Check(!handler.AllowAutoRedirect && !handler.UseCookies, "Redirects/cookies must be disabled.");
        foreach (string url in new[] { "http://example.invalid", "https://u:p@example.invalid", "https://example.invalid/api", "https://example.invalid/?x=y", "not a URL" })
        {
            bool failed = false;
            try { _ = new ServerOptions(true, url, "dummy-test-key"); } catch (ArgumentException) { failed = true; }
            Check(failed, "Invalid URL accepted.");
        }
        foreach (string key in new[] { "", " ", "bad\r\nheader" })
        {
            bool failed = false;
            try { _ = new ServerOptions(true, "https://example.invalid", key); } catch (ArgumentException) { failed = true; }
            Check(failed, "Invalid key accepted.");
        }
        Check(new ServerOptions(true, "http://localhost:8080", "dummy").Enabled, "Localhost development rejected.");
        Check(new ServerOptions(true, "http://127.0.0.1:8080", "dummy").Enabled, "Loopback development rejected.");
        var disabledHandler = Fake(200, Settings);
        using (var client = new ServerClient(new ServerOptions(false, null, null), disabledHandler, TimeSpan.FromSeconds(1)))
        {
            Check(!client.Enabled && disabledHandler.Calls == 0, "Disabled mode contacted server.");
        }

        var fake = new FakeServerHandler { Reply = async (request, token) => {
            Check(request.Headers.GetValues("X-EWS-Key").Single() == "dummy-test-key", "Authentication header missing.");
            if (request.Method == HttpMethod.Get)
            {
                Check(request.RequestUri!.AbsolutePath == "/api/settings", "Wrong settings route.");
                return new HttpResponseMessage(HttpStatusCode.OK) { Content = new StringContent(Settings) };
            }
            Check(request.Method == HttpMethod.Post && request.RequestUri!.AbsolutePath == "/api/reports", "Wrong report route.");
            Check(await request.Content!.ReadAsStringAsync(token) == ReportJson.Serialize(Report), "Report JSON changed.");
            Check(request.Content.Headers.ContentType!.MediaType == "application/json", "Wrong content type.");
            return new HttpResponseMessage(HttpStatusCode.Created) { Content = new StringContent(Result("created")) };
        }};
        using (var client = Client(fake))
        {
            var settings = await client.GetSettingsAsync(default);
            Check(settings.ReportSeconds == 15 && settings.IdleSeconds == 10 && settings.UnusedSeconds == 20 && settings.FallbackReason is null, "Settings not applied.");
            Check((await client.SendReportAsync(Report, default)).Outcome == ReportOutcome.Accepted, "Created not accepted.");
            Check(fake.Calls == 2, "Unexpected extra request/retry.");
        }
        foreach (int code in new[] { 408, 429, 500, 503 })
        {
            using var client = Client(Fake(code, "do not log response"));
            var result = await client.GetSettingsAsync(default);
            Check(result.ReportSeconds == 300 && result.IdleSeconds == 600 && result.UnusedSeconds == 3600 && result.FallbackReason != null, "Defaults not used.");
        }
        foreach (int code in new[] { 401, 403, 404, 302, 204 }) await InvalidSettings(code, Settings);
        foreach (string body in new[] { "not json", "[]", "{}", Settings.Replace(":15", ":0"), Settings.Replace(":15", ":1.5"), Settings.Replace(":15", ":\"15\""), Settings.Replace(":20", ":10"), new string('x', 65537) })
            await InvalidSettings(200, body);

        foreach (var pair in new[] { ("created", ReportOutcome.Accepted), ("duplicate", ReportOutcome.Duplicate), ("rejected", ReportOutcome.Rejected) })
        {
            using var client = Client(Fake(200, Result(pair.Item1)));
            Check((await client.SendReportAsync(Report, default)).Outcome == pair.Item2, "Wrong result interpretation.");
        }
        foreach (string body in new[] { "", "{}", "[]", "{\"results\":[]}", Result("created").Replace(":0", ":1"), Result("mystery"), Result("created").Replace("null", "123"), Result("created").Replace("null", "\"unexpected\""), new string('x', 65537) })
        {
            using var client = Client(Fake(200, body));
            Check((await client.SendReportAsync(Report, default)).Outcome == ReportOutcome.Dropped, "Malformed result accepted.");
        }
        foreach (int code in new[] { 401, 403, 422, 429, 500, 503, 307 })
        {
            var handler = Fake(code, "sensitive server body");
            using var client = Client(handler);
            var result = await client.SendReportAsync(Report, default);
            Check(result.Outcome == (code == 422 ? ReportOutcome.Rejected : ReportOutcome.Dropped), "HTTP error accepted.");
            Check(handler.Calls == 1 && !result.Reason.Contains("sensitive"), "Retried, followed redirect or exposed body.");
        }
        var network = new FakeServerHandler { Reply = (_, _) => throw new HttpRequestException("secret error text") };
        using (var client = Client(network))
        {
            Check((await client.GetSettingsAsync(default)).FallbackReason == "network failure", "Network settings failure not handled.");
            var result = await client.SendReportAsync(Report, default);
            Check(result.Outcome == ReportOutcome.Dropped && !result.Reason.Contains("secret"), "Network send failure not sanitized.");
        }
        var slow = new FakeServerHandler { Reply = async (_, token) => { await Task.Delay(Timeout.Infinite, token); return new HttpResponseMessage(); } };
        using (var client = Client(slow))
        {
            Check((await client.GetSettingsAsync(default)).FallbackReason == "request timeout", "Settings timeout not handled.");
            Check((await client.SendReportAsync(Report, default)).Outcome == ReportOutcome.Dropped, "Send timeout not handled.");
            using var cancellation = new CancellationTokenSource(); cancellation.CancelAfter(20);
            bool canceled = false;
            try { await client.GetSettingsAsync(cancellation.Token); } catch (OperationCanceledException) { canceled = true; }
            Check(canceled, "Startup cancellation swallowed.");
            canceled = false;
            using var sendCancellation = new CancellationTokenSource(); sendCancellation.CancelAfter(20);
            try { await client.SendReportAsync(Report, sendCancellation.Token); } catch (OperationCanceledException) { canceled = true; }
            Check(canceled, "Send cancellation swallowed.");
        }
        return checks;
    }
}
'@ | Set-Content -LiteralPath $harness
    Add-Type -Path @(
        (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
        (Join-Path $PSScriptRoot '../Reporting/DeviceReport.cs'),
        (Join-Path $PSScriptRoot '../Reporting/ReportJson.cs'),
        (Join-Path $PSScriptRoot '../Communication/ServerOptions.cs'),
        (Join-Path $PSScriptRoot '../Communication/ServerClient.cs'),
        $harness
    )
    $checks = [ServerChecks]::Run().GetAwaiter().GetResult()
    Write-Output "Passed $checks server-client checks using fake HTTP handlers only."
} finally { Remove-Item -LiteralPath $harness -Force -ErrorAction SilentlyContinue }
