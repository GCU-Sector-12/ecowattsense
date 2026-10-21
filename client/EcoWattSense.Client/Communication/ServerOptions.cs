#nullable enable
using System;

namespace EcoWattSense.Client.Communication;

public sealed class ServerOptions
{
    public bool Enabled { get; }
    public Uri? BaseUri { get; }
    internal string Key { get; } = "";

    public ServerOptions(bool enabled, string? baseUrl, string? clientKey)
    {
        Enabled = enabled;
        if (!enabled) return;
        if (!Uri.TryCreate(baseUrl, UriKind.Absolute, out var uri) ||
            !(uri.Scheme == "https" || (uri.Scheme == "http" && uri.IsLoopback)) ||
            uri.UserInfo.Length != 0 || uri.Query.Length != 0 || uri.Fragment.Length != 0 || uri.AbsolutePath != "/")
            throw new ArgumentException("Server:BaseUrl must be an HTTPS origin (HTTP allowed only for localhost/loopback), without credentials, path, query or fragment.");
        if (string.IsNullOrWhiteSpace(clientKey))
            throw new ArgumentException("Server:ClientKey is required when server communication is enabled.");
        foreach (char c in clientKey)
            if (c < 33 || c > 126)
                throw new ArgumentException("Server:ClientKey must contain only printable non-space ASCII characters.");
        BaseUri = uri;
        Key = clientKey;
    }
}
