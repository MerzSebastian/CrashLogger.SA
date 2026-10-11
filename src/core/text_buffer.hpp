// Fixed-capacity, allocation-free text builder.
//
// Everything the crash handler formats goes through this class so that the
// handler never touches the CRT heap (which may be corrupted at crash time)
// and never depends on CRT locks or locale state.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace sacl {

class TextBuffer {
public:
    // `storage` must stay valid for the lifetime of the buffer. One byte is
    // reserved for the terminating NUL, so at most capacity-1 chars are stored.
    TextBuffer(char* storage, size_t capacity) noexcept;

    void Clear() noexcept;
    void Append(const char* s) noexcept;
    void Append(const char* s, size_t length) noexcept;
    void AppendChar(char c) noexcept;

    // Minimal printf: %s %c %d %i %u %x %X %p %f %% with flags '-', '0', '#',
    // width, precision (strings and floats) and the 'll' length modifier.
    void Printf(const char* fmt, ...) noexcept;
    void VPrintf(const char* fmt, va_list args) noexcept;

    const char* Data() const noexcept { return m_data; }
    size_t Size() const noexcept { return m_size; }
    bool Truncated() const noexcept { return m_truncated; }

private:
    void EmitPadded(const char* prefix, size_t prefixLen, const char* body, size_t bodyLen,
                    int width, bool zeroPad, bool leftAlign) noexcept;

    char* m_data;
    size_t m_capacity;
    size_t m_size;
    bool m_truncated;
};

// Formats `value` with a fixed number of decimals (0..9). Returns the number of
// characters written (excluding the NUL). Handles NaN/Inf.
size_t FormatFloat(char* out, size_t capacity, double value, int precision) noexcept;

// Bounded, always-terminating string copy.
void CopyString(char* dst, size_t capacity, const char* src) noexcept;

} // namespace sacl
