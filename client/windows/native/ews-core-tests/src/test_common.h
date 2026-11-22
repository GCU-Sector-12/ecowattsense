// test_common.h: the tiny test framework of ews-core-tests (a CHECK macro and a test registry).
// Used by every test file. A test file adds cases with EWS_TEST(Name) { CHECK(...); } and needs no other edit.
#pragma once

namespace ews_test {

/// Records one check. Prints file and line when it failed.
/// @param passed The result of the condition.
/// @param text The condition as source text.
/// @param file The source file (from __FILE__).
/// @param line The source line (from __LINE__).
void Check(bool passed, const char* text, const char* file, int line);

/// @return The number of checks that failed so far.
int FailedChecks();

/// Adds a test function to the registry. Called by EWS_TEST during static initialisation.
struct Registrar {
    Registrar(const char* name, void (*function)());
};

/// Runs every registered test in registration order.
void RunAll();

}  // namespace ews_test

#define CHECK(condition) ::ews_test::Check((condition), #condition, __FILE__, __LINE__)

#define EWS_TEST(name)                                          \
    static void name();                                         \
    static const ::ews_test::Registrar registrar_##name(#name, name); \
    static void name()
