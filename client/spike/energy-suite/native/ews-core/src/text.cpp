// text.cpp: implementation of the text helpers in ews/text.h.
#include "ews/text.h"

#include <windows.h>

#include <limits>

namespace ews {

std::string WideToUtf8(const wchar_t* text, std::size_t length) {
    bool unusable = text == nullptr || length == 0 || length > static_cast<std::size_t>((std::numeric_limits<int>::max)());
    if (unusable) {
        return std::string();
    }
    int inputLength = static_cast<int>(length);
    int needed = WideCharToMultiByte(CP_UTF8, 0, text, inputLength, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return std::string();
    }
    std::string result(static_cast<std::size_t>(needed), '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, text, inputLength, result.data(), needed, nullptr, nullptr);
    if (written != needed) {
        return std::string();
    }
    return result;
}

}  // namespace ews
