// CalibrateButtonState.cs: decides whether the Calibrate button may be used, and gives the reason when it may not.
// Who uses it: the viewer window (MainWindow.Calibrate.cs) and Test-CalibrateButton.ps1.
// It is pure: the facts come in, the state goes out. The window reads the facts from Windows and from the disk.
// The order of the reasons is fixed: a running calibration, then a missing program, then the power source, then the charge.
#nullable enable
namespace EcoWattSense.Client.Gui.Viewer;

/// <summary>The state of the Calibrate button.</summary>
/// <param name="Enabled">True when the button may be clicked.</param>
/// <param name="Reason">One short line that says why the button is disabled; empty when it is enabled.</param>
public readonly record struct CalibrateState(bool Enabled, string Reason);

/// <summary>Chooses the state of the Calibrate button.</summary>
public static class CalibrateButtonState
{
    /// <summary>The lowest battery charge, in percent, at which a calibration may start.</summary>
    public const int MinimumChargePercent = 30;

    /// <summary>Decides the state of the button.</summary>
    /// <param name="onBattery">True on battery, false on mains, null when unknown. Unknown counts as "not on battery".</param>
    /// <param name="chargePercent">The battery charge from 0 to 100, or null when unknown. Unknown counts as too low.</param>
    /// <param name="calibratorFound">True when the calibrator program was found.</param>
    /// <param name="running">True when a calibrator process runs.</param>
    /// <returns>Enabled with an empty reason, or disabled with the first reason that applies.</returns>
    public static CalibrateState Decide(bool? onBattery, int? chargePercent, bool calibratorFound, bool running)
    {
        if (running) return new CalibrateState(false, "Calibration is running.");
        if (!calibratorFound) return new CalibrateState(false, "Calibrator not found.");
        if (onBattery != true) return new CalibrateState(false, "Unplug the charger to calibrate.");
        if (chargePercent is null or < MinimumChargePercent) return new CalibrateState(false, "Battery below 30 %.");
        return new CalibrateState(true, "");
    }
}
