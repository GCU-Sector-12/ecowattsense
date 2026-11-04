# Run with PowerShell 7: pwsh -NoProfile -File ./tests/Test-SingleInstance.ps1
# Tests the single-instance lock in temp folders only (no ProgramData, no monitoring).
$ErrorActionPreference = 'Stop'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-SingleInstanceTests-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testRoot
$harnessPath = Join-Path $testRoot 'SingleInstanceChecks.cs'
$lockSource = Join-Path $PSScriptRoot '../Identity/SingleInstanceLock.cs'
$child = $null
$deniedFile = $null
$denyRule = $null
try {
    @'
using System;
using System.IO;
using EcoWattSense.Client.Identity;

public static class SingleInstanceChecks
{
    public static int Checks;
    public static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
        Checks++;
    }

    public static void ExpectAlreadyRunning(string directory, string message)
    {
        try { SingleInstanceLock.Acquire(directory).Dispose(); }
        catch (ClientAlreadyRunningException) { Checks++; return; }
        throw new Exception(message);
    }

    // A real error (access denied, bad folder) must fail startup and must not look like "already running".
    public static void ExpectLockError(string directory, string message)
    {
        try { SingleInstanceLock.Acquire(directory).Dispose(); }
        catch (ClientAlreadyRunningException) { throw new Exception(message + " (reported as already running)"); }
        catch (InvalidOperationException) { Checks++; return; }
        throw new Exception(message + " (lock was taken)");
    }

    public static void Run(string root)
    {
        Check(ClientAlreadyRunningException.ExitCode == 2, "Exit code must be 2.");

        string directory = Path.Combine(root, "first");
        var first = SingleInstanceLock.Acquire(directory);
        Check(File.Exists(Path.Combine(directory, SingleInstanceLock.FileName)), "Lock file must be created.");
        ExpectAlreadyRunning(directory, "Second lock in the same folder must be refused.");

        using (var other = SingleInstanceLock.Acquire(Path.Combine(root, "other")))
            Check(other.FilePath != first.FilePath, "Another folder must have its own lock.");

        first.Dispose();
        first.Dispose(); // A second Dispose must be safe.
        using (var again = SingleInstanceLock.Acquire(directory))
            Check(again.FilePath == first.FilePath, "Lock must be free again after Dispose.");
        Check(File.Exists(first.FilePath), "Lock file must stay on disk.");

        string lockIsFolder = Path.Combine(root, "lock-is-folder");
        Directory.CreateDirectory(Path.Combine(lockIsFolder, SingleInstanceLock.FileName));
        ExpectLockError(lockIsFolder, "A folder in place of the lock file must be an error.");

        string blocked = Path.Combine(root, "blocked-directory");
        File.WriteAllText(blocked, "existing file");
        ExpectLockError(blocked, "A folder that cannot be created must be an error.");
        Check(File.ReadAllText(blocked) == "existing file", "Existing path was modified.");
    }
}
'@ | Set-Content -LiteralPath $harnessPath
    Add-Type -Path @($lockSource, $harnessPath)
    $checks = [SingleInstanceChecks]

    $checks::Run($testRoot)

    # Another process holds the lock, then crashes (is killed). The lock must be free afterwards.
    $crashDirectory = Join-Path $testRoot 'crash'
    $childScript = "`$ErrorActionPreference = 'Stop'; Add-Type -Path '$lockSource'; " +
        "`$held = [EcoWattSense.Client.Identity.SingleInstanceLock]::Acquire('$crashDirectory'); " +
        "[Console]::Out.WriteLine('locked'); [Console]::Out.Flush(); Start-Sleep -Seconds 600"
    $start = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    foreach ($argument in @('-NoProfile', '-NonInteractive', '-Command', $childScript)) { $start.ArgumentList.Add($argument) }
    $start.RedirectStandardOutput = $true
    $start.UseShellExecute = $false
    $child = [Diagnostics.Process]::Start($start)
    $line = $child.StandardOutput.ReadLineAsync()
    if (!$line.Wait([TimeSpan]::FromSeconds(60)) -or $line.Result -ne 'locked') { throw 'Child process did not take the lock.' }
    $checks::ExpectAlreadyRunning($crashDirectory, 'Lock held by another process must be refused.')
    $child.Kill()
    if (!$child.WaitForExit(30000)) { throw 'Child process did not end.' }
    $checks::Check($child.ExitCode -ne 0, 'Child process must end as killed.')
    $afterCrash = [EcoWattSense.Client.Identity.SingleInstanceLock]::Acquire($crashDirectory)
    $afterCrash.Dispose()
    $checks::Check($true, 'Lock must be free after the other process crashed.')

    # Real access denied: the current user may not read the lock file.
    $deniedDirectory = Join-Path $testRoot 'denied'
    [EcoWattSense.Client.Identity.SingleInstanceLock]::Acquire($deniedDirectory).Dispose()
    $deniedFile = [IO.FileInfo]::new((Join-Path $deniedDirectory 'instance.lock'))
    $security = [IO.FileSystemAclExtensions]::GetAccessControl($deniedFile, [Security.AccessControl.AccessControlSections]::Access)
    $denyRule = [Security.AccessControl.FileSystemAccessRule]::new(
        [Security.Principal.WindowsIdentity]::GetCurrent().User,
        [Security.AccessControl.FileSystemRights]::ReadData,
        [Security.AccessControl.AccessControlType]::Deny)
    $security.AddAccessRule($denyRule)
    [IO.FileSystemAclExtensions]::SetAccessControl($deniedFile, $security)
    $checks::ExpectLockError($deniedDirectory, 'Access denied on the lock file must be an error.')

    Write-Output "Passed $($checks::Checks) single-instance checks (including another process that crashed and an ACL access denial)."
} catch {
    Write-Output $_.Exception.ToString()
    throw
} finally {
    if ($child -and !$child.HasExited) { $child.Kill(); $null = $child.WaitForExit(30000) }
    if ($deniedFile -and $denyRule) {
        $security = [IO.FileSystemAclExtensions]::GetAccessControl($deniedFile, [Security.AccessControl.AccessControlSections]::Access)
        $null = $security.RemoveAccessRule($denyRule)
        [IO.FileSystemAclExtensions]::SetAccessControl($deniedFile, $security)
    }
    # Only delete the unique directory created above, after verifying its absolute parent.
    $resolvedRoot = [IO.Path]::GetFullPath($testRoot)
    $expectedParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolvedRoot) -ne $expectedParent -or
        ![IO.Path]::GetFileName($resolvedRoot).StartsWith('EcoWattSense-SingleInstanceTests-')) {
        throw "Refusing cleanup outside the test directory: $resolvedRoot"
    }
    Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
}
