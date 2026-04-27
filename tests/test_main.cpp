// Minimal hand-rolled test harness — no GoogleTest dependency.
// Add cases here; each TEST_CASE registers itself via a static instance.

#include "test_harness.h"

#include <cstdio>

namespace cgs::testing {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

}

int main() {
    int passed = 0;
    int failed = 0;
    for (auto& t : cgs::testing::registry()) {
        std::printf("[ RUN      ] %s\n", t.name);
        try {
            t.fn();
            std::printf("[       OK ] %s\n", t.name);
            ++passed;
        } catch (const cgs::testing::AssertionFailure& e) {
            std::printf("[  FAILED  ] %s\n  %s\n", t.name, e.what());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[  FAILED  ] %s\n  exception: %s\n", t.name, e.what());
            ++failed;
        }
    }
    std::printf("\n=== %d passed, %d failed ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
