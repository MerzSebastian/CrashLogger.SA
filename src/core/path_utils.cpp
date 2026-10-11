#include "path_utils.hpp"

#include "text_buffer.hpp"

#include <cstring>

namespace sacl {

namespace {

char Lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool ContainsNoCase(const char* haystack, const char* needle) noexcept {
    size_t n = std::strlen(needle);
    for (const char* h = haystack; *h; ++h) {
        size_t i = 0;
        while (i < n && h[i] && Lower(h[i]) == Lower(needle[i])) ++i;
        if (i == n) return true;
    }
    return false;
}

} // namespace

bool IsUnderProtectedDirectory(const char* path) noexcept {
    if (!path) return false;
    return ContainsNoCase(path, "\\program files") || ContainsNoCase(path, "\\windows\\");
}

bool ToVirtualStorePath(const char* path, const char* localAppData, char* out, size_t capacity) noexcept {
    if (!out || capacity == 0) return false;
    out[0] = '\0';
    if (!path || !localAppData) return false;

    // Need "X:\..." - virtualization only applies to local fixed drives.
    bool hasDrive = ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
                    path[1] == ':' && (path[2] == '\\' || path[2] == '/');
    if (!hasDrive) return false;

    TextBuffer tb(out, capacity);
    tb.Append(localAppData);
    size_t len = std::strlen(localAppData);
    if (len > 0 && localAppData[len - 1] != '\\') tb.AppendChar('\\');
    tb.Append("VirtualStore");
    tb.Append(path + 2); // keeps the leading backslash
    if (tb.Truncated()) {
        out[0] = '\0';
        return false;
    }
    return true;
}

const char* DescribeWin32Error(unsigned long error) noexcept {
    switch (error) {
    case 2: return "file not found";
    case 3: return "path not found";
    case 5: return "access denied";
    case 19: return "write protected";
    case 32: return "file in use by another process";
    case 39:
    case 112: return "disk full";
    case 123: return "invalid file name";
    default: return nullptr;
    }
}

} // namespace sacl
