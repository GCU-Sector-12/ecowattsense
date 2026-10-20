# PowerShell 7; compiles the production sources without starting the Worker.
$ErrorActionPreference = 'Stop'
Add-Type -Path @(
    (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
    (Join-Path $PSScriptRoot '../Reporting/DeviceReport.cs'),
    (Join-Path $PSScriptRoot '../Reporting/ReportJson.cs')
)
$script:checks = 0
function Assert-Check([bool] $condition, [string] $message) {
    if (!$condition) { throw $message }
    $script:checks++
}
function New-Report([hashtable] $overrides = @{}) {
    $values = @{
        DeviceId = [guid]'6F1C2A3E-9B4D-4C8E-A1F2-3D5E7B9C0A12'
        Timestamp = [datetime]::new(2026, 10, 19, 14, 5, 0, [DateTimeKind]::Utc)
        Interval = 300; State = [EcoWattSense.Client.Monitoring.ActivityState]::Active
        Idle = 12; Cpu = 37.5; Locked = $false; Battery = $false; DeviceType = 'laptop'
    }
    foreach ($key in $overrides.Keys) { $values[$key] = $overrides[$key] }
    return [EcoWattSense.Client.Reporting.DeviceReport]::new(
        $values.DeviceId, $values.Timestamp, $values.Interval, $values.State,
        $values.Idle, $values.Cpu, $values.Locked, $values.Battery, $values.DeviceType)
}
function Assert-Rejected([hashtable] $overrides) {
    $rejected = $false
    try { $null = New-Report $overrides }
    catch {
        if ($_.Exception.GetBaseException() -isnot [ArgumentException]) { throw }
        $rejected = $true
    }
    Assert-Check $rejected "Accepted invalid input: $($overrides | Out-String)"
}

$report = New-Report
$json = [EcoWattSense.Client.Reporting.ReportJson]::Serialize($report)
$schema = Get-Content -Raw (Join-Path $PSScriptRoot '../../../shared/report_schema.json') | ConvertFrom-Json
$document = [System.Text.Json.JsonDocument]::Parse($json)
try {
    $root = $document.RootElement
    $names = @($root.EnumerateObject() | ForEach-Object { $_.Name })
    Assert-Check ($names.Count -eq 10) 'Expected exactly ten fields.'
    Assert-Check (@(Compare-Object ($schema.required | Sort-Object) ($names | Sort-Object)).Count -eq 0) 'Field names differ from schema.'
    foreach ($name in $names) {
        $value = $root.GetProperty($name)
        $type = $schema.properties.$name.type
        switch ($type) {
            'string' { Assert-Check ($value.ValueKind -eq 'String') "$name must be a JSON string." }
            'boolean' { Assert-Check ($value.ValueKind -in @('True', 'False')) "$name must be a JSON boolean." }
            'number' { Assert-Check ($value.ValueKind -eq 'Number') "$name must be a JSON number." }
            'integer' {
                Assert-Check ($value.ValueKind -eq 'Number') "$name must be a JSON number."
                $integer = 0
                Assert-Check ($value.TryGetInt32([ref] $integer)) "$name must be an integer."
            }
        }
    }
    Assert-Check ($root.GetProperty('device_id').GetString() -ceq '6f1c2a3e-9b4d-4c8e-a1f2-3d5e7b9c0a12') 'UUID format is incorrect.'
    Assert-Check ($root.GetProperty('timestamp').GetString() -ceq '2026-10-19T14:05:00Z') 'UTC timestamp is incorrect.'
    Assert-Check ($root.GetProperty('os').GetString() -ceq 'windows') 'OS must be windows.'
    Assert-Check ($root.GetProperty('cpu_percent').GetDouble() -eq 37.5) 'CPU value changed.'
} finally { $document.Dispose() }

foreach ($state in 0, 1, 2) {
    $candidate = New-Report @{ State = [EcoWattSense.Client.Monitoring.ActivityState] $state; Cpu = 0; Idle = 0 }
    $doc = [System.Text.Json.JsonDocument]::Parse([EcoWattSense.Client.Reporting.ReportJson]::Serialize($candidate))
    try {
        Assert-Check ($doc.RootElement.GetProperty('state').GetInt32() -eq $state) 'State must serialize as its numeric code.'
        Assert-Check (@($doc.RootElement.EnumerateObject()).Count -eq 10) 'Zero/false fields were omitted.'
    } finally { $doc.Dispose() }
}

$utc = $report.Timestamp.AddTicks(1234567)
foreach ($timestamp in @($utc, $utc.ToLocalTime(), [DateTimeOffset]::new(2026, 10, 19, 16, 5, 0, [TimeSpan]::FromHours(2)).UtcDateTime)) {
    $candidate = New-Report @{ Timestamp = $timestamp }
    Assert-Check ($candidate.Timestamp.Kind -eq [DateTimeKind]::Utc) 'Timestamp was not normalized to UTC.'
    $doc = [System.Text.Json.JsonDocument]::Parse([EcoWattSense.Client.Reporting.ReportJson]::Serialize($candidate))
    try {
        $encoded = $doc.RootElement.GetProperty('timestamp')
        Assert-Check ($encoded.GetString().EndsWith('Z')) 'Timestamp must end in Z.'
        Assert-Check ($encoded.GetDateTime().Ticks -eq $timestamp.ToUniversalTime().Ticks) 'Timestamp instant/precision changed.'
    } finally { $doc.Dispose() }
}

foreach ($invalid in @(
    @{ DeviceId = [guid]::Empty },
    @{ Timestamp = [datetime]::SpecifyKind($utc, [DateTimeKind]::Unspecified) },
    @{ Interval = 0 }, @{ Interval = -1 }, @{ Idle = -1 },
    @{ State = [Enum]::ToObject([EcoWattSense.Client.Monitoring.ActivityState], -1) },
    @{ State = [Enum]::ToObject([EcoWattSense.Client.Monitoring.ActivityState], 3) },
    @{ Cpu = -0.001 }, @{ Cpu = 100.001 }, @{ Cpu = [double]::NaN },
    @{ Cpu = [double]::PositiveInfinity }, @{ Cpu = [double]::NegativeInfinity }
)) { Assert-Rejected $invalid }
foreach ($invalidType in @($null, '', 'Desktop', '1desktop', '_desktop', 'desk-top', 'desktop ', "laptop`n", 'dé sk', ('a' * 41))) {
    Assert-Rejected @{ DeviceType = $invalidType }
}
foreach ($validType in @('desktop', 'laptop', 'thin_client_2', 'a', ('a' * 40))) {
    $candidate = New-Report @{ DeviceType = $validType; Cpu = 100; Interval = 1; Locked = $true; Battery = $true }
    Assert-Check ($candidate.DeviceType -ceq $validType -and $candidate.CpuPercent -eq 100) 'Valid boundary/type rejected.'
    $parsed = [EcoWattSense.Client.Reporting.ReportJson]::Serialize($candidate) | ConvertFrom-Json
    Assert-Check ($parsed.screen_locked -eq $true -and $parsed.on_battery -eq $true) 'Boolean inputs changed.'
}
$modelType = [EcoWattSense.Client.Reporting.DeviceReport]
Assert-Check ($modelType.IsSealed) 'Report must be sealed.'
Assert-Check (@($modelType.GetProperties() | Where-Object { $_.CanWrite }).Count -eq 0) 'Report properties must be immutable.'
Assert-Check (@($modelType.GetConstructors()).Count -eq 1 -and $modelType.GetConstructors()[0].GetParameters().Count -eq 9) 'Every input must be supplied explicitly.'
Assert-Check (@($modelType.GetConstructors()[0].GetParameters() | Where-Object { $_.IsOptional }).Count -eq 0) 'Inputs must not have defaults.'

Write-Output "Passed $script:checks report checks."
Write-Output "Example: $json"
