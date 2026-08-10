#pragma once

#include <cstdlib>
#include <iostream>

// Minimal always-on assertion for test executables. Deliberately not
// std::assert: that compiles out under NDEBUG, and these tests must still
// catch regressions in optimized/release-configured CI runs.
#define ASTRO_TEST_CHECK(condition)                                                        \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            std::cerr << "CHECK failed: " #condition " at " << __FILE__ << ":" << __LINE__  \
                       << '\n';                                                             \
            std::exit(1);                                                                   \
        }                                                                                    \
    } while (0)
