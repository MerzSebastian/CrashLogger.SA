// Small path helpers used to explain *where* crash files ended up.
#pragma once

#include <cstddef>

namespace sacl {

// True if `path` lies below a directory Windows protects from unprivileged writes
// ("...\Program Files...", "...\Program Files (x86)...", "...\Windows\...").
// Case-insensitive.
bool IsUnderProtectedDirectory(const char* path) noexcept;

// Where Windows' UAC file virtualization puts a file that a non-elevated 32-bit
// process "wrote" to a protected directory:
//   C:\Program Files (x86)\Game\x.txt  ->  <localAppData>\VirtualStore\Program Files (x86)\Game\x.txt
// Returns false if `path` has no drive prefix or the result does not fit.
bool ToVirtualStorePath(const char* path, const char* localAppData, char* out, size_t capacity) noexcept;

// Short text for the Win32 errors a crash writer realistically hits, or nullptr.
const char* DescribeWin32Error(unsigned long error) noexcept;

} // namespace sacl
