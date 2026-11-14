// LocalVersionServer.cs: a tiny HTTP/1.1 listener on 127.0.0.1 that tells the local dashboard the client version (SPEC section 9).
// Who uses it: App.xaml.cs starts it at launch (viewer and classic mode) and disposes it at exit; Test-LocalVersion.ps1 starts it on a free port.
// How it works: one connection at a time; at most 8 KB of request head is read within 2 seconds, then the reply is
// written and the connection closed. The rules (what is answered, CORS) are in LocalVersionRules.cs.
// If the port is busy the listener is skipped silently and a count is written to the trace output.
#nullable enable
using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace EcoWattSense.Client.Gui.LocalVersion;

/// <summary>Answers GET /version on the loopback interface until it is disposed.</summary>
public sealed class LocalVersionServer : IDisposable
{
    private static readonly TimeSpan ConnectionTimeout = TimeSpan.FromSeconds(2);
    private static int skippedStarts;

    private readonly TcpListener listener;
    private readonly string version;
    private readonly CancellationTokenSource stop = new();

    private LocalVersionServer(TcpListener listener, string version)
    {
        this.listener = listener;
        this.version = version;
    }

    /// <summary>How many starts were skipped because the port was busy.</summary>
    public static int SkippedStarts => Volatile.Read(ref skippedStarts);

    /// <summary>The port the listener is bound to (useful when port 0 was asked for).</summary>
    public int Port => ((IPEndPoint)listener.LocalEndpoint).Port;

    /// <summary>Starts listening on 127.0.0.1 and begins to answer in the background.</summary>
    /// <param name="port">The TCP port; <see cref="LocalVersionRules.Port"/> in the client, 0 for a free port in tests.</param>
    /// <param name="version">The version text to give, without a "+build" suffix.</param>
    /// <returns>The running server, or null when the port is busy or cannot be opened.</returns>
    public static LocalVersionServer? TryStart(int port, string version)
    {
        var tcp = new TcpListener(IPAddress.Loopback, port) { ExclusiveAddressUse = true };
        try { tcp.Start(); }
        catch (SocketException)
        {
            // Another program (or a second client) owns the port: the dashboard just shows "Install", so stay quiet.
            int count = Interlocked.Increment(ref skippedStarts);
            Trace.WriteLine("local_version_skipped count=" + count);
            return null;
        }
        var server = new LocalVersionServer(tcp, version);
        _ = server.RunAsync();
        return server;
    }

    /// <summary>Stops listening. A reply in progress is cut off.</summary>
    public void Dispose()
    {
        stop.Cancel();
        listener.Stop();
        stop.Dispose();
    }

    /// <summary>Accepts and answers connections one after another until stopped.</summary>
    /// <returns>A task that ends after <see cref="Dispose"/>.</returns>
    private async Task RunAsync()
    {
        while (!stop.IsCancellationRequested) await AcceptOneAsync();
    }

    /// <summary>Accepts one connection and answers it. A failed connection is dropped; the loop goes on.</summary>
    /// <returns>A task that ends when the connection is closed or failed.</returns>
    private async Task AcceptOneAsync()
    {
        try
        {
            using var client = await listener.AcceptTcpClientAsync(stop.Token);
            using var limit = CancellationTokenSource.CreateLinkedTokenSource(stop.Token);
            limit.CancelAfter(ConnectionTimeout);
            await AnswerAsync(client.GetStream(), limit.Token);
        }
        catch (Exception ex) when (ex is IOException or SocketException or OperationCanceledException or ObjectDisposedException)
        {
            // A client that vanished, was too slow or sent too much, or a stop: nothing to answer.
        }
    }

    /// <summary>Reads the request head and writes the reply.</summary>
    /// <param name="stream">The connection.</param>
    /// <param name="token">Cancels the read and the write (2 s limit).</param>
    /// <returns>A task that ends after the reply is written; no reply when the head is oversized or the client left early.</returns>
    private async Task AnswerAsync(NetworkStream stream, CancellationToken token)
    {
        var head = await ReadHeadAsync(stream, token);
        if (head is null) return;
        var reply = LocalVersionRules.BuildResponse(LocalVersionRules.ParseHead(head), version);
        await stream.WriteAsync(Encoding.UTF8.GetBytes(reply), token);
    }

    /// <summary>Reads until the empty line that ends the head, at most 8 KB.</summary>
    /// <param name="stream">The connection.</param>
    /// <param name="token">Cancels the read.</param>
    /// <returns>The head text, or null when the limit was reached or the client closed before the head ended.</returns>
    private static async Task<string?> ReadHeadAsync(NetworkStream stream, CancellationToken token)
    {
        var buffer = new byte[LocalVersionRules.MaxHeadBytes];
        int length = 0;
        bool more = true;
        while (more)
        {
            int read = await stream.ReadAsync(buffer.AsMemory(length), token);
            length += read;
            more = read > 0 && length < buffer.Length && !LocalVersionRules.HeadComplete(buffer.AsSpan(0, length));
        }
        return LocalVersionRules.HeadComplete(buffer.AsSpan(0, length)) ? LocalVersionRules.HeadText(buffer.AsSpan(0, length)) : null;
    }
}
