// line_text.cpp: small text helpers for line based files (see ews/line_text.h).
// Used by config.cpp and configure.cpp.
#include "ews/line_text.h"

#include <algorithm>
#include <iterator>

namespace ews {

namespace {

/// @return True for a space or a tab.
bool IsBlank(char c) { return c == ' ' || c == '\t'; }

/// @return True for a digit character.
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = std::min(text.find('\n', start), text.size());
        std::string line = text.substr(start, end - start);
        bool hasCr = !line.empty() && line.back() == '\r';
        lines.push_back(hasCr ? line.substr(0, line.size() - 1) : line);
        start = end + 1;
    }
    return lines;
}

std::string TrimBlanks(const std::string& text) {
    auto first = std::find_if_not(text.begin(), text.end(), IsBlank);
    auto last = std::find_if_not(text.rbegin(), std::make_reverse_iterator(first), IsBlank).base();
    return std::string(first, last);
}

std::optional<int> ParseWholeNumber(const std::string& text) {
    bool shapeOk = !text.empty() && text.size() <= 9 && std::all_of(text.begin(), text.end(), IsDigit);
    if (!shapeOk) {
        return std::nullopt;
    }
    return std::stoi(text);
}

}  // namespace ews
