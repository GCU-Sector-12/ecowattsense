#nullable enable
using System;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EcoWattSense.Client.Reporting;

namespace EcoWattSense.Client.Communication;

public sealed record StartupSettings(int ReportSeconds, int IdleSeconds, int UnusedSeconds, string? FallbackReason = null);
public enum ReportOutcome { Accepted, Duplicate, Rejected, Dropped }
public sealed record SendResult(ReportOutcome Outcome, string Reason);

public sealed class ServerClient : IDisposable
{
    private readonly ServerOptions options;
    private readonly HttpClient http;
    private readonly TimeSpan timeout;
    public bool Enabled => options.Enabled;

    public ServerClient(ServerOptions options) : this(options, CreateHandler(), TimeSpan.FromSeconds(10)) { }

    // Injectable handler for offline tests; production always disables redirects.
    internal ServerClient(ServerOptions options, HttpMessageHandler handler, TimeSpan timeout)
    {
        this.options = options;
        this.timeout = timeout;
        http = new HttpClient(handler) { Timeout = Timeout.InfiniteTimeSpan };
    }

    internal static HttpClientHandler CreateHandler() => new() { AllowAutoRedirect = false, UseCookies = false };

    public async Task<StartupSettings> GetSettingsAsync(CancellationToken cancellationToken)
    {
        if (!Enabled) throw new InvalidOperationException("Server communication is disabled.");
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        deadline.CancelAfter(timeout);
        try
        {
            using var request = Request(HttpMethod.Get, "/api/settings");
            using var response = await http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, deadline.Token);
            int code = (int)response.StatusCode;
            if (code == 408 || code == 429 || code >= 500)
                return Fallback($"HTTP {code}");
            if (response.StatusCode != HttpStatusCode.OK)
                throw new InvalidOperationException($"Startup settings failed: HTTP {code}. Check server configuration and authentication.");
            using var json = await ReadJson(response, deadline.Token);
            var root = json.RootElement;
            int report = PositiveInteger(root, "report_interval_seconds");
            int idle = PositiveInteger(root, "idle_threshold_seconds");
            int unused = PositiveInteger(root, "unused_threshold_seconds");
            if (unused <= idle) throw new JsonException();
            return new(report, idle, unused);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested) { return Fallback("request timeout"); }
        catch (HttpRequestException) { cancellationToken.ThrowIfCancellationRequested(); return Fallback("network failure"); }
        catch (InvalidDataException) { throw new InvalidOperationException("Startup settings response exceeds the size limit."); }
        catch (IOException) { cancellationToken.ThrowIfCancellationRequested(); return Fallback("network read failure"); }
        catch (JsonException) { throw new InvalidOperationException("Invalid startup settings: expected positive integer thresholds with unused greater than idle."); }
    }

    public async Task<SendResult> SendReportAsync(DeviceReport report, CancellationToken cancellationToken)
    {
        if (!Enabled) throw new InvalidOperationException("Server communication is disabled.");
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        deadline.CancelAfter(timeout);
        try
        {
            using var request = Request(HttpMethod.Post, "/api/reports");
            request.Content = new StringContent(ReportJson.Serialize(report), Encoding.UTF8, "application/json");
            using var response = await http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, deadline.Token);
            if (!response.IsSuccessStatusCode)
                return new(response.StatusCode == HttpStatusCode.UnprocessableEntity ? ReportOutcome.Rejected : ReportOutcome.Dropped,
                    $"HTTP {(int)response.StatusCode}");
            using var json = await ReadJson(response, deadline.Token);
            var root = json.RootElement;
            if (root.ValueKind != JsonValueKind.Object || !root.TryGetProperty("results", out var results) ||
                results.ValueKind != JsonValueKind.Array || results.GetArrayLength() != 1)
                return Malformed();
            var item = results[0];
            if (item.ValueKind != JsonValueKind.Object || !item.TryGetProperty("index", out var index) ||
                index.ValueKind != JsonValueKind.Number || !index.TryGetInt32(out int number) || number != 0 ||
                !item.TryGetProperty("status", out var status) || status.ValueKind != JsonValueKind.String ||
                !item.TryGetProperty("error", out var error) || error.ValueKind is not (JsonValueKind.Null or JsonValueKind.String))
                return Malformed();
            return status.GetString() switch
            {
                "created" when error.ValueKind == JsonValueKind.Null => new(ReportOutcome.Accepted, "created"),
                "duplicate" when error.ValueKind == JsonValueKind.Null => new(ReportOutcome.Duplicate, "duplicate"),
                "rejected" => new(ReportOutcome.Rejected, "server rejected report"),
                _ => Malformed()
            };
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested) { return new(ReportOutcome.Dropped, "request timeout; acceptance unknown"); }
        catch (HttpRequestException) { cancellationToken.ThrowIfCancellationRequested(); return new(ReportOutcome.Dropped, "network failure; acceptance unknown"); }
        catch (InvalidDataException) { return Malformed(); }
        catch (IOException) { cancellationToken.ThrowIfCancellationRequested(); return new(ReportOutcome.Dropped, "network read failure; acceptance unknown"); }
        catch (JsonException) { return Malformed(); }
    }

    private HttpRequestMessage Request(HttpMethod method, string path)
    {
        var request = new HttpRequestMessage(method, new Uri(options.BaseUri!, path));
        request.Headers.Add("X-EWS-Key", options.Key);
        return request;
    }

    private static int PositiveInteger(JsonElement root, string name)
    {
        if (root.ValueKind != JsonValueKind.Object || !root.TryGetProperty(name, out var value) ||
            value.ValueKind != JsonValueKind.Number || !value.TryGetInt32(out int number) || number <= 0)
            throw new JsonException();
        return number;
    }

    private static async Task<JsonDocument> ReadJson(HttpResponseMessage response, CancellationToken token)
    {
        // Bound response memory; never include response text in diagnostics.
        using var stream = await response.Content.ReadAsStreamAsync(token);
        using var buffer = new MemoryStream();
        byte[] chunk = new byte[4096];
        int read;
        while ((read = await stream.ReadAsync(chunk.AsMemory(), token)) != 0)
        {
            if (buffer.Length + read > 65536) throw new InvalidDataException();
            buffer.Write(chunk, 0, read);
        }
        return JsonDocument.Parse(buffer.ToArray());
    }

    private static StartupSettings Fallback(string reason) => new(300, 600, 3600, reason);
    private static SendResult Malformed() => new(ReportOutcome.Dropped, "invalid report response; acceptance unknown");
    public void Dispose() => http.Dispose();
}
