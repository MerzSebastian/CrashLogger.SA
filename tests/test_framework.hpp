// Tiny dependency-free test harness: TEST(name) { CHECK(...); CHECK_EQ(...); }
#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace sacl_test {

struct TestCase {
    const char* name;
    void (*fn)();
};

std::vector<TestCase>& Registry();
int& FailureCount();

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

inline bool Contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace sacl_test

#define TEST(name)                                          \
    static void test_##name();                              \
    static sacl_test::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
            ++sacl_test::FailureCount();                                                 \
        }                                                                                \
    } while (0)

#define CHECK_STR(actual, expected)                                                      \
    do {                                                                                 \
        std::string a_ = (actual);                                                       \
        std::string e_ = (expected);                                                     \
        if (a_ != e_) {                                                                  \
            std::printf("    FAILED %s:%d: %s\n      expected: \"%s\"\n      actual:   \"%s\"\n", \
                        __FILE__, __LINE__, #actual, e_.c_str(), a_.c_str());            \
            ++sacl_test::FailureCount();                                                 \
        }                                                                                \
    } while (0)

#define CHECK_CONTAINS(haystack, needle)                                                 \
    do {                                                                                 \
        std::string h_ = (haystack);                                                     \
        if (!sacl_test::Contains(h_, needle)) {                                          \
            std::printf("    FAILED %s:%d: output does not contain \"%s\"\n", __FILE__, __LINE__, needle); \
            ++sacl_test::FailureCount();                                                 \
        }                                                                                \
    } while (0)
