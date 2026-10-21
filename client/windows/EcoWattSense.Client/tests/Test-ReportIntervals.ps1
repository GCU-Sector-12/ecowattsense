$ErrorActionPreference = 'Stop'
$harness = Join-Path ([IO.Path]::GetTempPath()) ('ReportIntervalChecks-' + [guid]::NewGuid().ToString('N') + '.cs')
try {
@'
#nullable enable
using System;
using EcoWattSense.Client.Identity;
using EcoWattSense.Client.Monitoring;
using EcoWattSense.Client.Reporting;

public static class ReportIntervalChecks
{
    private static int checks;
    private static readonly DeviceIdentity Identity = new(Guid.Parse("8f20c31c-77ed-446d-a681-7717e3f62c25"));
    private static readonly DateTime End = new(2026, 10, 21, 12, 0, 0, DateTimeKind.Utc);
    private static CpuReading Baseline() => new(CpuReadingStatus.BaselineEstablished);
    private static CpuReading Sample(double busy = 10, double total = 100) => new(CpuReadingStatus.Valid, new CpuSample(busy, total));
    private static ReportInterval New(int seconds = 15) => new(Identity, new ReportingSettings(seconds, "laptop"));
    private static ReportIntervalResult Observe(ReportInterval interval, double time, CpuReading? cpu,
        double? idle = 600.9, ActivityState? state = ActivityState.Idle, bool? locked = false, bool? battery = true,
        DateTime? utc = null) => interval.Observe(TimeSpan.FromSeconds(time), utc ?? End, cpu, idle, state, locked, battery);
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
        checks++;
    }
    private static void Skip(ReportIntervalResult result, string part) =>
        Check(result.Report is null && result.SkipReason?.Contains(part) == true, "Expected skip: " + part);
    public static int Run()
    {
        var interval = New();
        Check(Observe(interval, 100, Baseline()).Report is null, "Baseline must not report.");
        Check(Observe(interval, 105, Sample()).Report is null, "Early report.");
        Check(Observe(interval, 114.999, Sample()).Report is null, "Reported before threshold.");
        var result = Observe(interval, 115, Sample(900, 900));
        var report = result.Report!;
        Check(report is not null && result.SkipReason is null, "Exact boundary must close.");
        Check(Math.Abs(report!.CpuPercent - 100.0 * 920 / 1100) < 1e-9, "Closing sample/weights missing.");
        Check(report.DeviceId == Identity.Id && report.Timestamp == End, "Identity/UTC end incorrect.");
        Check(report.IntervalSeconds == 15 && report.IdleSeconds == 600, "Durations not actual/floored.");
        Check(report.State == ActivityState.Idle && !report.ScreenLocked && report.OnBattery && report.DeviceType == "laptop", "End fields incorrect.");
        var next = Observe(interval, 131.9, Sample(0, 100), state: ActivityState.Active).Report!;
        Check(next.IntervalSeconds == 16 && next.CpuPercent == 0 && next.State == ActivityState.Active, "Overshoot/reset/end-state incorrect.");
        Check(Observe(interval, 140, Sample()).Report is null, "Scheduling reused prior boundary.");

        interval = New();
        Observe(interval, 0, Baseline());
        Skip(Observe(interval, 5, null), "CPU measurement");
        Skip(Observe(interval, 10, Baseline()), "coverage incomplete");
        Check(Observe(interval, 20, Sample()).Report is null, "Recovery started before fresh baseline.");
        Check(Observe(interval, 25, Sample()).Report?.IntervalSeconds == 15, "API gap recovery failed.");

        interval = New();
        Observe(interval, 0, Baseline());
        Skip(Observe(interval, 15, new CpuReading(CpuReadingStatus.Invalid)), "CPU measurement");
        Skip(Observe(interval, 20, Sample(100, 100)), "coverage incomplete");
        Check(Observe(interval, 35, Sample(0, 100)).Report?.CpuPercent == 0, "Recovery sample was reused.");

        foreach (string field in new[] { "inactivity", "state", "screen-lock", "power-source" })
        {
            interval = New(); Observe(interval, 0, Baseline());
            Observe(interval, 5, Sample()); // Good prior values must not fill unknown end values.
            Skip(Observe(interval, 15, Sample(), idle: field == "inactivity" ? null : 1,
                state: field == "state" ? null : ActivityState.Active,
                locked: field == "screen-lock" ? null : false,
                battery: field == "power-source" ? null : false), field);
            Check(Observe(interval, 30, Sample()).Report?.IntervalSeconds == 15, "Unknown-end skip did not reset.");
        }
        foreach (double idle in new[] { double.NaN, double.PositiveInfinity, -1.0 })
        {
            interval = New(); Observe(interval, 0, Baseline());
            Skip(Observe(interval, 15, Sample(), idle: idle), "inactivity");
        }
        interval = New(); Observe(interval, 0, Baseline());
        Skip(Observe(interval, 15, Sample(), idle: (double)int.MaxValue + 1), "integer range");
        Check(Observe(interval, 30, Sample(), idle: int.MaxValue + 0.5).Report?.IdleSeconds == int.MaxValue, "Floor-before-conversion boundary failed.");
        interval = New(); Observe(interval, 0, Baseline());
        Skip(Observe(interval, (double)int.MaxValue + 1, Sample()), "integer range");

        interval = New(); Observe(interval, 0, Baseline(), utc: End.AddDays(1));
        Check(Observe(interval, 15, Sample(), utc: End.AddDays(-1)).Report?.IntervalSeconds == 15, "Wall clock affected scheduling.");
        interval = New(); Observe(interval, 0, Baseline());
        Skip(Observe(interval, 15, Sample(), utc: DateTime.SpecifyKind(End, DateTimeKind.Unspecified)), "not UTC");
        interval = New(); Observe(interval, 0, Baseline());
        Observe(interval, 5, Sample(double.MaxValue, double.MaxValue));
        Skip(Observe(interval, 15, Sample(double.MaxValue, double.MaxValue)), "coverage");
        Check(Observe(interval, 30, Sample()).Report is not null, "Overflow recovery failed.");

        foreach (int duration in new[] { 0, -1 })
        {
            bool rejected = false;
            try { _ = new ReportingSettings(duration, "laptop"); } catch (ArgumentException) { rejected = true; }
            Check(rejected, "Invalid interval accepted.");
        }
        foreach (string type in new[] { "", "Laptop", "laptop\n", new string('a', 41) })
        {
            bool rejected = false;
            try { _ = new ReportingSettings(15, type); } catch (ArgumentException) { rejected = true; }
            Check(rejected, "Invalid device type accepted.");
        }
        return checks;
    }
}
'@ | Set-Content -LiteralPath $harness
    Add-Type -Path @(
        (Join-Path $PSScriptRoot '../Identity/DeviceIdentity.cs'),
        (Join-Path $PSScriptRoot '../Monitoring/StateClassifier.cs'),
        (Join-Path $PSScriptRoot '../Monitoring/CpuSample.cs'),
        (Join-Path $PSScriptRoot '../Monitoring/CpuIntervalAccumulator.cs'),
        (Join-Path $PSScriptRoot '../Reporting/DeviceReport.cs'),
        (Join-Path $PSScriptRoot '../Reporting/ReportingSettings.cs'),
        (Join-Path $PSScriptRoot '../Reporting/ReportInterval.cs'),
        $harness
    )
    $checks = [ReportIntervalChecks]::Run()
    Write-Output "Passed $checks report interval checks. Synthetic sample boundaries; no native calls."
} finally {
    Remove-Item -LiteralPath $harness -Force -ErrorAction SilentlyContinue
}
