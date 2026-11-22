// test_json_reader.cpp: tests of the small JSON reader (ews/json_reader.h).
#include <string>

#include "ews/json_reader.h"
#include "test_common.h"

using ews::JsonType;
using ews::JsonValue;
using ews::ParseJson;

namespace {

/// @return `count` nested empty arrays, for example [[]] for 2.
std::string Nest(std::size_t count) { return std::string(count, '[') + std::string(count, ']'); }

/// @return A JSON unicode escape: a backslash, the letter u and the four hex digits.
std::string U(const char* hex) { return std::string("\\") + "u" + hex; }

}  // namespace

EWS_TEST(JsonNestedObject) {
    auto r = ParseJson(R"({"a": {"b": [1, true, null, "x"]}, "c": false})");
    CHECK(r.ok());
    const JsonValue* a = r.value.Find("a");
    CHECK(a != nullptr);
    const JsonValue* b = a->Find("b");
    CHECK(b != nullptr);
    CHECK(b->type == JsonType::Array);
    CHECK(b->items.size() == 4);
    CHECK(b->items[0].number == 1.0);
    CHECK(b->items[1].type == JsonType::Bool && b->items[1].boolean);
    CHECK(b->items[2].type == JsonType::Null);
    CHECK(b->items[3].text == "x");
    CHECK(r.value.Find("c")->type == JsonType::Bool && !r.value.Find("c")->boolean);
    CHECK(r.value.Find("missing") == nullptr);
}

EWS_TEST(JsonEscapes) {
    auto r = ParseJson(R"(["a\"b\\c\/d\n\t", ")" + U("0041") + U("00e9") + R"(", ")" + U("d83d") + U("de00") + R"("])");
    CHECK(r.ok());
    CHECK(r.value.items[0].text == "a\"b\\c/d\n\t");
    CHECK(r.value.items[1].text == "A\xC3\xA9");
    CHECK(r.value.items[2].text == "\xF0\x9F\x98\x80");
}

EWS_TEST(JsonBadEscapes) {
    CHECK(!ParseJson(std::string("\"") + "\\" + "x\"").ok());
    CHECK(!ParseJson("\"" + U("12") + "\"").ok());
    CHECK(!ParseJson("\"" + U("d83d") + "\"").ok());
    CHECK(!ParseJson("\"" + U("de00") + "\"").ok());
}

EWS_TEST(JsonNumbers) {
    auto r = ParseJson("[0, -1, 2.5, 1e3, 1.5E-2, -0.0]");
    CHECK(r.ok());
    CHECK(r.value.items[2].number == 2.5);
    CHECK(r.value.items[3].number == 1000.0);
    CHECK(r.value.items[4].number == 0.015);
    CHECK(!ParseJson("01").ok());
    CHECK(!ParseJson("1.").ok());
    CHECK(!ParseJson("1e").ok());
    CHECK(!ParseJson("-").ok());
    CHECK(!ParseJson("1e999").ok());
    CHECK(!ParseJson("NaN").ok());
}

EWS_TEST(JsonDepthLimit) {
    CHECK(ParseJson(Nest(16)).ok());
    CHECK(!ParseJson(Nest(17)).ok());
}

EWS_TEST(JsonTrailingText) {
    CHECK(ParseJson("{} \n").ok());
    CHECK(!ParseJson("{} x").ok());
    CHECK(!ParseJson("{}{}").ok());
    CHECK(!ParseJson("[1,]").ok());
}

EWS_TEST(JsonBrokenInput) {
    CHECK(!ParseJson("").ok());
    CHECK(!ParseJson("   ").ok());
    CHECK(!ParseJson("\"abc").ok());
    CHECK(!ParseJson("{\"a\": ").ok());
    CHECK(!ParseJson("{\"a\" 1}").ok());
    CHECK(!ParseJson("tru").ok());
}

EWS_TEST(JsonSizeLimit) {
    std::string atLimit = "1" + std::string(ews::kJsonMaxTextBytes - 1, ' ');
    std::string overLimit = "1" + std::string(ews::kJsonMaxTextBytes, ' ');
    CHECK(ParseJson(atLimit).ok());
    CHECK(!ParseJson(overLimit).ok());
}

EWS_TEST(JsonDuplicateKeyLastWins) {
    auto r = ParseJson(R"({"a": 1, "a": 2})");
    CHECK(r.ok());
    CHECK(r.value.Find("a")->number == 2.0);
}
