// UpdateChecker.cs: downloads latest.json (SPEC 9, level 1 update check).
// Who uses it: MainWindow.Update.cs, at client start and each time the Settings page opens (no timer).
// Rules: HTTPS only, 10 seconds, at most 64 KB, no redirects (raw.githubusercontent.com answers directly).
// The body is untrusted; UpdateRules.ParseManifest checks it.
#nullable enable
using System;
using System.Net.Http;
using System.Threading.Tasks;

namespace EcoWattSense.Client.Gui.Update;

/// <summary>Fetches and checks the update manifest.</summary>
public static class UpdateChecker
{
    private static readonly TimeSpan Timeout = TimeSpan.FromSeconds(10);

    /// <summary>Gets latest.json.</summary>
    /// <returns>The checked manifest, or null on any network error, wrong status, too big a body or invalid content.</returns>
    public static async Task<UpdateManifest?> FetchAsync()
    {
        using var handler = new HttpClientHandler { AllowAutoRedirect = false };
        using var client = new HttpClient(handler) { Timeout = Timeout, MaxResponseContentBufferSize = UpdateRules.MaxManifestBytes };
        string? body = await TryGetBodyAsync(client);
        return body is null ? null : UpdateRules.ParseManifest(body);
    }

    /// <summary>Runs the request and turns a network failure into null.</summary>
    /// <param name="client">The client with the 10 second timeout.</param>
    /// <returns>The body text, or null on a network error, timeout or too big a body.</returns>
    private static async Task<string?> TryGetBodyAsync(HttpClient client)
    {
        try { return await GetBodyAsync(client); }
        catch (HttpRequestException) { return null; }
        catch (TaskCanceledException) { return null; }
    }

    /// <summary>Gets the manifest URL.</summary>
    /// <param name="client">The client.</param>
    /// <returns>The body text, or null when the status is not 200-299 or the text is longer than 64 KB.</returns>
    private static async Task<string?> GetBodyAsync(HttpClient client)
    {
        using HttpResponseMessage response = await client.GetAsync(UpdateRules.ManifestUrl);
        if (!response.IsSuccessStatusCode) return null;
        string body = await response.Content.ReadAsStringAsync();
        return body.Length <= UpdateRules.MaxManifestBytes ? body : null;
    }
}
