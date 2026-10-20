# Run with PowerShell 7: pwsh -NoProfile -File ./tests/Test-DeviceIdentity.ps1
# Tests the actual identity sources without starting monitors or accessing ProgramData.
$ErrorActionPreference = 'Stop'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-IdentityTests-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testRoot
$harnessPath = Join-Path $testRoot 'IdentityChecks.cs'
try {
    @'
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EcoWattSense.Client.Identity;

public static class IdentityChecks
{
    private static int checks;
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
        checks++;
    }

    private static void ExpectFailure(Action action, string message)
    {
        try { action(); }
        catch (InvalidOperationException) { checks++; return; }
        throw new Exception(message);
    }

    public static int Run(string root)
    {
        string directory = Path.Combine(root, "creation");
        string path = Path.Combine(directory, DeviceIdentityStore.FileName);
        var first = new DeviceIdentityStore().LoadOrCreate(directory);
        Check(first.Created && first.Identity.Id != Guid.Empty, "First run must create an identity.");
        Check(Guid.Parse(File.ReadAllText(path)) == first.Identity.Id, "Returned identity must be persisted.");
        byte[] original = File.ReadAllBytes(path);
        DateTime originalTime = File.GetLastWriteTimeUtc(path);
        var second = new DeviceIdentityStore().LoadOrCreate(directory);
        Check(!second.Created && second.Identity.Id == first.Identity.Id, "New store must reuse identity.");
        Check(File.ReadAllBytes(path).SequenceEqual(original) && File.GetLastWriteTimeUtc(path) == originalTime,
            "Reading identity must not rewrite it.");

        string[] invalid = { "", "not-a-uuid", Guid.Empty.ToString(), Guid.NewGuid() + "\n" + Guid.NewGuid() };
        for (int i = 0; i < invalid.Length; i++)
        {
            string badDirectory = Path.Combine(root, "invalid-" + i);
            Directory.CreateDirectory(badDirectory);
            string badPath = Path.Combine(badDirectory, DeviceIdentityStore.FileName);
            File.WriteAllText(badPath, invalid[i]);
            byte[] before = File.ReadAllBytes(badPath);
            ExpectFailure(() => new DeviceIdentityStore().LoadOrCreate(badDirectory), "Invalid identity accepted.");
            Check(File.ReadAllBytes(badPath).SequenceEqual(before), "Invalid identity was replaced.");
            Check(Directory.GetFiles(badDirectory).Length == 1, "Invalid file triggered temporary creation.");
        }

        using (var locked = new FileStream(path, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
            ExpectFailure(() => new DeviceIdentityStore().LoadOrCreate(directory), "Unreadable file must fail.");
        Check(File.ReadAllBytes(path).SequenceEqual(original), "Unreadable identity was replaced.");

        string blockedDirectory = Path.Combine(root, "blocked-directory");
        File.WriteAllText(blockedDirectory, "existing file");
        ExpectFailure(() => new DeviceIdentityStore().LoadOrCreate(blockedDirectory), "Invalid directory must fail.");
        Check(File.ReadAllText(blockedDirectory) == "existing file", "Existing path was modified.");

        foreach (string relative in new[] { "", "relative", ".", @"C:relative", @"\root-relative" })
        {
            bool rejected = false;
            try { DeviceIdentityStore.ResolveDirectory(relative); }
            catch (ArgumentException) { rejected = true; }
            Check(rejected, "Accepted non-absolute directory: " + relative);
        }
        Check(DeviceIdentityStore.ResolveDirectory(root) == Path.GetFullPath(root), "Absolute override rejected.");
        Check(DeviceIdentityStore.ResolveDirectory() == Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "EcoWattSense", "Client"),
            "Incorrect default directory.");

        for (int round = 0; round < 5; round++)
        {
            string concurrentDirectory = Path.Combine(root, "concurrent-" + round);
            using (var gate = new Barrier(8))
            {
                var tasks = Enumerable.Range(0, 8).Select(_ => Task.Factory.StartNew(() =>
                {
                    if (!gate.SignalAndWait(TimeSpan.FromSeconds(30))) throw new Exception("Barrier timeout.");
                    return new DeviceIdentityStore().LoadOrCreate(concurrentDirectory);
                }, CancellationToken.None, TaskCreationOptions.LongRunning, TaskScheduler.Default)).ToArray();
                Task.WaitAll(tasks);
                var results = tasks.Select(task => task.Result).ToArray();
                Guid persisted = Guid.Parse(File.ReadAllText(Path.Combine(concurrentDirectory, DeviceIdentityStore.FileName)));
                Check(results.All(result => result.Identity.Id == persisted), "Concurrent callers disagreed.");
                Check(results.Count(result => result.Created) == 1, "Expected exactly one creator.");
                Check(Directory.GetFiles(concurrentDirectory).Length == 1, "Temporary files leaked.");
            }
        }
        return checks;
    }
}
'@ | Set-Content -LiteralPath $harnessPath
    Add-Type -Path @(
        (Join-Path $PSScriptRoot '../Identity/DeviceIdentity.cs'),
        (Join-Path $PSScriptRoot '../Identity/DeviceIdentityStore.cs'),
        $harnessPath
    )
    $checks = [IdentityChecks]::Run($testRoot)
    Write-Output "Passed $checks identity checks (including 5 rounds of 8 concurrent creators)."
    Write-Output 'Windows ACL permission-denial tests were not run; exclusive-file-lock failure was tested.'
} catch {
    Write-Output $_.Exception.ToString()
    throw
} finally {
    # Only delete the unique directory created above, after verifying its absolute parent.
    $resolvedRoot = [IO.Path]::GetFullPath($testRoot)
    $expectedParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolvedRoot) -ne $expectedParent -or
        ![IO.Path]::GetFileName($resolvedRoot).StartsWith('EcoWattSense-IdentityTests-')) {
        throw "Refusing cleanup outside the test directory: $resolvedRoot"
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
