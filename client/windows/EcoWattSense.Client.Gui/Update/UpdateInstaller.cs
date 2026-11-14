// UpdateInstaller.cs: downloads the release ZIP and unpacks it (SPEC 9, level 1 update).
// Who uses it: MainWindow.Update.cs when the user clicks "Update". Starting Setup.exe /upgrade is done by the window,
// which already has the elevated start with the UAC-cancel handling (TryStartSetup).
// Rules: every hop of the download is https on a GitHub host (UpdateRules.IsAllowedRedirect), redirects are followed by hand
// so each hop is checked, the byte count must equal the manifest size, the SHA256 must match. The ZIP is unpacked with the
// system tar.exe, so no own ZIP code runs on downloaded data. A damaged or failed download deletes its folder.
#nullable enable
using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;

namespace EcoWattSense.Client.Gui.Update;

/// <summary>The result of a download.</summary>
/// <param name="Ok">True when the ZIP is complete and its SHA256 matches.</param>
/// <param name="Note">The line for the user when Ok is false; empty otherwise.</param>
/// <param name="Folder">The temporary folder that holds the ZIP (already deleted when Ok is false).</param>
/// <param name="ZipPath">The full path of the ZIP.</param>
public readonly record struct DownloadResult(bool Ok, string Note, string Folder, string ZipPath);

/// <summary>Downloads, checks and unpacks the update ZIP.</summary>
public static class UpdateInstaller
{
    /// <summary>The line shown when size or SHA256 do not match.</summary>
    public const string DamagedNote = "The download is damaged. Try again.";

    /// <summary>The line shown when the network fails or the address is not allowed.</summary>
    public const string FailedNote = "The download failed. Try again.";

    private const int MaxHops = 5;
    private const int BufferSize = 81920;

    /// <summary>Makes a new random folder in %TEMP%.</summary>
    /// <returns>The full path of the new, empty folder.</returns>
    public static string NewFolder()
    {
        string name = "EcoWattSense-Update-" + Path.GetRandomFileName().Replace(".", "");
        string folder = Path.Combine(Path.GetTempPath(), name);
        Directory.CreateDirectory(folder);
        return folder;
    }

    /// <summary>Downloads the ZIP and checks size and SHA256.</summary>
    /// <param name="manifest">The checked manifest.</param>
    /// <param name="progress">Gets the number of bytes received so far.</param>
    /// <returns>Ok with the ZIP path, or a note; on failure the folder is deleted.</returns>
    public static async Task<DownloadResult> DownloadAsync(UpdateManifest manifest, IProgress<long> progress)
    {
        string folder = NewFolder();
        string zip = Path.Combine(folder, "update.zip");
        string note = await TryDownloadAsync(manifest, zip, progress);
        if (note.Length > 0) DeleteFolder(folder);
        return new DownloadResult(note.Length == 0, note, folder, zip);
    }

    /// <summary>Runs the download and turns every expected failure into a note.</summary>
    /// <param name="manifest">The checked manifest.</param>
    /// <param name="zip">The file to write.</param>
    /// <param name="progress">Gets the bytes received.</param>
    /// <returns>An empty text on success, otherwise the note for the user.</returns>
    private static async Task<string> TryDownloadAsync(UpdateManifest manifest, string zip, IProgress<long> progress)
    {
        try { return await DownloadCoreAsync(manifest, zip, progress); }
        catch (HttpRequestException) { return FailedNote; }
        catch (IOException) { return FailedNote; }
        catch (TaskCanceledException) { return FailedNote; }
    }

    /// <summary>Opens the connection and saves the body; the caller handles the network exceptions.</summary>
    /// <param name="manifest">The checked manifest.</param>
    /// <param name="zip">The file to write.</param>
    /// <param name="progress">Gets the bytes received.</param>
    /// <returns>An empty text on success, otherwise the note for the user.</returns>
    private static async Task<string> DownloadCoreAsync(UpdateManifest manifest, string zip, IProgress<long> progress)
    {
        using var handler = new HttpClientHandler { AllowAutoRedirect = false };
        using var client = new HttpClient(handler) { Timeout = TimeSpan.FromMinutes(10) };
        Uri? first = Uri.TryCreate(manifest.ZipUrl, UriKind.Absolute, out Uri? parsed) ? parsed : null;
        using HttpResponseMessage? response = await OpenAsync(client, first, MaxHops);
        if (response is null) return FailedNote;
        return await SaveAsync(response, manifest, zip, progress);
    }

    /// <summary>Opens the download, following redirects by hand so each hop is checked.</summary>
    /// <param name="client">The client; automatic redirects are off.</param>
    /// <param name="uri">The address of this hop, or null.</param>
    /// <param name="hopsLeft">How many more redirects may be followed.</param>
    /// <returns>The final response (status 200, headers read), or null when a hop is not allowed, a status is wrong or there are too many hops.</returns>
    private static async Task<HttpResponseMessage?> OpenAsync(HttpClient client, Uri? uri, int hopsLeft)
    {
        if (hopsLeft < 0 || !UpdateRules.IsAllowedRedirect(uri)) return null;
        HttpResponseMessage response = await client.GetAsync(uri!, HttpCompletionOption.ResponseHeadersRead);
        if (response.StatusCode == HttpStatusCode.OK) return response;
        Uri? next = NextHop(response, uri!);
        return await OpenAsync(client, next, hopsLeft - 1);
    }

