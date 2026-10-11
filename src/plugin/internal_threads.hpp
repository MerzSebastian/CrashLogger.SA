// Threads owned by the crash logger. The vectored handler ignores exceptions on
// these threads: they deliberately provoke access violations inside SEH-guarded
// reads and must never be mistaken for game crashes.
#pragma once

#include <windows.h>

namespace sacl {

namespace detail {
constexpr int kMaxInternalThreads = 4;
inline volatile LONG g_internalThreadIds[kMaxInternalThreads] = {};
} // namespace detail

inline void RegisterInternalThread(DWORD tid) noexcept {
    for (int i = 0; i < detail::kMaxInternalThreads; ++i) {
        if (InterlockedCompareExchange(&detail::g_internalThreadIds[i], static_cast<LONG>(tid), 0) == 0) return;
    }
}

inline void UnregisterInternalThread(DWORD tid) noexcept {
    for (int i = 0; i < detail::kMaxInternalThreads; ++i) {
        InterlockedCompareExchange(&detail::g_internalThreadIds[i], 0, static_cast<LONG>(tid));
    }
}

inline bool IsInternalThread(DWORD tid) noexcept {
    for (int i = 0; i < detail::kMaxInternalThreads; ++i) {
        if (static_cast<DWORD>(detail::g_internalThreadIds[i]) == tid) return true;
    }
    return false;
}

} // namespace sacl
