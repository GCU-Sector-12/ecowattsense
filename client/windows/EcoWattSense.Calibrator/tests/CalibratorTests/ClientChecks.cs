// ClientChecks.cs: checks of the stream client: a fake program for every failure path (missing, no output, bad lines, ends in the middle,
// stopped by the user), and, where the system allows, a real child process (a shell script) to prove the pipes, the arguments and the close of the input.
// Who uses it: Program.Main. Failed or empty reads mean "unknown", never "nothing exists".
using System.Diagnostics;
using System.IO;
using EnergyCalibrator;
using static CalibratorTests.Harness;

namespace CalibratorTests;

/// <summary>Checks of WatchClient and ScanProcessLauncher.</summary>
internal static class ClientChecks
{
    private const string Line1 = "{\"t_ms\":2000,\"emi_uj\":[10,null],\"nvml_mw\":[1],\"nvml_mj\":[null],\"gpu_pct\":[1,1]}";
    private const string Line2 = "{\"t_ms\":4000,\"emi_uj\":[20,null],\"nvml_mw\":[2],\"nvml_mj\":[null],\"gpu_pct\":[2,2]}";

    /// <summary>Runs the group.</summary>
    public static void Run()
    {
        Group("client good", GoodStream);
        Group("client missing", MissingProgram);
        Group("client silent", SilentProgram);
        Group("client bad lines", BadLines);
        Group("client ends", EndsInTheMiddle);
        Group("client bad header", BadHeader);
        Group("client stop", StopTwice);
        Group("real process", RealProcess);
    }

    /// <summary>A header and two lines, then the program ends: the header and the latest line are available, the input is closed at the stop.</summary>
    private static void GoodStream()
    {
        FakeProcess process = new([StreamData.HeaderLine, Line1, Line2], true);
        WatchClient client = new(new FakeLauncher(process));
        client.Start();
        WatchHeader? header = client.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Result;
        Check(header != null && header.Emi.Count == 2, "client good: the header arrives");
        WaitUntil(() => client.Latest(ProcessClock.Seconds)?.TimeMs == 4000);
        Check(client.Latest(ProcessClock.Seconds)?.TimeMs == 4000, "client good: the latest line is the last one sent");
        client.StopAsync().Wait(TimeSpan.FromSeconds(5));
        Check(process.InputClosed && process.Disposed && !process.Killed, "client good: the stop closes the input, waits and disposes without killing");
        Check(client.Latest(ProcessClock.Seconds) == null, "client good: after the stop no line is given");
    }

    /// <summary>No program: the start does not throw, no header comes, the end reason is Failed.</summary>
    private static void MissingProgram()
    {
        FakeLauncher launcher = new(null);
        WatchClient client = new(launcher);
        client.Start();
        WatchHeader? header = client.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Result;
        Check(header == null && client.End == WatchEnd.Failed && launcher.Starts == 1, "client missing: no program gives no header and the reason Failed");
        client.StopAsync().Wait(TimeSpan.FromSeconds(3));
        Check(client.Latest(ProcessClock.Seconds) == null, "client missing: no line is given");
    }

    /// <summary>A program that prints nothing and keeps running: the wait for the header times out and the stop still works.</summary>
    private static void SilentProgram()
    {
        FakeProcess process = new([], true);
        WatchClient client = new(new FakeLauncher(process));
        client.Start();
        Stopwatch clock = Stopwatch.StartNew();
        WatchHeader? header = client.WaitForHeaderAsync(TimeSpan.FromMilliseconds(300)).Result;
        Check(header == null && clock.ElapsedMilliseconds >= 250, "client silent: no header within the time gives null after the wait");
        bool stopped = client.StopAsync().Wait(TimeSpan.FromSeconds(5));
        Check(stopped && process.InputClosed, "client silent: the stop closes the input and ends");
    }

    /// <summary>Bad lines between good ones are skipped and counted.</summary>
    private static void BadLines()
    {
        FakeProcess process = new([StreamData.HeaderLine, "garbage", Line1, "{\"t_ms\":", Line2], true);
        WatchClient client = new(new FakeLauncher(process));
        client.Start();
        client.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Wait();
        WaitUntil(() => client.Latest(ProcessClock.Seconds)?.TimeMs == 4000);
        Check(client.BadLines == 2 && client.Latest(ProcessClock.Seconds)?.TimeMs == 4000, "client bad lines: two bad lines are counted and the good ones are used");
        client.StopAsync().Wait(TimeSpan.FromSeconds(5));
    }

