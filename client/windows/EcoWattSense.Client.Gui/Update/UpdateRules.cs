// UpdateRules.cs: the pure rules of the client update (level 1, SPEC 9): manifest checks, version compare,
// redirect host allowlist, SHA256 compare, the "seen version" rule and the texts of the Updates section.
// Who uses it: UpdateChecker.cs, UpdateInstaller.cs, MainWindow.Update.cs and Test-Update.ps1.
// It has no network, no file and no WPF code, so the test compiles it alone. latest.json comes from the network and is
// untrusted: every field is type-checked and range-checked here before anything is downloaded.
#nullable enable
using System;
using System.Collections;
using System.Globalization;
using System.Linq;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace EcoWattSense.Client.Gui.Update;

/// <summary>The checked content of latest.json.</summary>
/// <param name="Version">Digits and dots, for example "0.1.1".</param>
/// <param name="ZipUrl">The https address of the release ZIP on github.com.</param>
/// <param name="Sha256">The expected SHA256 of the ZIP, 64 hex characters.</param>
/// <param name="Size">The expected size of the ZIP in bytes.</param>
public sealed record UpdateManifest(string Version, string ZipUrl, string Sha256, long Size);

/// <summary>What the Updates section shows.</summary>
public enum UpdateState
{
    /// <summary>The check is running.</summary>
    Checking,
    /// <summary>The client is as new as the manifest or newer.</summary>
    UpToDate,
    /// <summary>The manifest holds a newer version.</summary>
    Available,
    /// <summary>The manifest could not be read or is not valid.</summary>
    Failed
}

/// <summary>The lines of the Updates section.</summary>
/// <param name="VersionLine">"This version 0.1.0".</param>
/// <param name="StatusLine">The result of the check.</param>
/// <param name="ShowButton">True when the Update button is shown.</param>
public readonly record struct UpdateView(string VersionLine, string StatusLine, bool ShowButton);

/// <summary>The rules of the update.</summary>
public static class UpdateRules
{
    /// <summary>Where the manifest lives (SPEC 9). HTTPS only.</summary>
    public const string ManifestUrl = "https://raw.githubusercontent.com/GCU-Sector-12/ecowattsense/EWS-energy-suite/client/spike/energy-suite/release/latest.json";

    /// <summary>The only place a release ZIP may come from.</summary>
    public const string ZipUrlPrefix = "https://github.com/GCU-Sector-12/ecowattsense/releases/download/";

    /// <summary>The largest manifest accepted, in bytes.</summary>
    public const int MaxManifestBytes = 64 * 1024;

    /// <summary>The largest ZIP accepted, in bytes (300 MB).</summary>
    public const long MaxZipBytes = 300L * 1024 * 1024;

    /// <summary>The hosts a download may be redirected to (GitHub release assets).</summary>
    private static readonly string[] AllowedHosts =
        { "github.com", "objects.githubusercontent.com", "release-assets.githubusercontent.com" };

    private static readonly Regex VersionPattern = new("^[0-9]{1,9}(\\.[0-9]{1,9}){0,3}$", RegexOptions.Compiled);
    private static readonly Regex HashPattern = new("^[0-9a-fA-F]{64}$", RegexOptions.Compiled);

    /// <summary>Checks the manifest text.</summary>
    /// <param name="text">The body of latest.json.</param>
    /// <returns>The checked manifest, or null when the text is not JSON or a field is missing or wrong.</returns>
    public static UpdateManifest? ParseManifest(string text)
    {
        try
        {
            using JsonDocument document = JsonDocument.Parse(text);
            return FromRoot(document.RootElement);
        }
        catch (JsonException)
        {
            return null;
        }
    }

    /// <summary>Builds the manifest from the root element.</summary>
    /// <param name="root">The root JSON value.</param>
    /// <returns>The manifest, or null when the root is not an object or a field fails its check.</returns>
    private static UpdateManifest? FromRoot(JsonElement root)
    {
        if (root.ValueKind != JsonValueKind.Object) return null;
        string? version = GetText(root, "version");
        string? zip = GetText(root, "zip_url");
        string? hash = GetText(root, "sha256");
        long size = GetSize(root);
        bool good = IsVersionText(version)
            && zip is not null && zip.StartsWith(ZipUrlPrefix, StringComparison.Ordinal) && zip.Length <= 2048
            && hash is not null && HashPattern.IsMatch(hash)
            && size >= 1 && size <= MaxZipBytes;
        return good ? new UpdateManifest(version!, zip!, hash!.ToLowerInvariant(), size) : null;
    }

    /// <summary>Reads the "size" property.</summary>
    /// <param name="root">The JSON object.</param>
    /// <returns>The whole number, or 0 when the property is missing or not a whole number (0 fails the range check).</returns>
    private static long GetSize(JsonElement root)
    {
        if (!root.TryGetProperty("size", out JsonElement value)) return 0;
        if (value.ValueKind != JsonValueKind.Number) return 0;
        return value.TryGetInt64(out long size) ? size : 0;
    }

    /// <summary>Reads a string property.</summary>
    /// <param name="root">The JSON object.</param>
    /// <param name="name">The property name.</param>
    /// <returns>The text, or null when the property is missing or not a string.</returns>
    private static string? GetText(JsonElement root, string name)
    {
        if (!root.TryGetProperty(name, out JsonElement value)) return null;
        return value.ValueKind == JsonValueKind.String ? value.GetString() : null;
    }

