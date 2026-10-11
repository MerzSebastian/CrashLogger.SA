#include "streaming_monitor.hpp"

#include "game_addresses.hpp"
#include "internal_threads.hpp"
#include "safe_memory.hpp"

#include <windows.h>

namespace sacl {

namespace {

constexpr uint32_t kRingSize = 128; // power of two
static_assert((kRingSize & (kRingSize - 1)) == 0, "ring size must be a power of two");

struct Record {
    volatile uint32_t modelId;
    volatile uint32_t tick;
    volatile uint32_t state;
};

Record g_ring[kRingSize];
volatile LONG g_head = 0; // total records ever written
uint8_t g_prevState[gta::kStreamingInfoCount];
uint32_t g_intervalMs = 25;
volatile LONG g_versionOk = 0;

void Push(uint32_t id, uint8_t state, uint32_t tick) {
    LONG slot = InterlockedIncrement(&g_head) - 1;
    Record& r = g_ring[static_cast<uint32_t>(slot) & (kRingSize - 1)];
    r.modelId = id;
    r.tick = tick;
    r.state = state;
}

// One pass over the streaming info array. Returns false if memory was unreadable.
bool Scan(uint32_t tick) {
    __try {
        const volatile uint8_t* base =
            reinterpret_cast<const volatile uint8_t*>(static_cast<uintptr_t>(gta::kStreamingInfoArray));
        for (uint32_t i = 0; i < gta::kStreamingInfoCount; ++i) {
            uint8_t cur = base[i * gta::kStreamingInfoSize + gta::kStreamingInfoLoadState];
            uint8_t prev = g_prevState[i];
            if (cur != prev) {
                bool active = cur == gta::kLoadStateRequested || cur == gta::kLoadStateChannelled ||
                              cur == gta::kLoadStateFinishing;
                bool wasIdle = prev == 0 || prev == 1;
                if (active && wasIdle) Push(i, cur, tick);
                g_prevState[i] = cur;
            }
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsVersionOk() {
    uint32_t v = 0;
    return SafeRead(gta::kVersionCheckAddr, v) && v == gta::kVersionCheckValue;
}

DWORD WINAPI MonitorThread(LPVOID) {
    RegisterInternalThread(GetCurrentThreadId());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    if (!IsVersionOk()) {
        InterlockedExchange(&g_versionOk, 0);
        UnregisterInternalThread(GetCurrentThreadId());
        return 0; // wrong exe: do not poke at unknown memory
    }
    InterlockedExchange(&g_versionOk, 1);

    // Seed with the current states so the first pass does not report the whole
    // already-loaded world as "new requests".
    Scan(GetTickCount());
    for (;;) {
        Sleep(g_intervalMs);
        Scan(GetTickCount());
    }
}

} // namespace

void StartStreamingMonitor(const Config& cfg) {
    g_intervalMs = cfg.sampleIntervalMs;
    HANDLE h = CreateThread(nullptr, 0, MonitorThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

uint32_t CopyStreamingHistory(StreamEntry* out, uint32_t maxEntries) {
    LONG head = g_head;
    uint32_t total = head < 0 ? 0u : static_cast<uint32_t>(head);
    uint32_t available = total < kRingSize ? total : kRingSize;
    uint32_t count = available < maxEntries ? available : maxEntries;
    uint32_t now = GetTickCount();
    for (uint32_t i = 0; i < count; ++i) {
        const Record& r = g_ring[(total - 1 - i) & (kRingSize - 1)];
        out[i].modelId = r.modelId;
        out[i].state = static_cast<uint8_t>(r.state);
        out[i].ageMs = now - r.tick;
    }
    return count;
}

} // namespace sacl
