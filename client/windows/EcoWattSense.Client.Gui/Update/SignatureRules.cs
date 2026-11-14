// SignatureRules.cs: the pure decision about the signature of the extracted Setup.exe (SPEC 9).
// Who uses it: SignatureChecker.cs and Test-SignatureCheck.ps1 (which compiles this file and SigningPin.cs alone).
// It has no file and no Windows call: the WinVerifyTrust result and the signer thumbprint come in as plain values.
#nullable enable
using System;

namespace EcoWattSense.Client.Gui.Update;

/// <summary>What to do with the extracted Setup.exe.</summary>
public enum SignatureVerdict
{
    /// <summary>The signature is valid and the signer is ours: start it.</summary>
    Start,
    /// <summary>The pin is the placeholder: the check was not done, start it.</summary>
    Skipped,
    /// <summary>Anything else: do not start it.</summary>
    Refuse
}

/// <summary>The rules of the signature check.</summary>
public static class SignatureRules
{
    /// <summary>The line shown when Setup.exe is refused.</summary>
    public const string RefusedNote = "The downloaded Setup is not signed by EcoWattSense. It was not started.";

    /// <summary>WinVerifyTrust result: valid and trusted.</summary>
    public const int TrustOk = 0;

    /// <summary>WinVerifyTrust result TRUST_E_NOSIGNATURE.</summary>
    public const int TrustNoSignature = unchecked((int)0x800B0100);

    /// <summary>WinVerifyTrust result TRUST_E_BAD_DIGEST: the file changed after signing.</summary>
    public const int TrustBadDigest = unchecked((int)0x80096010);

    /// <summary>WinVerifyTrust result CERT_E_UNTRUSTEDROOT: signed, but this computer does not trust the root yet.</summary>
    public const int TrustUntrustedRoot = unchecked((int)0x800B0109);

    /// <summary>WinVerifyTrust result CERT_E_CHAINING: signed, but the chain does not end in a trusted root.</summary>
    public const int TrustChaining = unchecked((int)0x800B010A);

    private const int ThumbprintChars = 40;

    /// <summary>Decides from a WinVerifyTrust result and the signer thumbprint.</summary>
    /// <param name="trustResult">The WinVerifyTrust result. 0, CERT_E_UNTRUSTEDROOT and CERT_E_CHAINING mean "file not changed"; everything else refuses.</param>
    /// <param name="signerThumbprint">The SHA-1 thumbprint of the signer certificate as hex, or null when there is none.</param>
    /// <param name="pin">The pinned thumbprint, or <see cref="SigningPin.Placeholder"/>.</param>
    /// <returns>Skipped for the placeholder pin; Start when the result is acceptable and the thumbprint equals the pin (case does not matter); otherwise Refuse.</returns>
    public static SignatureVerdict Decide(int trustResult, string? signerThumbprint, string pin)
    {
        if (pin == SigningPin.Placeholder) return SignatureVerdict.Skipped;
        if (!TrustAcceptable(trustResult)) return SignatureVerdict.Refuse;
        bool shapeOk = signerThumbprint is not null && signerThumbprint.Length == ThumbprintChars && pin.Length == ThumbprintChars;
        bool same = shapeOk && string.Equals(signerThumbprint, pin, StringComparison.OrdinalIgnoreCase);
        return same ? SignatureVerdict.Start : SignatureVerdict.Refuse;
    }

    /// <summary>Tells whether a WinVerifyTrust result means "signed and not changed since".</summary>
    /// <param name="trustResult">The WinVerifyTrust result.</param>
    /// <returns>True for 0, CERT_E_UNTRUSTEDROOT and CERT_E_CHAINING.</returns>
    private static bool TrustAcceptable(int trustResult)
        => trustResult == TrustOk || trustResult == TrustUntrustedRoot || trustResult == TrustChaining;
}
