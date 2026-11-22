// signature_check.h: the Authenticode check of the extracted Setup.exe, before the web installer starts it (SPEC 13).
// Used by ews-websetup (the program) and by the tests. The decision (DecideSignature) is pure; VerifySetupSignature adds
// the Windows calls (WinVerifyTrust and the signer certificate thumbprint).
// Why: the SHA256 in latest.json comes from the same place as the package, so a signature pinned in the program is the
// second, independent proof that the package is ours.
#pragma once

#include <cstdint>
#include <string_view>

namespace ews {

/// The placeholder value of kSigningThumbprint. While the pin has this value the signature check is skipped.
inline constexpr char kSigningThumbprintPlaceholder[] = "PENDING_THUMBPRINT";

/// The pinned SHA-1 thumbprint (40 hex digits, any case) of the EcoWattSense signing certificate.
/// This is the one place of the native code that holds it.
inline constexpr char kSigningThumbprint[] = "F53EC709CE14C833AECF191E11DAB909446B6CBA";

/// WinVerifyTrust results the decision knows (HRESULT values, written here so the pure code needs no Windows header).
inline constexpr std::int32_t kTrustOk = 0;
inline constexpr std::int32_t kTrustNoSignature = static_cast<std::int32_t>(0x800B0100u);    ///< TRUST_E_NOSIGNATURE.
inline constexpr std::int32_t kTrustBadDigest = static_cast<std::int32_t>(0x80096010u);      ///< TRUST_E_BAD_DIGEST: file changed after signing.
inline constexpr std::int32_t kTrustUntrustedRoot = static_cast<std::int32_t>(0x800B0109u);  ///< CERT_E_UNTRUSTEDROOT.
inline constexpr std::int32_t kTrustChaining = static_cast<std::int32_t>(0x800B010Au);       ///< CERT_E_CHAINING.

/// What to do with the extracted Setup.exe.
enum class SignatureVerdict {
    Start,    ///< The signature is valid and the signer is ours: start it.
    Skipped,  ///< The pin is the placeholder: the check was not done, start it.
    Refuse    ///< Anything else: do not start it.
};

/// Decides from a WinVerifyTrust result and the signer thumbprint.
/// @param trustResult The WinVerifyTrust result. 0, CERT_E_UNTRUSTEDROOT and CERT_E_CHAINING count as "file not changed"
///        (the last two happen while this computer does not trust our self-signed root); everything else refuses.
/// @param signerThumbprint The SHA-1 thumbprint of the signer certificate as hex, or empty when there is none.
/// @param pin The pinned thumbprint, or kSigningThumbprintPlaceholder.
/// @return Skipped for the placeholder pin; Start when the result is acceptable and the thumbprint equals the pin (case
///         does not matter); otherwise Refuse.
SignatureVerdict DecideSignature(std::int32_t trustResult, std::string_view signerThumbprint, std::string_view pin);

/// Checks a file: WinVerifyTrust (no UI, no revocation check), then the signer thumbprint, then DecideSignature.
/// With the placeholder pin no Windows call is made.
/// @param path The file (for example Setup.exe).
/// @param pin The pinned thumbprint, or kSigningThumbprintPlaceholder.
/// @return The verdict. A file that cannot be read or has no signature gives Refuse.
SignatureVerdict VerifySetupSignature(const wchar_t* path, std::string_view pin);

}  // namespace ews
