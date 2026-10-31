$ErrorActionPreference = 'Stop'
Add-Type -Path @(
    (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
    (Join-Path $PSScriptRoot '../Reporting/DeviceReport.cs'),
    (Join-Path $PSScriptRoot '../Reporting/DeviceTypeSelection.cs')
)
$directory = Join-Path ([IO.Path]::GetTempPath()) ('EcoWattSense-TypeTest-' + [guid]::NewGuid().ToString('N'))
try {
    $output = [IO.StringWriter]::new()
    $type = [EcoWattSense.Client.Reporting.DeviceTypeSelection]
    $selected = $type::Resolve([System.Management.Automation.Language.NullString]::Value, $directory, [IO.StringReader]::new("BAD TYPE`n2`n"), $output, $true)
    if ($selected -ne 'desktop') { throw 'Choice/retry failed' }
    $saved = $type::Resolve([System.Management.Automation.Language.NullString]::Value, $directory, [IO.StringReader]::new(''), $output, $false)
    if ($saved -ne 'desktop') { throw 'Saved selection not reused' }
    if ($type::Resolve('laptop', $directory, [IO.StringReader]::new(''), $output, $false) -ne 'laptop') { throw 'Override failed' }
    if ((Get-Content (Join-Path $directory 'device-type.txt')).Trim() -ne 'desktop') { throw 'Override modified stored choice' }
    foreach ($case in @('invalid', 'unattended', 'eof')) {
        $casePath = Join-Path $directory $case
        if ($case -eq 'invalid') {
            $null = New-Item -ItemType Directory -Path $casePath
            Set-Content -LiteralPath (Join-Path $casePath 'device-type.txt') -Value 'BAD TYPE'
        }
        $failed = $false
        try { $null = $type::Resolve([System.Management.Automation.Language.NullString]::Value, $casePath, [IO.StringReader]::new(''), $output, ($case -ne 'unattended')) }
        catch { $failed = $true }
        if (!$failed) { throw "Expected failure: $case" }
    }
    if ((Get-Content (Join-Path $directory 'invalid/device-type.txt')).Trim() -ne 'BAD TYPE') { throw 'Invalid file overwritten' }
    Write-Output 'Passed 8 device-type selection checks.'
} finally {
    $resolved = [IO.Path]::GetFullPath($directory)
    $parent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Path]::GetDirectoryName($resolved) -ne $parent -or ![IO.Path]::GetFileName($resolved).StartsWith('EcoWattSense-TypeTest-')) { throw 'Unsafe cleanup path' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
