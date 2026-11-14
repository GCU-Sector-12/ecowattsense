// main.cpp: EcoWattSense-Web-Setup.exe, the small web installer (SPEC 13).
// What it does: shows a TaskDialog "EcoWattSense setup" with a progress bar and one status line, and runs the steps of steps.cpp
// on a worker thread: manifest, download, SHA256 check, tar.exe extraction, start of Setup.exe (which asks for UAC itself).
// Who runs it: the user, by double click. It needs no administrator rights (asInvoker).
// Cancel: sets a flag; the worker stops at the next chunk (a read waits at most 10 s), then the program ends with exit code 1.
// Exit codes (ews::WebSetupExitCode): 0 Setup.exe started, 1 cancelled, 2 manifest download failed, 3 manifest invalid,
//   4 temporary folder failed, 5 package download failed, 6 size differs from the manifest, 7 SHA256 differs, 8 SHA256 could not be
//   computed, 9 tar.exe failed, 10 Setup.exe missing in the package, 11 Setup.exe not started (UAC cancelled), 12 worker thread failed.
// A failure shows one clear line in a second dialog; it holds no address and no raw error text.
#include <windows.h>
#include <commctrl.h>

#include <atomic>

#include "ews/handles.h"
#include "ews/web_setup.h"
#include "steps.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

constexpr wchar_t kTitle[] = L"EcoWattSense setup";

/// The dialog state shared by the dialog thread and the worker. It is also the SetupHost of the steps.
class Installer : public websetup::SetupHost {
public:
    /// @return The flag set by the Cancel button.
    const std::atomic<bool>& CancelFlag() const override { return cancel_; }

    /// Updates the status line and the progress bar (SendMessage to the dialog, so the text stays valid during the call).
    /// @param line The status text.
    /// @param percent 0 to 100.
    void Report(const wchar_t* line, int percent) override {
        SendMessageW(window_, TDM_UPDATE_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(line));
        SendMessageW(window_, TDM_SET_PROGRESS_BAR_POS, static_cast<WPARAM>(percent), 0);
    }

    /// Sets the Cancel flag. Called from the dialog thread.
    void RequestCancel() { cancel_.store(true); }

    /// Starts the worker thread. Called once, when the dialog exists.
    /// @param window The dialog window.
    void Start(HWND window) {
        window_ = window;
        SendMessageW(window_, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, 100));
        worker_.Reset(CreateThread(nullptr, 0, WorkerMain, this, 0, nullptr));
        if (!worker_.valid()) {
            result_ = ews::WebSetupResult::Internal;
            PostMessageW(window_, TDM_CLICK_BUTTON, IDCANCEL, 0);
        }
    }

    /// Waits for the worker to end.
    /// @return The result of the installer.
    ews::WebSetupResult Join() {
        if (worker_.valid()) {
            WaitForSingleObject(worker_.get(), INFINITE);
        }
        return result_;
    }

private:
    /// Thread entry: runs the steps, then closes the dialog.
    /// @param parameter The Installer.
    /// @return Always 0.
    static DWORD WINAPI WorkerMain(LPVOID parameter) {
        Installer* self = static_cast<Installer*>(parameter);
        self->result_ = websetup::RunWebSetup(*self);
        PostMessageW(self->window_, TDM_CLICK_BUTTON, IDCANCEL, 0);
        return 0;
    }

    std::atomic<bool> cancel_{false};
    HWND window_ = nullptr;
    ews::UniqueHandle worker_;
    ews::WebSetupResult result_ = ews::WebSetupResult::Internal;
};

/// The TaskDialog callback: starts the worker when the dialog is created and sets the cancel flag on any close request.
/// When the worker ends it clicks the same button; the flag is then irrelevant because the result comes from the worker.
/// @param window The dialog.
/// @param notification A TDN_ code.
/// @param data The Installer.
/// @return S_OK, so the dialog may close.
HRESULT CALLBACK DialogCallback(HWND window, UINT notification, WPARAM, LPARAM, LONG_PTR data) {
    Installer* installer = reinterpret_cast<Installer*>(data);
    if (notification == TDN_CREATED) {
        installer->Start(window);
    }
    if (notification == TDN_BUTTON_CLICKED) {
        installer->RequestCancel();
    }
    return S_OK;
}

/// Shows the dialog and runs the installer.
/// @param installer The shared state.
/// @return True when the dialog was shown.
bool RunDialog(Installer& installer) {
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_ALLOW_DIALOG_CANCELLATION;
    config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
    config.pszWindowTitle = kTitle;
    config.pszMainInstruction = L"Installing EcoWattSense";
    config.pszContent = L"Starting...";
    config.pfCallback = DialogCallback;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(&installer);
    int button = 0;
    return SUCCEEDED(TaskDialogIndirect(&config, &button, nullptr, nullptr));
}

/// Shows the one-line failure message.
/// @param result The failed result.
void ShowFailure(ews::WebSetupResult result) {
    TaskDialog(nullptr, nullptr, kTitle, L"Setup stopped", ews::WebSetupMessage(result), TDCBF_CLOSE_BUTTON, TD_ERROR_ICON, nullptr);
}

}  // namespace

/// Program entry.
/// @return The exit code from ews::WebSetupExitCode.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Installer installer;
    bool shown = RunDialog(installer);
    ews::WebSetupResult result = shown ? installer.Join() : ews::WebSetupResult::Internal;
    bool silent = result == ews::WebSetupResult::Ok || result == ews::WebSetupResult::Cancelled;
    if (!silent) {
        ShowFailure(result);
    }
    return ews::WebSetupExitCode(result);
}
