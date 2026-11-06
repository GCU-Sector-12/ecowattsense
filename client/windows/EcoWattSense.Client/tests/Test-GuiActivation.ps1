param([string]$RequestLock)
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../../EcoWattSense.Client.Gui/GuiActivationPipe.cs'
$harness = Join-Path ([IO.Path]::GetTempPath()) ('ActivationChecks-' + [guid]::NewGuid() + '.cs')
try {
    @'
using System;
using System.IO;
using System.IO.Pipes;
using System.Threading;
using System.Threading.Tasks;
using EcoWattSense.Client.Gui;

public static class ActivationChecks
{
    static int handled;
    static GuiActivationPipe owner;
    public static void Start(string path)
    {
        owner = new GuiActivationPipe(path, async token => {
            await Task.Delay(50, token);
            Interlocked.Increment(ref handled);
            return true;
        });
    }
    public static Task Stop() => owner.DisposeAsync().AsTask();
    public static bool Request(string path) =>
        GuiActivationPipe.RequestAsync(path, CancellationToken.None).GetAwaiter().GetResult();
    public static void CheckCount(int expected)
    {
        if (handled != expected) throw new Exception($"Expected {expected} restores, got {handled}.");
    }
    public static async Task BadRequest(string path)
    {
        using var pipe = new NamedPipeClientStream(".", GuiActivationPipe.NameFor(path),
            PipeDirection.InOut, PipeOptions.Asynchronous);
        using var limit = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        await pipe.ConnectAsync(limit.Token);
        try
        {
            await pipe.WriteAsync("OPEN-extra"u8.ToArray(), limit.Token);
            if (await pipe.ReadAsync(new byte[8], limit.Token) != 0)
                throw new Exception("Acknowledged an invalid request.");
        }
        catch (IOException) { /* Windows may report the rejected connection as broken. */ }
    }
    public static async Task BadReply(string path, bool stall)
    {
        using var pipe = new NamedPipeServerStream(GuiActivationPipe.NameFor(path), PipeDirection.InOut,
            1, PipeTransmissionMode.Message, PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        using var limit = new CancellationTokenSource(TimeSpan.FromSeconds(6));
        var request = GuiActivationPipe.RequestAsync(path, CancellationToken.None);
        await pipe.WaitForConnectionAsync(limit.Token);
        await pipe.ReadExactlyAsync(new byte[4], limit.Token);
        if (!stall)
        {
            try { await pipe.WriteAsync("OPENED-extra"u8.ToArray(), limit.Token); }
            catch (IOException) { /* The client disconnected on the invalid reply. */ }
        }
        if (await request.WaitAsync(limit.Token))
            throw new Exception("Reported success without a valid acknowledgement.");
    }
}
'@ | Set-Content -LiteralPath $harness
    Add-Type -Path @($source, $harness)
    if ($RequestLock) {
        if (![ActivationChecks]::Request($RequestLock)) { throw 'Cross-process request failed.' }
        return
    }
    $path = Join-Path ([IO.Path]::GetTempPath()) ('activation-lock-' + [guid]::NewGuid())
    [ActivationChecks]::Start($path)
    try {
        [ActivationChecks]::BadRequest($path).GetAwaiter().GetResult()
        [ActivationChecks]::CheckCount(0)
        & (Get-Process -Id $PID).Path -NoProfile -File $PSCommandPath -RequestLock $path
        if ($LASTEXITCODE) { throw 'The launching process failed.' }
        [ActivationChecks]::CheckCount(1)
        if ([ActivationChecks]::Request($path + '-other')) { throw 'Activated another identity directory.' }
    } finally { [ActivationChecks]::Stop().GetAwaiter().GetResult() }
    if ([ActivationChecks]::Request($path)) { throw 'Acknowledged after shutdown.' }
    [ActivationChecks]::BadReply($path, $false).GetAwaiter().GetResult()
    [ActivationChecks]::BadReply($path, $true).GetAwaiter().GetResult()
    Write-Output 'Passed cross-process activation, invalid request and acknowledgement, separate lock, shutdown and stalled reply checks.'
} finally { Remove-Item -LiteralPath $harness -ErrorAction SilentlyContinue }
