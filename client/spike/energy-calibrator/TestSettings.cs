// TestSettings.cs: every number that shapes the calibration test, in one place so it is easy to change.
// Who uses it: PhaseCatalogue, TestRunner, PhaseAnalyser, PowerModel, StartGate, GraphicsWindow and the readers.
// How to use: change a constant, then rebuild with "dotnet publish -c Release -o out". Nothing else needs editing.
namespace EnergyCalibrator;

/// <summary>Constants for the length of the phases, the safety limits and the graphics load. Times are in seconds.</summary>
internal static class TestSettings
{
    /// <summary>Length of every phase. Seven phases of 150 s are 17.5 minutes, about 18 minutes with the pauses.</summary>
    public const int PhaseSeconds = 150;

    /// <summary>Seconds at the start of every phase that are left out of the result while the laptop settles.</summary>
    public const int SettleSeconds = 30;

    /// <summary>Seconds between two samples of all sources.</summary>
    public const int SampleIntervalSeconds = 2;

    /// <summary>The test stops when the battery falls below this percentage.</summary>
    public const int MinimumBatteryPercent = 15;

    /// <summary>The Start button stays disabled below this percentage, so the test cannot hit the stop limit.</summary>
    public const int MinimumStartBatteryPercent = 30;

    /// <summary>A phase whose battery drop is below this many mWh gets a warning about the coarse battery steps.</summary>
    public const double LowDropWarningMwh = 100;

    /// <summary>Screen brightness (percent) of the "screen 0%" phase.</summary>
    public const int ScreenLowPercent = 0;

    /// <summary>Screen brightness (percent) of the "screen 100%" phase.</summary>
    public const int ScreenHighPercent = 100;

    /// <summary>Screen brightness (percent) of the reference phase and of every load phase.</summary>
    public const int ScreenReferencePercent = 50;

    /// <summary>A phase gets a warning when the measured mean brightness is further than this from the target (points).</summary>
    public const double BrightnessTolerancePoints = 5;

    /// <summary>The CPU 50% phase gets a warning when the mean CPU use is further than this from 50 (points).</summary>
    public const double CpuHalfTolerancePoints = 15;

    /// <summary>The CPU 100% phase gets a warning when the mean CPU use is below this percentage.</summary>
    public const double CpuFullMinimumPercent = 85;

    /// <summary>The graphics phase gets a warning when the mean GPU 3D use is below this percentage.</summary>
    public const double GraphicsMinimumGpuPercent = 5;

    /// <summary>Smallest difference in CPU use (points) between two phases that is accepted for "watts per CPU percent".</summary>
    public const double MinimumCpuSpanPoints = 5;

    /// <summary>Number of animated shapes in the graphics phase.</summary>
    public const int GraphicsShapeCount = 200;

    /// <summary>Every n-th shape gets a blur effect, which is drawn by a pixel shader on the graphics chip.</summary>
    public const int GraphicsBlurEveryNth = 3;

    /// <summary>Radius of the blur effect in device-independent pixels.</summary>
    public const double GraphicsBlurRadius = 8;

    /// <summary>Seed of the random numbers that place the shapes, so every run draws the same scene.</summary>
    public const int GraphicsSeed = 20261111;

    /// <summary>Power in watts that the start warning assumes for the whole test, to estimate the share of the battery it uses.</summary>
    public const double FullPowerEstimateWatts = 15;

    /// <summary>Seconds of tolerance when deciding whether the user touched the mouse or keyboard while the display was off.</summary>
    public const double InputGraceSeconds = 3;

    /// <summary>How long one call that switches the display on or off may wait for a window that does not answer.</summary>
    public const int DisplayMessageTimeoutMs = 1500;

    /// <summary>Seconds to wait after the graphics window has closed before the display-off command is sent, so the window change cannot wake the display.</summary>
    public const double DisplayOffDelaySeconds = 2;

    /// <summary>Seconds to wait for the display state "off" after the first command, before the broadcast fall-back is sent.</summary>
    public const double DisplayFirstWaitSeconds = 3;

    /// <summary>Seconds after the first command at which the display counts as "stayed on" when it still has not reported "off".</summary>
    public const double DisplayGiveUpSeconds = 10;
}
