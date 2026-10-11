#include "text_buffer.hpp"

#include <cmath>
#include <cstring>

namespace sacl {

namespace {

size_t UnsignedToText(char* out, uint64_t value, unsigned base, bool upper) noexcept {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    size_t n = 0;
    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value != 0);
    for (size_t i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
    return n;
}

} // namespace

size_t FormatFloat(char* out, size_t capacity, double value, int precision) noexcept {
    if (capacity == 0) return 0;
    char tmp[64];
    size_t n = 0;

    if (precision < 0) precision = 0;
    if (precision > 9) precision = 9;

    if (std::isnan(value)) {
        std::memcpy(tmp, "nan", 3);
        n = 3;
    } else if (std::isinf(value)) {
        const char* s = value < 0 ? "-inf" : "inf";
        n = std::strlen(s);
        std::memcpy(tmp, s, n);
    } else if (value >= 1e18 || value <= -1e18) {
        const char* s = value < 0 ? "-huge" : "huge";
        n = std::strlen(s);
        std::memcpy(tmp, s, n);
    } else {
        if (value < 0) {
            tmp[n++] = '-';
            value = -value;
        }
        uint64_t scale = 1;
        for (int i = 0; i < precision; ++i) scale *= 10;
        uint64_t whole = static_cast<uint64_t>(value);
        double frac = value - static_cast<double>(whole);
        uint64_t fracDigits = static_cast<uint64_t>(frac * static_cast<double>(scale) + 0.5);
        if (fracDigits >= scale) {
            ++whole;
            fracDigits -= scale;
        }
        n += UnsignedToText(tmp + n, whole, 10, false);
        if (precision > 0) {
            tmp[n++] = '.';
            char digits[24];
            size_t dn = UnsignedToText(digits, fracDigits, 10, false);
            for (size_t i = dn; i < static_cast<size_t>(precision); ++i) tmp[n++] = '0';
            std::memcpy(tmp + n, digits, dn);
            n += dn;
        }
    }

    if (n > capacity - 1) n = capacity - 1;
    std::memcpy(out, tmp, n);
    out[n] = '\0';
    return n;
}

void CopyString(char* dst, size_t capacity, const char* src) noexcept {
    if (capacity == 0) return;
    size_t i = 0;
    if (src) {
        for (; i + 1 < capacity && src[i] != '\0'; ++i) dst[i] = src[i];
    }
    dst[i] = '\0';
}

TextBuffer::TextBuffer(char* storage, size_t capacity) noexcept
    : m_data(storage), m_capacity(capacity), m_size(0), m_truncated(false) {
    if (m_capacity) m_data[0] = '\0';
}

void TextBuffer::Clear() noexcept {
    m_size = 0;
    m_truncated = false;
    if (m_capacity) m_data[0] = '\0';
}

void TextBuffer::Append(const char* s, size_t length) noexcept {
    if (!s || length == 0) return;
    if (m_capacity == 0) {
        m_truncated = true;
        return;
    }
    size_t room = m_capacity - 1 - m_size;
    if (length > room) {
        length = room;
        m_truncated = true;
    }
    std::memcpy(m_data + m_size, s, length);
    m_size += length;
    m_data[m_size] = '\0';
}

void TextBuffer::Append(const char* s) noexcept {
    if (s) Append(s, std::strlen(s));
}

void TextBuffer::AppendChar(char c) noexcept {
    Append(&c, 1);
}

void TextBuffer::Printf(const char* fmt, ...) noexcept {
    va_list args;
    va_start(args, fmt);
    VPrintf(fmt, args);
    va_end(args);
}

void TextBuffer::EmitPadded(const char* prefix, size_t prefixLen, const char* body, size_t bodyLen,
                            int width, bool zeroPad, bool leftAlign) noexcept {
    size_t total = prefixLen + bodyLen;
    size_t pad = (width > 0 && static_cast<size_t>(width) > total) ? static_cast<size_t>(width) - total : 0;
    if (leftAlign) {
        Append(prefix, prefixLen);
        Append(body, bodyLen);
        for (size_t i = 0; i < pad; ++i) AppendChar(' ');
    } else if (zeroPad) {
        Append(prefix, prefixLen);
        for (size_t i = 0; i < pad; ++i) AppendChar('0');
        Append(body, bodyLen);
    } else {
        for (size_t i = 0; i < pad; ++i) AppendChar(' ');
        Append(prefix, prefixLen);
        Append(body, bodyLen);
    }
}

void TextBuffer::VPrintf(const char* fmt, va_list args) noexcept {
    if (!fmt) return;
    while (*fmt) {
        char c = *fmt++;
        if (c != '%') {
            AppendChar(c);
            continue;
        }

        bool zero = false, left = false, alt = false;
        for (;; ++fmt) {
            if (*fmt == '0') zero = true;
            else if (*fmt == '-') left = true;
            else if (*fmt == '#') alt = true;
            else break;
        }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        int precision = -1;
        if (*fmt == '.') {
            ++fmt;
            precision = 0;
            while (*fmt >= '0' && *fmt <= '9') precision = precision * 10 + (*fmt++ - '0');
        }
        bool isLongLong = false;
        if (*fmt == 'l') {
            ++fmt;
            if (*fmt == 'l') {
                ++fmt;
                isLongLong = true;
            }
        }

        char spec = *fmt;
        if (spec == '\0') break;
        ++fmt;

        char num[32];
        switch (spec) {
        case '%':
            AppendChar('%');
            break;
        case 'c':
            AppendChar(static_cast<char>(va_arg(args, int)));
            break;
        case 's': {
            const char* s = va_arg(args, const char*);
            if (!s) s = "(null)";
            size_t len = 0;
            while (s[len] && (precision < 0 || len < static_cast<size_t>(precision))) ++len;
            EmitPadded("", 0, s, len, width, false, left);
            break;
        }
        case 'd':
        case 'i': {
            long long v = isLongLong ? va_arg(args, long long) : static_cast<long long>(va_arg(args, int));
            const char* sign = "";
            uint64_t mag;
            if (v < 0) {
                sign = "-";
                mag = 0 - static_cast<uint64_t>(v);
            } else {
                mag = static_cast<uint64_t>(v);
            }
            size_t n = UnsignedToText(num, mag, 10, false);
            EmitPadded(sign, sign[0] ? 1 : 0, num, n, width, zero && !left, left);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            uint64_t v = isLongLong ? va_arg(args, unsigned long long)
                                    : static_cast<uint64_t>(va_arg(args, unsigned int));
            unsigned base = spec == 'u' ? 10u : 16u;
            size_t n = UnsignedToText(num, v, base, spec == 'X');
            const char* prefix = "";
            if (alt && base == 16) prefix = spec == 'X' ? "0X" : "0x";
            EmitPadded(prefix, prefix[0] ? 2 : 0, num, n, width, zero && !left, left);
            break;
        }
        case 'p': {
            uintptr_t v = reinterpret_cast<uintptr_t>(va_arg(args, void*));
            size_t n = UnsignedToText(num, v, 16, true);
            EmitPadded("0x", 2, num, n, static_cast<int>(sizeof(void*) * 2 + 2), true, false);
            break;
        }
        case 'f': {
            double v = va_arg(args, double);
            size_t n = FormatFloat(num, sizeof(num), v, precision < 0 ? 6 : precision);
            EmitPadded("", 0, num, n, width, zero && !left, left);
            break;
        }
        default:
            // Unknown conversion: emit it verbatim so mistakes are visible.
            AppendChar('%');
            AppendChar(spec);
            break;
        }
    }
}

} // namespace sacl
