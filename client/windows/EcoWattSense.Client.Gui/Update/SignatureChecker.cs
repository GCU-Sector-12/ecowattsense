// SignatureChecker.cs: checks the Authenticode signature of the extracted Setup.exe before the update starts it (SPEC 9).
// Who uses it: MainWindow.Update.cs, after the ZIP is unpacked. WinVerifyTrust (wintrust.dll) gives the trust result;
// X509Certificate.CreateFromSignedFile (it calls CryptQueryObject) gives the signer certificate, whose SHA-1 thumbprint
// is compared with the pin. The decision itself is in SignatureRules.cs.
#nullable enable
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;

namespace EcoWattSense.Client.Gui.Update;

/// <summary>Runs the signature check on a file.</summary>
public static class SignatureChecker
{
    private const uint UiNone = 2;        // WTD_UI_NONE
    private const uint RevokeNone = 0;    // WTD_REVOKE_NONE
    private const uint ChoiceFile = 1;    // WTD_CHOICE_FILE
    private const uint ActionVerify = 1;  // WTD_STATEACTION_VERIFY
    private const uint ActionClose = 2;   // WTD_STATEACTION_CLOSE

    /// <summary>WINTRUST_ACTION_GENERIC_VERIFY_V2.</summary>
    private static readonly Guid GenericVerifyV2 = new("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");

    [StructLayout(LayoutKind.Sequential)]
    private struct WinTrustFileInfo
    {
        public uint StructSize;
        public IntPtr FilePath;
        public IntPtr FileHandle;
        public IntPtr KnownSubject;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct WinTrustData
    {
        public uint StructSize;
        public IntPtr PolicyCallbackData;
        public IntPtr SipClientData;
        public uint UiChoice;
        public uint RevocationChecks;
        public uint UnionChoice;
        public IntPtr FileInfo;
        public uint StateAction;
        public IntPtr StateData;
        public IntPtr UrlReference;
        public uint ProvFlags;
        public uint UiContext;
        public IntPtr SignatureSettings;
    }

    [DllImport("wintrust.dll", ExactSpelling = true)]
    private static extern int WinVerifyTrust(IntPtr window, ref Guid action, ref WinTrustData data);

    /// <summary>Checks a file against the pinned thumbprint of <see cref="SigningPin"/>.</summary>
    /// <param name="path">The full path of Setup.exe.</param>
    /// <returns>The verdict. A file that cannot be read or has no signature gives Refuse. With the placeholder pin no check is made, Skipped is returned and "signature_check_skipped" is traced.</returns>
    public static SignatureVerdict Check(string path)
    {
        SignatureVerdict verdict = Check(path, SigningPin.Thumbprint);
        if (verdict == SignatureVerdict.Skipped) Trace.WriteLine("signature_check_skipped");
        return verdict;
    }

    /// <summary>Checks a file against a given pin.</summary>
    /// <param name="path">The full path of the file.</param>
    /// <param name="pin">The pinned thumbprint, or the placeholder.</param>
    /// <returns>The verdict of <see cref="SignatureRules.Decide"/>.</returns>
    public static SignatureVerdict Check(string path, string pin)
    {
        if (pin == SigningPin.Placeholder) return SignatureVerdict.Skipped;
        int trust = TrustResultOf(path);
        return SignatureRules.Decide(trust, SignerThumbprintOf(path), pin);
    }

    /// <summary>Runs WinVerifyTrust (no UI, no revocation check) and closes its state.</summary>
    /// <param name="path">The file.</param>
    /// <returns>The WinVerifyTrust result; 0 is valid and trusted.</returns>
    private static int TrustResultOf(string path)
    {
        IntPtr text = Marshal.StringToCoTaskMemUni(path);
        var file = new WinTrustFileInfo { StructSize = (uint)Marshal.SizeOf<WinTrustFileInfo>(), FilePath = text };
        IntPtr filePointer = Marshal.AllocHGlobal(Marshal.SizeOf<WinTrustFileInfo>());
        Marshal.StructureToPtr(file, filePointer, false);
        var data = new WinTrustData
        {
            StructSize = (uint)Marshal.SizeOf<WinTrustData>(),
            UiChoice = UiNone,
            RevocationChecks = RevokeNone,
            UnionChoice = ChoiceFile,
            FileInfo = filePointer,
            StateAction = ActionVerify
        };
        Guid action = GenericVerifyV2;
        int result = WinVerifyTrust(new IntPtr(-1), ref action, ref data);
        data.StateAction = ActionClose;
        WinVerifyTrust(new IntPtr(-1), ref action, ref data);
        Marshal.FreeHGlobal(filePointer);
        Marshal.FreeCoTaskMem(text);
        return result;
    }

    /// <summary>Reads the SHA-1 thumbprint of the signer certificate of a signed file.</summary>
    /// <param name="path">The file.</param>
    /// <returns>40 hex digits, or null when the file has no readable signature.</returns>
    private static string? SignerThumbprintOf(string path)
    {
        try
        {
            // X509CertificateLoader has no method for a signed file; CreateFromSignedFile is the system call (CryptQueryObject)
            // for exactly this and is not deprecated for this use, only its sibling constructors are.
#pragma warning disable SYSLIB0057
            using X509Certificate certificate = X509Certificate.CreateFromSignedFile(path);
#pragma warning restore SYSLIB0057
            return certificate.GetCertHashString();
        }
        catch (CryptographicException) { return null; }
    }
}
