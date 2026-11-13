// pawnio.cpp: PawnIO detection through the registry and one device open (see ews/pawnio.h).
// Used by ews-scan. The registry key is opened read-only; the device handle is closed at once.
// The device path is used here only and never goes into a result or a file.

#include <windows.h>

#include <cstddef>

#include "ews/pawnio.h"

namespace ews {

namespace {

constexpr wchar_t kUninstallKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PawnIO";
constexpr wchar_t kVersionValue[] = L"DisplayVersion";
constexpr wchar_t kDevicePath[] = L"\\\\?\\GLOBALROOT\\Device\\PawnIO";
constexpr std::size_t kMaxVersionRead = 64;  ///< Characters read; more than the 32 allowed, so a long text stays invalid.

/// Owns an open registry key and closes it in the destructor.
class RegistryKey {
public:
    RegistryKey() = default;
    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;
    ~RegistryKey() {
        if (key_ != nullptr) {
            RegCloseKey(key_);
        }
    }

    /// Opens the PawnIO uninstall key read-only in the 64-bit view.
    /// @return The Windows error code, ERROR_SUCCESS when open.
    LONG Open() { return RegOpenKeyExW(HKEY_LOCAL_MACHINE, kUninstallKey, 0, KEY_READ | KEY_WOW64_64KEY, &key_); }
    /// @return The raw key; this object keeps ownership.
    HKEY get() const noexcept { return key_; }

private:
    HKEY key_ = nullptr;
};

/// What the registry part found.
struct RegistryFacts {
    PawnIoRegistry state = PawnIoRegistry::Unknown;
    std::optional<std::string> version;
};

/// @return One character as ASCII; every other character becomes '?'.
char AsciiOrQuestion(wchar_t c) { return (c > 0 && c < 0x80) ? static_cast<char>(c) : '?'; }

/// @return The text of a buffer of `bytes` bytes (zero-terminated or not) as ASCII; other characters become '?'.
std::string AsciiText(const wchar_t* text, DWORD bytes) {
    std::string out;
    std::size_t capacity = bytes / sizeof(wchar_t);
    for (std::size_t i = 0; i < capacity && text[i] != L'\0'; ++i) {
        out += AsciiOrQuestion(text[i]);
    }
    return out;
}

/// Reads the DisplayVersion value of an open key.
RegistryFacts ReadVersionValue(HKEY key) {
    RegistryFacts facts;
    wchar_t buffer[kMaxVersionRead + 1] = {};
    DWORD type = 0;
    DWORD bytes = static_cast<DWORD>(kMaxVersionRead * sizeof(wchar_t));
    LONG status = RegQueryValueExW(key, kVersionValue, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bytes);
    if (status == ERROR_FILE_NOT_FOUND) {
        facts.state = PawnIoRegistry::Missing;
        return facts;
    }
    if (status == ERROR_MORE_DATA) {
        // Longer than the buffer: the value exists, and its text is too long to be a version.
        facts.state = PawnIoRegistry::Found;
        facts.version = std::string(kMaxVersionRead, '?');
        return facts;
    }
    if (status != ERROR_SUCCESS) {
        return facts;
    }
    facts.state = PawnIoRegistry::Found;
    if (type != REG_SZ) {
        return facts;
    }
    std::string text = AsciiText(buffer, bytes);
    if (!text.empty()) {
        facts.version = text;
    }
    return facts;
}

/// Reads the registry part: the key, then the value.
RegistryFacts ReadRegistry() {
    RegistryKey key;
    LONG status = key.Open();
    if (status == ERROR_SUCCESS) {
        return ReadVersionValue(key.get());
    }
    RegistryFacts facts;
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        facts.state = PawnIoRegistry::Missing;
    }
    return facts;
}

/// @return The device result for a failed open with this Windows error code.
PawnIoDevice DeviceOfError(DWORD error) {
    if (error == ERROR_ACCESS_DENIED) {
        return PawnIoDevice::Denied;
    }
    bool missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    return missing ? PawnIoDevice::NotFound : PawnIoDevice::Other;
}

/// Opens the PawnIO device once and closes the handle at once.
/// Why: opening is the only test of whether this context gets access; no IOCTL is sent, so the driver does nothing.
void TestDevice(PawnIoState& state) {
    HANDLE handle = CreateFileW(kDevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        state.device = PawnIoDevice::Opened;
        state.deviceError = 0;
        return;
    }
    state.deviceError = GetLastError();
    state.device = DeviceOfError(state.deviceError);
}

}  // namespace

bool IsValidPawnIoVersion(const std::string& text) {
    if (text.empty() || text.size() > 32) {
        return false;
    }
    return text.find_first_not_of("0123456789.") == std::string::npos;
}

Reading<PawnIoState> SystemPawnIoSource::Read() {
    PawnIoState state;
    RegistryFacts facts = ReadRegistry();
    state.registry = facts.state;
    state.versionText = facts.version;
    TestDevice(state);
    return Reading<PawnIoState>::Success(state);
}

}  // namespace ews
