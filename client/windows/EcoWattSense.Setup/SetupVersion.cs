// SetupVersion.cs: the version of this Setup.exe, which is the version of the whole package (SPEC section 10).
// Who uses it: the window shows it at the bottom of the sidebar and the upgrade shows it in "Updated to <version>.".
// Build-Package.ps1 -Version sets it for every program with -p:Version, so it is stored as AssemblyInformationalVersion.
using System.Reflection;

namespace EcoWattSense.Setup;

/// <summary>Reads the version of this program.</summary>
public static class SetupVersion
{
    /// <summary>
    /// The version text, for example 0.1.0. The build adds "+<commit>" to the informational version; that part is cut off
    /// so the person sees only the number. An unreadable version gives "0.0.0".
    /// </summary>
    public static string Text
    {
        get
        {
            string? full = Assembly.GetExecutingAssembly().GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion;
            return Trim(full);
        }
    }

    /// <summary>Cuts the build metadata (after "+") off a version text.</summary>
    /// <param name="full">The informational version, or null.</param>
    /// <returns>The version number, or "0.0.0" when the text is empty.</returns>
    public static string Trim(string? full)
    {
        if (string.IsNullOrWhiteSpace(full)) return "0.0.0";
        int plus = full.IndexOf('+');
        return plus < 0 ? full : full[..plus];
    }
}
