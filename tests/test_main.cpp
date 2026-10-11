#include "test_framework.hpp"

namespace sacl_test {

std::vector<TestCase>& Registry() {
    static std::vector<TestCase> r;
    return r;
}

int& FailureCount() {
    static int n = 0;
    return n;
}

} // namespace sacl_test

int main() {
    int failedTests = 0;
    for (const auto& t : sacl_test::Registry()) {
        int before = sacl_test::FailureCount();
        std::printf("[ RUN  ] %s\n", t.name);
        t.fn();
        if (sacl_test::FailureCount() != before) {
            ++failedTests;
            std::printf("[ FAIL ] %s\n", t.name);
        } else {
            std::printf("[  OK  ] %s\n", t.name);
        }
    }
    std::printf("\n%zu tests, %d failed\n", sacl_test::Registry().size(), failedTests);
    return failedTests == 0 ? 0 : 1;
}
