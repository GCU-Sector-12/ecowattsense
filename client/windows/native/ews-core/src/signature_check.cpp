// signature_check.cpp: DecideSignature (pure) and VerifySetupSignature (WinVerifyTrust, CryptQueryObject).
#include "ews/signature_check.h"

#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <string>
#include <vector>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

namespace ews {

namespace {

constexpr std::size_t kThumbprintChars = 40;

/// @param text A thumbprint or pin.
/// @return Upper-case copy (ASCII only).
std::string Upper(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    }
    return out;
}

/// @param trustResult A WinVerifyTrust result.
/// @return True for the results that mean "signed and not changed since" (see DecideSignature).
bool TrustAcceptable(std::int32_t trustResult) {
    return trustResult == kTrustOk || trustResult == kTrustUntrustedRoot || trustResult == kTrustChaining;
}

/// Runs WinVerifyTrust on a file and closes the state again.
/// @param path The file.
/// @return The WinVerifyTrust result (0 = valid and trusted).
std::int32_t TrustResultOfFile(const wchar_t* path) {
    WINTRUST_FILE_INFO file = {};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = path;
    WINTRUST_DATA data = {};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    HWND noWindow = static_cast<HWND>(INVALID_HANDLE_VALUE);
    std::int32_t result = static_cast<std::int32_t>(WinVerifyTrust(noWindow, &action, &data));
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(noWindow, &action, &data);
    return result;
}

/// Owns the store, message and certificate that CryptQueryObject gives for a signed file.
class SignedFile {
public:
    SignedFile() = default;
    SignedFile(const SignedFile&) = delete;
    SignedFile& operator=(const SignedFile&) = delete;
    ~SignedFile() {
        if (cert_ != nullptr) {
            CertFreeCertificateContext(cert_);
        }
        if (message_ != nullptr) {
            CryptMsgClose(message_);
        }
        if (store_ != nullptr) {
            CertCloseStore(store_, 0);
        }
    }

    /// Reads the embedded signature and finds the certificate of the first signer.
    /// @param path The file.
    /// @return True when a signer certificate was found.
    bool Open(const wchar_t* path) {
        BOOL queried = CryptQueryObject(CERT_QUERY_OBJECT_FILE, path, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                        CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr, nullptr, &store_, &message_, nullptr);
        if (!queried) {
            return false;
        }
        return FindSigner();
    }

    /// @return The SHA-1 thumbprint of the signer certificate as 40 lower-case hex digits, or empty on failure.
    std::string Thumbprint() const {
        BYTE hash[20] = {};
        DWORD size = sizeof(hash);
        if (!CertGetCertificateContextProperty(cert_, CERT_HASH_PROP_ID, hash, &size) || size != sizeof(hash)) {
            return "";
        }
        static const char digits[] = "0123456789abcdef";
        std::string text;
        for (DWORD i = 0; i < size; ++i) {
            text.push_back(digits[hash[i] >> 4]);
            text.push_back(digits[hash[i] & 15]);
        }
        return text;
    }

private:
    /// Reads the signer information of the message and looks the certificate up in the store.
    /// @return True when cert_ is set.
    bool FindSigner() {
        DWORD size = 0;
        if (!CryptMsgGetParam(message_, CMSG_SIGNER_CERT_INFO_PARAM, 0, nullptr, &size) || size == 0) {
            return false;
        }
        std::vector<BYTE> buffer(size);
        if (!CryptMsgGetParam(message_, CMSG_SIGNER_CERT_INFO_PARAM, 0, buffer.data(), &size)) {
            return false;
        }
        const CERT_INFO* info = reinterpret_cast<const CERT_INFO*>(buffer.data());
        cert_ = CertFindCertificateInStore(store_, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, info, nullptr);
        return cert_ != nullptr;
    }

    HCERTSTORE store_ = nullptr;
    HCRYPTMSG message_ = nullptr;
    PCCERT_CONTEXT cert_ = nullptr;
};

}  // namespace

SignatureVerdict DecideSignature(std::int32_t trustResult, std::string_view signerThumbprint, std::string_view pin) {
    if (pin == kSigningThumbprintPlaceholder) {
        return SignatureVerdict::Skipped;
    }
    if (!TrustAcceptable(trustResult)) {
        return SignatureVerdict::Refuse;
    }
    bool shapeOk = signerThumbprint.size() == kThumbprintChars && pin.size() == kThumbprintChars;
    if (!shapeOk || Upper(signerThumbprint) != Upper(pin)) {
        return SignatureVerdict::Refuse;
    }
    return SignatureVerdict::Start;
}

SignatureVerdict VerifySetupSignature(const wchar_t* path, std::string_view pin) {
    if (pin == kSigningThumbprintPlaceholder) {
        return SignatureVerdict::Skipped;
    }
    std::int32_t trust = TrustResultOfFile(path);
    SignedFile signedFile;
    std::string thumbprint = signedFile.Open(path) ? signedFile.Thumbprint() : "";
    return DecideSignature(trust, thumbprint, pin);
}

}  // namespace ews
