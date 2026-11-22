// json_reader.cpp: the small JSON reader (see ews/json_reader.h).
// Used by the model parser. It is a recursive descent parser over a string_view. Every method returns false
// on bad input and never throws. Loops keep their work in small methods, so no control statement sits inside another.
#include "ews/json_reader.h"

#include <algorithm>
#include <charconv>
#include <system_error>

#include <windows.h>

namespace ews {

namespace {

/// @return True for the four JSON white space characters.
bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/// @return True for 0 to 9.
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

/// Appends one code point as UTF-8. The code point must be at most 0x10FFFF and not a surrogate.
void AppendUtf8(std::string& out, unsigned codePoint) {
    if (codePoint < 0x80) {
        out += static_cast<char>(codePoint);
        return;
    }
    if (codePoint < 0x800) {
        out += static_cast<char>(0xC0 | (codePoint >> 6));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
        return;
    }
    if (codePoint < 0x10000) {
        out += static_cast<char>(0xE0 | (codePoint >> 12));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
        return;
    }
    out += static_cast<char>(0xF0 | (codePoint >> 18));
    out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (codePoint & 0x3F));
}

/// @return The character for a simple escape letter (the letter after a backslash), or 0 when it is not one.
char SimpleEscape(char letter) {
    switch (letter) {
        case '"': return '"';
        case '\\': return '\\';
        case '/': return '/';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        default: return 0;
    }
}

/// The recursive descent parser. One object reads one document.
class Parser {
public:
    /// @param text The document. The parser keeps a view, so the text must outlive the parser.
    explicit Parser(std::string_view text) : text_(text) {}

    /// Reads the whole document: one value, then only white space.
    /// @param out Receives the value.
    /// @return True when the text is one valid JSON value.
    bool ParseDocument(JsonValue& out) {
        if (!ParseValue(out, 0)) {
            return false;
        }
        SkipSpace();
        return pos_ == text_.size();
    }

private:
    /// Skips white space.
    void SkipSpace() {
        while (pos_ < text_.size() && IsSpace(text_[pos_])) {
            ++pos_;
        }
    }

    /// Skips a run of digits.
    /// @return How many digits were skipped.
    std::size_t SkipDigits() {
        std::size_t start = pos_;
        while (pos_ < text_.size() && IsDigit(text_[pos_])) {
            ++pos_;
        }
        return pos_ - start;
    }

    /// Consumes one character when it is next.
    /// @return True when it was consumed.
    bool TryConsume(char c) {
        if (pos_ >= text_.size() || text_[pos_] != c) {
            return false;
        }
        ++pos_;
        return true;
    }

    /// Consumes a fixed text when it is next.
    /// @return True when it was consumed.
    bool ConsumeText(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) {
            return false;
        }
        pos_ += word.size();
        return true;
    }

    /// Reads one value of any kind. `depth` is the number of containers around it.
    bool ParseValue(JsonValue& out, int depth) {
        SkipSpace();
        if (pos_ >= text_.size()) {
            return false;
        }
        char c = text_[pos_];
        if (c == '{') {
            return ParseObject(out, depth);
        }
        if (c == '[') {
            return ParseArray(out, depth);
        }
        if (c == '"') {
            out.type = JsonType::String;
            return ParseString(out.text);
        }
        if (c == '-' || IsDigit(c)) {
            return ParseNumber(out);
        }
        return ParseLiteral(out);
    }

    /// Reads true, false or null.
    bool ParseLiteral(JsonValue& out) {
        if (ConsumeText("true")) {
            out.type = JsonType::Bool;
            out.boolean = true;
            return true;
        }
        if (ConsumeText("false")) {
            out.type = JsonType::Bool;
            return true;
        }
        if (ConsumeText("null")) {
            out.type = JsonType::Null;
            return true;
        }
        return false;
    }

    /// Reads the digits of the integer part: a single 0, or 1 to 9 followed by digits.
    bool ConsumeIntegerPart() {
        if (TryConsume('0')) {
            return true;
        }
        return SkipDigits() > 0;
    }

    /// Reads an optional exponent (e or E, an optional sign, digits).
    /// @return True when there is no exponent or it is complete.
    bool ConsumeExponent() {
        if (!TryConsume('e') && !TryConsume('E')) {
            return true;
        }
        if (!TryConsume('+')) {
            TryConsume('-');
        }
        return SkipDigits() > 0;
    }

    /// Reads a number by the JSON grammar and converts it. A number that does not fit a double is an error.
    bool ParseNumber(JsonValue& out) {
        std::size_t start = pos_;
        TryConsume('-');
        if (!ConsumeIntegerPart()) {
            return false;
        }
        if (TryConsume('.') && SkipDigits() == 0) {
            return false;
        }
        if (!ConsumeExponent()) {
            return false;
        }
        double value = 0;
        auto converted = std::from_chars(text_.data() + start, text_.data() + pos_, value);
        if (converted.ec != std::errc() || converted.ptr != text_.data() + pos_) {
            return false;
        }
        out.type = JsonType::Number;
        out.number = value;
        return true;
    }

