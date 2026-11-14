// UninstallButton.cs: the state of the "Uninstall EcoWattSense..." button on the Settings page and how Setup is started.
// Who uses it: the Settings page (MainWindow.Settings.cs) and Test-UninstallButton.ps1.
// It is pure: the fact "Setup.exe was found" comes in, the state goes out. The window finds Setup.exe with ProgramLocator.
#nullable enable
namespace EcoWattSense.Client.Gui.Settings;

/// <summary>The state of the Uninstall button.</summary>
/// <param name="Enabled">True when the button may be clicked.</param>
/// <param name="Note">One short line under the button; empty when the button is enabled.</param>
public readonly record struct UninstallState(bool Enabled, string Note);

/// <summary>Chooses the state of the Uninstall button and gives the Setup arguments.</summary>
public static class UninstallButton
{
    /// <summary>The command line argument that starts Setup in uninstall mode.</summary>
    public const string SetupArguments = "/uninstall";

    /// <summary>Decides the state of the button.</summary>
    /// <param name="setupFound">True when Setup.exe was found.</param>
    /// <returns>Enabled with an empty note, or disabled with the line "Setup.exe not found.".</returns>
    public static UninstallState Decide(bool setupFound)
        => setupFound ? new UninstallState(true, "") : new UninstallState(false, "Setup.exe not found.");
}