    /// <summary>The program ends after one line: the reading ends with InputEnded and no line stays fresh for ever.</summary>
    private static void EndsInTheMiddle()
    {
        FakeProcess process = new([StreamData.HeaderLine, Line1], false);
        WatchClient client = new(new FakeLauncher(process));
        client.Start();
        client.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Wait();
        WaitUntil(() => client.End == WatchEnd.InputEnded);
        Check(client.End == WatchEnd.InputEnded, "client ends: the end of the output is noticed");
        double later = ProcessClock.Seconds + 60;
        Check(client.Latest(later) == null, "client ends: a line from long ago is stale and not given");
        client.StopAsync().Wait(TimeSpan.FromSeconds(3));
        Check(client.End == WatchEnd.InputEnded, "client ends: the stop does not change the first reason");
    }

    /// <summary>A program whose first line is not a header: no header, reading ends with BadHeader, the program is stopped.</summary>
    private static void BadHeader()
    {
        FakeProcess process = new([Line1, Line2], true);
        WatchClient client = new(new FakeLauncher(process));
        client.Start();
        WatchHeader? header = client.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Result;
        Check(header == null && client.End == WatchEnd.BadHeader, "client bad header: a first line that is not a header gives no header");
        bool stopped = client.StopAsync().Wait(TimeSpan.FromSeconds(5));
        Check(stopped && process.InputClosed, "client bad header: the stop closes the input");
    }

    /// <summary>Stopping twice, or before the start, is harmless.</summary>
    private static void StopTwice()
    {
        WatchClient never = new(new FakeLauncher(null));
        Check(never.StopAsync().Wait(TimeSpan.FromSeconds(2)), "client stop: stop before start is harmless");
        FakeProcess process = new([StreamData.HeaderLine], true);
        WatchClient client = new(new FakeLauncher(process));
        client.Start();
        client.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Wait();
        client.StopAsync().Wait(TimeSpan.FromSeconds(5));
        Check(client.StopAsync().Wait(TimeSpan.FromSeconds(2)), "client stop: a second stop is harmless");
    }

    /// <summary>A real child process (a shell script) with redirected pipes. Skipped on Windows, where a script cannot be run this way.</summary>
    private static void RealProcess()
    {
        if (OperatingSystem.IsWindows()) { Info("real process: skipped on Windows"); return; }
        string folder = Path.Combine(Path.GetTempPath(), "ews-cal-proc-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        string script = Path.Combine(folder, "fake-scan.sh");
        File.WriteAllText(script, "#!/bin/sh\n[ \"$1 $2 $3 $4\" = \"--watch --period-ms 2000 \" ] || exit 9\nprintf '%s\\n' '" + StreamData.HeaderLine + "'\nprintf '%s\\n' '" + Line1 + "'\nwhile read -r line; do :; done\nexit 0\n");
        File.SetUnixFileMode(script, UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute);
        WatchClient client = new(new ScanProcessLauncher(script, ["--watch", "--period-ms", "2000"]));
        client.Start();
        WatchHeader? header = client.WaitForHeaderAsync(TimeSpan.FromSeconds(5)).Result;
        Check(header != null && header.Emi.Count == 2, "real process: the header of a real child process arrives through the pipe");
        WaitUntil(() => client.Latest(ProcessClock.Seconds)?.TimeMs == 2000);
        Check(client.Latest(ProcessClock.Seconds)?.TimeMs == 2000, "real process: a line arrives");
        Stopwatch clock = Stopwatch.StartNew();
        bool stopped = client.StopAsync().Wait(TimeSpan.FromSeconds(10));
        Check(stopped && clock.ElapsedMilliseconds < 5000, "real process: closing the input ends the child quickly (it ends by itself, no kill)");
        WatchClient missing = new(new ScanProcessLauncher(Path.Combine(folder, "no-such-file.exe"), ["--watch"]));
        missing.Start();
        Check(missing.WaitForHeaderAsync(TimeSpan.FromSeconds(3)).Result == null && missing.End == WatchEnd.Failed, "real process: a missing file gives Failed and no exception");
        Directory.Delete(folder, true);
    }

    /// <summary>Waits until a condition is true, at most 5 seconds. Used only by the checks, to wait for a background reader.</summary>
    /// <param name="condition">The condition.</param>
    private static void WaitUntil(Func<bool> condition)
    {
        Stopwatch clock = Stopwatch.StartNew();
        while (!condition() && clock.ElapsedMilliseconds < 5000) { Thread.Sleep(10); }
    }
}