    /// <summary>Tells whether a text is a version: 1 to 4 numbers separated by dots.</summary>
    /// <param name="text">The text, or null.</param>
    /// <returns>True for "0.1.0"; false for null, "1.x", "v1" or an empty text.</returns>
    public static bool IsVersionText(string? text) => text is not null && VersionPattern.IsMatch(text);

    /// <summary>Takes the "+build" suffix off an informational version.</summary>
    /// <param name="informational">The AssemblyInformationalVersion, or null.</param>
    /// <returns>The text before the first "+"; an empty text for null.</returns>
    public static string StripBuild(string? informational) => (informational ?? "").Split('+')[0].Trim();

    /// <summary>Compares two versions number by number; a missing part counts as 0.</summary>
    /// <param name="left">A version text.</param>
    /// <param name="right">A version text.</param>
    /// <returns>Negative when left is older, 0 when equal, positive when newer; null when either text is not a version.</returns>
    public static int? CompareVersions(string? left, string? right)
    {
        if (!IsVersionText(left) || !IsVersionText(right)) return null;
        return Math.Sign(StructuralComparisons.StructuralComparer.Compare(Parts(left!), Parts(right!)));
    }

    /// <summary>Splits a checked version into four numbers; missing parts are 0, so "1.0" and "1.0.0.0" are equal.</summary>
    /// <param name="version">A text that passed <see cref="IsVersionText"/>.</param>
    /// <returns>An array of four numbers.</returns>
    private static long[] Parts(string version)
    {
        string[] pieces = version.Split('.');
        long[] numbers = new long[4];
        for (int i = 0; i < pieces.Length; i++) numbers[i] = long.Parse(pieces[i], CultureInfo.InvariantCulture);
        return numbers;
    }

    /// <summary>Tells whether a download or a redirect may go to this address.</summary>
    /// <param name="uri">The address, or null.</param>
    /// <returns>True only for https on the default port, no user info, and one of the GitHub hosts.</returns>
    public static bool IsAllowedRedirect(Uri? uri)
    {
        if (uri is null || !uri.IsAbsoluteUri) return false;
        bool secure = uri.Scheme == Uri.UriSchemeHttps && uri.IsDefaultPort && uri.UserInfo.Length == 0;
        return secure && AllowedHosts.Contains(uri.Host, StringComparer.OrdinalIgnoreCase);
    }

    /// <summary>Compares the SHA256 of the download with the manifest.</summary>
    /// <param name="expectedHex">The 64 hex characters from the manifest.</param>
    /// <param name="actual">The hash bytes of the file.</param>
    /// <returns>True when they are the same (case does not matter).</returns>
    public static bool Sha256Matches(string expectedHex, byte[] actual)
        => Convert.ToHexString(actual).Equals(expectedHex, StringComparison.OrdinalIgnoreCase);

    /// <summary>Decides whether the balloon is shown: once for each newer version.</summary>
    /// <param name="available">The newer version found.</param>
    /// <param name="seenText">The content of update-seen.txt, or null when there is no file.</param>
    /// <returns>True when the file is missing, empty or holds another version.</returns>
    public static bool ShouldNotify(string available, string? seenText)
        => !string.Equals((seenText ?? "").Trim(), available, StringComparison.Ordinal);

    /// <summary>Gives the state for a manifest.</summary>
    /// <param name="ownVersion">The client's version without a "+build" suffix.</param>
    /// <param name="manifest">The checked manifest, or null when the check failed.</param>
    /// <returns>Failed for null or a version that cannot be compared; Available when newer; otherwise UpToDate.</returns>
    public static UpdateState StateFor(string ownVersion, UpdateManifest? manifest)
    {
        int? order = CompareVersions(ownVersion, manifest?.Version);
        if (order is null) return UpdateState.Failed;
        return order < 0 ? UpdateState.Available : UpdateState.UpToDate;
    }

    /// <summary>Gives the progress line of the download.</summary>
    /// <param name="received">The bytes received.</param>
    /// <param name="total">The bytes expected.</param>
    /// <returns>For example "Downloading... 1.5 of 40.2 MB".</returns>
    public static string ProgressLine(long received, long total)
    {
        string done = (received / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture);
        string all = (total / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture);
        return "Downloading... " + done + " of " + all + " MB";
    }

    /// <summary>Gives the lines of the Updates section.</summary>
    /// <param name="ownVersion">The client's version.</param>
    /// <param name="state">The state of the check.</param>
    /// <param name="available">The newer version, used only for Available.</param>
    /// <returns>The version line, the status line and whether the Update button shows.</returns>
    public static UpdateView View(string ownVersion, UpdateState state, string? available)
    {
        string version = "This version " + ownVersion;
        return state switch
        {
            UpdateState.Checking => new UpdateView(version, "Checking for updates...", false),
            UpdateState.UpToDate => new UpdateView(version, "Up to date.", false),
            UpdateState.Available => new UpdateView(version, "Update available: " + available, true),
            _ => new UpdateView(version, "Could not check for updates.", false)
        };
    }
}
