// CalibrationResult.cs: the text shown on the Calibration page after the calibrator has closed.
// Who uses it: SetupWindow shows it after "Calibrate now"; the tests check every code.
// The exit codes are those of EnergyCalibrator.exe.
namespace EcoWattSense.Setup;

/// <summary>Maps the calibrator exit code to a line for the user.</summary>
public static class CalibrationResult
{
    /// <summary>The line shown while the calibrator runs.</summary>
    public const string Running = "Calibration is running.";

    /// <summary>Turns an exit code into text.</summary>
    /// <param name="code">The calibrator exit code, or -1 when it could not start.</param>
    /// <returns>A short sentence; an unknown code is shown as a number.</returns>
    public static string TextFor(int code) => code switch
    {
        0 => "Calibration finished.",
        1 => "Calibration stopped.",
        2 => "Calibration needs the battery: unplug the charger. You can run it later from the client.",
        3 => "Calibration was aborted.",
        4 => "The model file could not be written.",
        _ => $"Calibration ended with code {code}.",
    };
}
