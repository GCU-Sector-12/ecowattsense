<#
.SYNOPSIS
Checks identity reads during rename publication for client developers and release checks.
.DESCRIPTION
Keeps a Windows delete-access handle open across publication so the sharing race is deterministic.
Run with PowerShell 7 on Windows. Uses the actual identity sources and a private temporary directory.
#>
$ErrorActionPreference = 'Stop'

function Remove-IdentityTestDirectory {
    <#
    .SYNOPSIS
    Removes only the private directory created by this test.
    .PARAMETER Directory
    The absolute test directory under the current temporary directory.
    .OUTPUTS
    None. Throws if the directory is outside the expected parent or cleanup fails.
    #>
    param([string]$Directory)
    $resolved = [IO.Path]::GetFullPath($Directory)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or
        ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-IdentityRename-')) {
        throw "Refusing cleanup outside the test directory: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-IdentityRename-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $testRoot
$harnessPath = Join-Path $testRoot 'IdentityRenameChecks.cs'
try {
    @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
using EcoWattSense.Client.Identity;

public static class IdentityRenameChecks
{
    private const uint DeleteAccess = 0x00010000;
    private const uint ShareReadWriteDelete = 7;
    private const uint OpenExisting = 3;

    /// <summary>Checks that readers can reuse an identity while a rename handle is still open.</summary>
    /// <param name="root">The private directory used by this test.</param>
    /// <exception cref="Exception">A regression assertion fails or a file operation fails.</exception>
    public static void Run(string root)
    {
        string directory = Path.Combine(root, "published");
        Directory.CreateDirectory(directory);
        string temporaryPath = Path.Combine(directory, "identity.pending");
        string identityPath = Path.Combine(directory, DeviceIdentityStore.FileName);
        Guid expected = Guid.NewGuid();
        string contents = expected.ToString("D") + "\n";
        File.WriteAllText(temporaryPath, contents);

        // Windows renames with DELETE access, then closes the handle after the new name is visible.
        // Hold equivalent access across File.Move to make that short interval reproducible.
        using var renameHandle = OpenRenameHandle(temporaryPath);
        File.Move(temporaryPath, identityPath, overwrite: false);
        var result = new DeviceIdentityStore().LoadOrCreate(directory);
        Check(!result.Created && result.Identity.Id == expected,
            "A read during rename publication must reuse the persisted UUID.");

        renameHandle.Dispose();
        Check(File.ReadAllText(identityPath) == contents, "Reading must preserve the identity file.");
        Check(Directory.GetFiles(directory).Length == 1, "Reading must not create temporary files.");
    }

    /// <summary>Opens the delete-access handle that a Windows rename holds temporarily.</summary>
    /// <param name="path">The complete temporary identity file.</param>
    /// <returns>A handle owned by the caller.</returns>
    /// <exception cref="Win32Exception">Windows cannot open the test file.</exception>
    private static SafeFileHandle OpenRenameHandle(string path)
    {
        var handle = CreateFileW(path, DeleteAccess, ShareReadWriteDelete, IntPtr.Zero,
            OpenExisting, 0, IntPtr.Zero);
        if (!handle.IsInvalid) return handle;
        int error = Marshal.GetLastWin32Error();
        handle.Dispose();
        throw new Win32Exception(error);
    }

    /// <summary>Fails the test when a regression assertion does not hold.</summary>
    /// <param name="condition">The result to verify.</param>
    /// <param name="message">The failure explanation.</param>
    /// <exception cref="Exception">The condition is false.</exception>
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }

    /// <summary>Opens a Windows file with the requested access and sharing flags.</summary>
    /// <param name="name">The file path.</param>
    /// <param name="access">The requested access rights.</param>
    /// <param name="share">The permitted sharing modes.</param>
    /// <param name="security">Optional security attributes.</param>
    /// <param name="mode">The creation disposition.</param>
    /// <param name="flags">File attributes and flags.</param>
    /// <param name="template">An optional template file handle.</param>
    /// <returns>A file handle, or an invalid handle with the Windows error set.</returns>
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern SafeFileHandle CreateFileW(string name, uint access, uint share,
        IntPtr security, uint mode, uint flags, IntPtr template);
}
'@ | Set-Content -LiteralPath $harnessPath
    Add-Type -Path @(
        (Join-Path $PSScriptRoot '../Identity/DeviceIdentity.cs'),
        (Join-Path $PSScriptRoot '../Identity/DeviceIdentityStore.cs'),
        $harnessPath
    )
    [IdentityRenameChecks]::Run($testRoot)
    Write-Output 'Passed identity rename checks: concurrent read, UUID preservation and no temporary files.'
} finally {
    Remove-IdentityTestDirectory -Directory $testRoot
}
