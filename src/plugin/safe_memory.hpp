// SEH-guarded memory reads. Every access to game memory goes through here.
#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sacl {

// Copies `size` bytes from an arbitrary address. Returns false if any part of
// the source is unreadable (the destination may be partially written).
inline bool SafeCopy(void* dst, const void* src, size_t size) noexcept {
    __try {
        std::memcpy(dst, src, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
inline bool SafeRead(uint32_t address, T& out) noexcept {
    return SafeCopy(&out, reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), sizeof(T));
}

} // namespace sacl
