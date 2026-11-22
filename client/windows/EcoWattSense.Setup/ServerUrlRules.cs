// ServerUrlRules.cs: the rules for the server address that Setup.exe gets on its command line as /server=<url>.
// Who uses it: SetupArguments.Parse cleans the value, the window prefills the Server page with it. The value comes from
// outside (for example from the web installer), so it is untrusted text: a bad value is ignored, never repaired.
namespace EcoWattSense.Setup;

/// <summary>Checks and chooses the server address offered on the command line.</summary>
public static class ServerUrlRules
{
    /// <summary>The longest address accepted.</summary>
    public const int MaxLength = 200;

    /// <summary>Returns the address when it is safe, otherwise an empty text.</summary>
    /// <param name="value">The text after /server=, or null.</param>
    /// <returns>The address, or "" when it is not https, too long, or holds a space, a quote or a control character.</returns>
    public static string Clean(string? value)
    {
        if (string.IsNullOrEmpty(value) || value.Length > MaxLength) return "";
        if (!value.StartsWith("https://", StringComparison.OrdinalIgnoreCase) || value.Length == "https://".Length) return "";
        bool bad = value.Any(letter => char.IsControl(letter) || char.IsWhiteSpace(letter) || letter == '"' || letter == '\'');
        return bad ? "" : value;
    }

    /// <summary>Chooses what the Server address field shows at the start: a stored address always wins.</summary>
    /// <param name="stored">The address stored in service.ini, or empty.</param>
    /// <param name="offered">The cleaned /server value, or empty.</param>
    public static string Prefill(string stored, string offered) => stored.Length > 0 ? stored : offered;
}
