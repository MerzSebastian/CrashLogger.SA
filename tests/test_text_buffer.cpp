#include "test_framework.hpp"
#include "text_buffer.hpp"

#include <limits>

using sacl::TextBuffer;

namespace {
struct Fixture {
    char storage[256];
    TextBuffer buf{storage, sizeof(storage)};
    std::string str() const { return std::string(buf.Data(), buf.Size()); }
};
} // namespace

TEST(printf_hex_widths) {
    Fixture f;
    f.buf.Printf("%08X|%x|%#x|%X", 0x4087E0u, 0xabcu, 0x1fu, 0xDEADu);
    CHECK_STR(f.str(), "004087E0|abc|0x1f|DEAD");
}

TEST(printf_decimal_signed_unsigned) {
    Fixture f;
    f.buf.Printf("%d %d %u %5d|%-5d|%05d", -42, 7, 4000000000u, 12, 12, -12);
    CHECK_STR(f.str(), "-42 7 4000000000    12|12   |-0012");
}

TEST(printf_int_min_does_not_overflow) {
    Fixture f;
    f.buf.Printf("%d", static_cast<int>(-2147483647 - 1));
    CHECK_STR(f.str(), "-2147483648");
}

TEST(printf_long_long) {
    Fixture f;
    f.buf.Printf("%llx %llu", 0x1122334455667788ull, 18446744073709551615ull);
    CHECK_STR(f.str(), "1122334455667788 18446744073709551615");
}

TEST(printf_strings_and_padding) {
    Fixture f;
    f.buf.Printf("[%-6s][%6s][%.3s][%s]", "ab", "cd", "abcdef", static_cast<const char*>(nullptr));
    CHECK_STR(f.str(), "[ab    ][    cd][abc][(null)]");
}

TEST(printf_percent_and_char) {
    Fixture f;
    f.buf.Printf("100%% %c%c", 'o', 'k');
    CHECK_STR(f.str(), "100% ok");
}

TEST(printf_unknown_conversion_is_visible) {
    Fixture f;
    f.buf.Printf("%q");
    CHECK_STR(f.str(), "%q");
}

TEST(printf_floats) {
    Fixture f;
    f.buf.Printf("%.2f|%.0f|%.1f|%f|%8.3f|%-8.1f|", 3.14159, 2.4, -1.25, 1.5, 2.0, 7.0);
    CHECK_STR(f.str(), "3.14|2|-1.3|1.500000|   2.000|7.0     |");
}

TEST(printf_float_rounding_carries_into_integer_part) {
    char out[32];
    sacl::FormatFloat(out, sizeof(out), 9.999, 2);
    CHECK_STR(out, "10.00");
}

TEST(printf_float_special_values) {
    char out[32];
    const double inf = std::numeric_limits<double>::infinity();
    sacl::FormatFloat(out, sizeof(out), std::numeric_limits<double>::quiet_NaN(), 2);
    CHECK_STR(out, "nan");
    sacl::FormatFloat(out, sizeof(out), inf, 2);
    CHECK_STR(out, "inf");
    sacl::FormatFloat(out, sizeof(out), -inf, 2);
    CHECK_STR(out, "-inf");
    sacl::FormatFloat(out, sizeof(out), 1e19, 2);
    CHECK_STR(out, "huge");
}

TEST(buffer_truncates_and_stays_terminated) {
    char storage[8];
    TextBuffer buf(storage, sizeof(storage));
    buf.Append("0123456789");
    CHECK(buf.Truncated());
    CHECK(buf.Size() == 7);
    CHECK_STR(std::string(storage), "0123456");
    buf.Printf("more");
    CHECK(buf.Size() == 7);
    CHECK(storage[7] == '\0');
}

TEST(buffer_zero_capacity_is_safe) {
    TextBuffer buf(nullptr, 0);
    buf.Append("x");
    buf.Printf("%d", 5);
    CHECK(buf.Size() == 0);
    CHECK(buf.Truncated());
}

TEST(copy_string_bounds) {
    char dst[4];
    sacl::CopyString(dst, sizeof(dst), "abcdef");
    CHECK_STR(std::string(dst), "abc");
    sacl::CopyString(dst, sizeof(dst), nullptr);
    CHECK_STR(std::string(dst), "");
}