    /// <summary>Reads the redirect target and disposes the response.</summary>
    /// <param name="response">A response that is not 200.</param>
    /// <param name="current">The address that was requested.</param>
    /// <returns>The absolute target of a 3xx answer with a Location header; otherwise null.</returns>
    private static Uri? NextHop(HttpResponseMessage response, Uri current)
    {
        Uri? target = response.Headers.Location;
        int code = (int)response.StatusCode;
        response.Dispose();
        bool redirect = code >= 300 && code < 400 && target is not null;
        return redirect ? new Uri(current, target!) : null;
    }

    /// <summary>Writes the body to the file and checks size and SHA256.</summary>
    /// <param name="response">The open response.</param>
    /// <param name="manifest">The checked manifest.</param>
    /// <param name="zip">The file to write.</param>
    /// <param name="progress">Gets the bytes received.</param>
    /// <returns>An empty text when size and hash match, otherwise <see cref="DamagedNote"/>.</returns>
    private static async Task<string> SaveAsync(HttpResponseMessage response, UpdateManifest manifest, string zip, IProgress<long> progress)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        long total;
        await using (Stream body = await response.Content.ReadAsStreamAsync())
        await using (var file = new FileStream(zip, FileMode.CreateNew, FileAccess.Write, FileShare.None))
        {
            total = await CopyAsync(body, file, hash, manifest.Size, progress);
        }
        bool sizeOk = total == manifest.Size;
        return sizeOk && UpdateRules.Sha256Matches(manifest.Sha256, hash.GetHashAndReset()) ? "" : DamagedNote;
    }

    /// <summary>Copies the stream and hashes it. It stops as soon as more than the expected bytes arrive.</summary>
    /// <param name="source">The response body.</param>
    /// <param name="target">The file.</param>
    /// <param name="hash">The running SHA256.</param>
    /// <param name="limit">The size from the manifest.</param>
    /// <param name="progress">Gets the bytes received.</param>
    /// <returns>The number of bytes received; limit + 1 when the body is longer than the limit.</returns>
    private static async Task<long> CopyAsync(Stream source, Stream target, IncrementalHash hash, long limit, IProgress<long> progress)
    {
        byte[] buffer = new byte[BufferSize];
        long total = 0;
        int read = await source.ReadAsync(buffer);
        while (read > 0 && total + read <= limit)
        {
            total += read;
            hash.AppendData(buffer, 0, read);
            await target.WriteAsync(buffer.AsMemory(0, read));
            progress.Report(total);
            read = await source.ReadAsync(buffer);
        }
        return read > 0 ? limit + 1 : total;
    }

    /// <summary>Unpacks the ZIP with %SystemRoot%\System32\tar.exe into the same folder.</summary>
    /// <param name="folder">The folder that holds the ZIP.</param>
    /// <param name="zip">The ZIP path.</param>
    /// <returns>The path of EcoWattSense\Setup.exe in the folder, or null when tar failed, timed out or Setup.exe is missing.</returns>
    public static async Task<string?> ExtractAsync(string folder, string zip)
    {
        var info = new ProcessStartInfo(Path.Combine(Environment.SystemDirectory, "tar.exe"))
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = folder
        };
        info.ArgumentList.Add("-xf");
        info.ArgumentList.Add(zip);
        info.ArgumentList.Add("-C");
        info.ArgumentList.Add(folder);
        int code = await RunAsync(info);
        string setup = Path.Combine(folder, "EcoWattSense", "Setup.exe");
        return code == 0 && File.Exists(setup) ? setup : null;
    }

    /// <summary>Runs a process and waits for it, at most two minutes.</summary>
    /// <param name="info">The start info.</param>
    /// <returns>The exit code, or -1 when the process did not start or did not end in time.</returns>
    private static async Task<int> RunAsync(ProcessStartInfo info)
    {
        using var limit = new CancellationTokenSource(TimeSpan.FromMinutes(2));
        try { return await StartAndWaitAsync(info, limit.Token); }
        catch (System.ComponentModel.Win32Exception) { return -1; }
        catch (OperationCanceledException) { return -1; }
    }

    /// <summary>Starts the process and waits for it to end.</summary>
    /// <param name="info">The start info.</param>
    /// <param name="token">Cancels the wait.</param>
    /// <returns>The exit code, or -1 when the process did not start.</returns>
    private static async Task<int> StartAndWaitAsync(ProcessStartInfo info, CancellationToken token)
    {
        using Process? process = Process.Start(info);
        if (process is null) return -1;
        await process.WaitForExitAsync(token);
        return process.ExitCode;
    }

    /// <summary>Deletes a folder this class made; a failure is ignored (the folder is in %TEMP%).</summary>
    /// <param name="folder">The folder.</param>
    public static void DeleteFolder(string folder)
    {
        try { Directory.Delete(folder, true); }
        catch (IOException) { }
        catch (UnauthorizedAccessException) { }
    }
}
