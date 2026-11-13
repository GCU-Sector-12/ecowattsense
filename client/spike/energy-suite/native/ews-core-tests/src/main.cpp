// main.cpp: entry point of ews-core-tests.
// Run with no argument to run all tests; the exit code is the number of failed checks (at most 255).
// Run with --live to print one line per real reader on this computer (exit code 0).
#include <cstdio>
#include <cstring>
#include <vector>

#include "live.h"
#include "test_common.h"

namespace ews_test {

namespace {

struct Entry {
    const char* name;
    void (*function)();
};

/// @return The registry, created on first use so static registrars in other files can fill it.
std::vector<Entry>& Registry() {
    static std::vector<Entry> entries;
    return entries;
}

int g_failed = 0;

}  // namespace

Registrar::Registrar(const char* name, void (*function)()) {
    Registry().push_back({name, function});
}

void Check(bool passed, const char* text, const char* file, int line) {
    if (passed) {
        return;
    }
    ++g_failed;
    std::printf("FAILED %s:%d: %s\n", file, line, text);
}

int FailedChecks() { return g_failed; }

void RunAll() {
    for (const Entry& entry : Registry()) {
        entry.function();
    }
}

}  // namespace ews_test

/// Program entry. @return The number of failed checks (capped at 255), or 0 for --live.
int main(int argc, char** argv) {
    bool live = argc > 1 && std::strcmp(argv[1], "--live") == 0;
    if (live) {
        return ews_test::RunLive();
    }
    ews_test::RunAll();
    int failed = ews_test::FailedChecks();
    std::printf("failed checks: %d\n", failed);
    return failed > 255 ? 255 : failed;
}
