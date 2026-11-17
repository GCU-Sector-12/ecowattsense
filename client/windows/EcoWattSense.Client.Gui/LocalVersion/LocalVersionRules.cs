// LocalVersionRules.cs: the pure rules of the local version listener (SPEC section 9): parse a request head and build the reply.
// Who uses it: LocalVersionServer.cs (live) and Test-LocalVersion.ps1 (tests, no network).
// Why it exists: the dashboard (a web page) asks the client on the same computer for its version, to show
// Install / Update / Up to date. The only data ever given is the version text. Network input is untrusted:
// every request is checked here, and anything that is not exactly GET or OPTIONS of /version gets an empty 404.
#nullable enable
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace EcoWattSense.Client.Gui.LocalVersion;

/// <summary>The parts of a request that the listener looks at.</summary>
/// <param name="Method">The HTTP method, for example GET.</param>
/// <param name="Path">The request target, for example /version.</param>
/// <param name="Origin">The Origin header, or null when it is missing.</param>
/// <param name="PrivateNetwork">True when Access-Control-Request-Private-Network is "true".</param>
public sealed record LocalVersionRequest(string Method, string Path, string? Origin, bool PrivateNetwork);

/// <summary>Parses request heads and builds replies for the local version listener.</summary>
public static class LocalVersionRules
{
    /// <summary>The loopback TCP port of the listener.</summary>
    public const int Port = 47613;

    /// <summary>The most bytes of a request head that are read.</summary>
    public const int MaxHeadBytes = 8192;

    private const string HeadEnd = "\r\n\r\n";

    private static readonly string[] AllowedOrigins =
    {
        "https://dev.ecowattsense.co.uk",
        "http://localhost:5173",
        "http://127.0.0.1:5173"
    };

    /// <summary>Reads the request line and the two headers that matter.</summary>
    /// <param name="head">The request head, with or without the empty line at its end; null or empty is refused.</param>
    /// <returns>The request, or null when the head is empty, too long or has a bad request line.</returns>
    public static LocalVersionRequest? ParseHead(string? head)
    {
        if (string.IsNullOrEmpty(head) || head.Length > MaxHeadBytes) return null;
        var lines = head.Split("\r\n");
        var parts = lines[0].Split(' ');
        bool good = parts.Length == 3 && parts[0].Length > 0 && parts[1].StartsWith('/') && parts[2].StartsWith("HTTP/1.");
        if (!good) return null;
        var privateNetwork = HeaderValue(lines, "Access-Control-Request-Private-Network");
        return new LocalVersionRequest(parts[0], parts[1], HeaderValue(lines, "Origin"),
            string.Equals(privateNetwork, "true", StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>Gives the value of one header.</summary>
    /// <param name="lines">The lines of the head; line 0 is the request line and is skipped.</param>
    /// <param name="name">The header name, compared without regard to case.</param>
    /// <returns>The trimmed value of the first match, or null.</returns>
    private static string? HeaderValue(string[] lines, string name) =>
        lines.Skip(1).Select(line => line.Split(':', 2))
            .Where(pair => pair.Length == 2 && pair[0].Trim().Equals(name, StringComparison.OrdinalIgnoreCase))
            .Select(pair => pair[1].Trim()).FirstOrDefault();

    /// <summary>Finds the end of a request head in the bytes read so far.</summary>
    /// <param name="data">The bytes read.</param>
    /// <returns>True when the empty line that ends the head is present.</returns>
    public static bool HeadComplete(ReadOnlySpan<byte> data) => data.IndexOf("\r\n\r\n"u8) >= 0;

    /// <summary>Cuts the head text out of the bytes read.</summary>
    /// <param name="data">The bytes read; must pass <see cref="HeadComplete"/>.</param>
    /// <returns>The text before the empty line; null when the empty line is missing.</returns>
    public static string? HeadText(ReadOnlySpan<byte> data)
    {
        var text = Encoding.Latin1.GetString(data);
        int end = text.IndexOf(HeadEnd, StringComparison.Ordinal);
        return end < 0 ? null : text[..end];
    }

    /// <summary>Builds the full HTTP reply for a request.</summary>
    /// <param name="request">The parsed request, or null for a bad one.</param>
    /// <param name="version">The version text of the client, without a "+build" suffix.</param>
    /// <returns>The reply text: 200 with JSON for GET /version, 204 for OPTIONS /version, otherwise an empty 404.</returns>
    public static string BuildResponse(LocalVersionRequest? request, string version)
    {
        bool known = request is not null && request.Path == "/version";
        if (known && request!.Method == "GET") return Reply("200 OK", request, JsonSerializer.Serialize(new { version }));
        if (known && request!.Method == "OPTIONS") return Reply("204 No Content", request, null);
        return Reply("404 Not Found", null, "");
    }

    /// <summary>Writes the status line, the headers and the body.</summary>
    /// <param name="status">The status code and text.</param>
    /// <param name="cors">The request that decides the CORS headers, or null for none.</param>
    /// <param name="body">The body; null means no body and no Content-Length (204).</param>
    /// <returns>The reply text.</returns>
    private static string Reply(string status, LocalVersionRequest? cors, string? body)
    {
        var lines = new List<string> { "HTTP/1.1 " + status, "Connection: close" };
        if (body is not null) lines.Add("Content-Length: " + Encoding.UTF8.GetByteCount(body));
        if (body is { Length: > 0 }) lines.Add("Content-Type: application/json");
        lines.AddRange(CorsLines(cors));
        return string.Join("\r\n", lines) + HeadEnd + body;
    }

    /// <summary>Gives the CORS headers for a request.</summary>
    /// <param name="request">The request, or null.</param>
    /// <returns>No lines unless the Origin is exactly one of the allowed origins.</returns>
    private static IEnumerable<string> CorsLines(LocalVersionRequest? request)
    {
        if (request?.Origin is null || !AllowedOrigins.Contains(request.Origin)) return Array.Empty<string>();
        var lines = new List<string>
        {
            "Access-Control-Allow-Origin: " + request.Origin,
            "Vary: Origin",
            "Access-Control-Allow-Methods: GET",
            "Access-Control-Max-Age: 600"
        };
        if (request.PrivateNetwork) lines.Add("Access-Control-Allow-Private-Network: true");
        return lines;
    }
}
