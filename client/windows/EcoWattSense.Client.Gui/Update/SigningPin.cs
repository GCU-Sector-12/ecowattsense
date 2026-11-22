// SigningPin.cs: the pinned SHA-1 thumbprint of the EcoWattSense signing certificate, for the client update (SPEC 9).
// Who uses it: SignatureChecker.cs. This is the one place of the C# code that holds the value; the native twin is
// ews::kSigningThumbprint in ews-core (ews/signature_check.h). Both must always hold the same thumbprint.
#nullable enable

namespace EcoWattSense.Client.Gui.Update;

/// <summary>The pinned signer thumbprint.</summary>
public static class SigningPin
{
    /// <summary>The value that switches the signature check off, until the real thumbprint is known.</summary>
    public const string Placeholder = "PENDING_THUMBPRINT";

    /// <summary>The SHA-1 thumbprint (40 hex digits, any case) of the signing certificate, or <see cref="Placeholder"/>.</summary>
    public const string Thumbprint = "F53EC709CE14C833AECF191E11DAB909446B6CBA";
}