    /// Reads four hex digits.
    bool ReadHex4(unsigned& value) {
        if (text_.size() - pos_ < 4) {
            return false;
        }
        const char* begin = text_.data() + pos_;
        auto converted = std::from_chars(begin, begin + 4, value, 16);
        if (converted.ec != std::errc() || converted.ptr != begin + 4) {
            return false;
        }
        pos_ += 4;
        return true;
    }

    /// Reads the part after "\u": one code point, or a surrogate pair written as two escapes.
    bool ConsumeUnicodeEscape(std::string& out) {
        unsigned first = 0;
        if (!ReadHex4(first)) {
            return false;
        }
        if (first >= 0xDC00 && first <= 0xDFFF) {
            return false;  // a low surrogate with no high surrogate before it
        }
        if (first < 0xD800 || first > 0xDBFF) {
            AppendUtf8(out, first);
            return true;
        }
        unsigned second = 0;
        if (!ConsumeText("\\u") || !ReadHex4(second)) {
            return false;
        }
        if (second < 0xDC00 || second > 0xDFFF) {
            return false;
        }
        AppendUtf8(out, 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00));
        return true;
    }

    /// Reads what follows a backslash inside a string.
    bool ConsumeEscape(std::string& out) {
        if (pos_ >= text_.size()) {
            return false;
        }
        char letter = text_[pos_++];
        if (letter == 'u') {
            return ConsumeUnicodeEscape(out);
        }
        char mapped = SimpleEscape(letter);
        if (mapped == 0) {
            return false;
        }
        out += mapped;
        return true;
    }

    /// Reads one character of a string (or one escape). A raw control character is an error.
    bool ConsumeStringChar(std::string& out) {
        char c = text_[pos_];
        if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        }
        ++pos_;
        if (c != '\\') {
            out += c;
            return true;
        }
        return ConsumeEscape(out);
    }

    /// Reads a string, including both quotes. The text is kept as it is when it is UTF-8.
    bool ParseString(std::string& out) {
        if (!TryConsume('"')) {
            return false;
        }
        bool ok = true;
        while (ok && pos_ < text_.size() && text_[pos_] != '"') {
            ok = ConsumeStringChar(out);
        }
        return ok && TryConsume('"');
    }

    /// Reads the separator after an element. Sets `more` to true after a comma. A closing mark ends the container.
    bool ConsumeSeparator(char closing, bool& more) {
        SkipSpace();
        more = TryConsume(',');
        if (more) {
            return true;
        }
        return TryConsume(closing);
    }

    /// Reads one array element and the separator after it.
    bool ParseArrayElement(JsonValue& out, int depth, bool& more) {
        JsonValue item;
        if (!ParseValue(item, depth + 1)) {
            return false;
        }
        out.items.push_back(std::move(item));
        return ConsumeSeparator(']', more);
    }

    /// Reads an array. `depth` is the number of containers around it.
    bool ParseArray(JsonValue& out, int depth) {
        if (depth >= kJsonMaxDepth) {
            return false;
        }
        out.type = JsonType::Array;
        ++pos_;  // the '['
        SkipSpace();
        if (TryConsume(']')) {
            return true;
        }
        bool ok = true;
        bool more = true;
        while (ok && more) {
            ok = ParseArrayElement(out, depth, more);
        }
        return ok;
    }

    /// Reads one object member (key, colon, value) and the separator after it.
    bool ParseObjectMember(JsonValue& out, int depth, bool& more) {
        SkipSpace();
        std::string key;
        if (!ParseString(key)) {
            return false;
        }
        SkipSpace();
        if (!TryConsume(':')) {
            return false;
        }
        JsonValue item;
        if (!ParseValue(item, depth + 1)) {
            return false;
        }
        out.keys.push_back(std::move(key));
        out.items.push_back(std::move(item));
        return ConsumeSeparator('}', more);
    }

    /// Reads an object. `depth` is the number of containers around it.
    bool ParseObject(JsonValue& out, int depth) {
        if (depth >= kJsonMaxDepth) {
            return false;
        }
        out.type = JsonType::Object;
        ++pos_;  // the '{'
        SkipSpace();
        if (TryConsume('}')) {
            return true;
        }
        bool ok = true;
        bool more = true;
        while (ok && more) {
            ok = ParseObjectMember(out, depth, more);
        }
        return ok;
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

}  // namespace

const JsonValue* JsonValue::Find(const std::string& key) const {
    if (type != JsonType::Object) {
        return nullptr;
    }
    auto found = std::find(keys.rbegin(), keys.rend(), key);  // from the end, so the last duplicate wins
    if (found == keys.rend()) {
        return nullptr;
    }
    std::size_t index = keys.size() - 1 - static_cast<std::size_t>(found - keys.rbegin());
    return &items[index];
}

Reading<JsonValue> ParseJson(std::string_view text) {
    if (text.size() > kJsonMaxTextBytes) {
        return Reading<JsonValue>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    JsonValue value;
    Parser parser(text);
    if (!parser.ParseDocument(value)) {
        return Reading<JsonValue>::Failure(Status::Error, ERROR_INVALID_DATA);
    }
    return Reading<JsonValue>::Success(std::move(value));
}

}  // namespace ews
