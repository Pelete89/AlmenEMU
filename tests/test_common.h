// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

// Small assertion helpers shared by the test binaries. Counters are per process, so
// each test executable reports its own totals.
namespace tests {

inline int checks = 0;
inline int failures = 0;

inline void Check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "  FAIL: %s\n", message.c_str());
    }
}

inline void CheckEqual(long long actual, long long expected, const std::string& what) {
    Check(actual == expected,
          what + ": esperado " + std::to_string(expected) + ", obtenido " + std::to_string(actual));
}

inline void CheckNear(double actual, double expected, double tolerance, const std::string& what) {
    Check(std::fabs(actual - expected) <= tolerance,
          what + ": esperado ~" + std::to_string(expected) + ", obtenido " + std::to_string(actual));
}

inline void SetEnv(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value != nullptr ? value : "");
#else
    if (value != nullptr) {
        setenv(name, value, 1);
    } else {
        unsetenv(name);
    }
#endif
}

} // namespace tests
