// Manual/integration test host. Stands in for gta_sa.exe: loads CrashLogger.SA.asi
// and then crashes on purpose, so the whole handler pipeline can be exercised
// without the game.
//
//   sacl_crash_host.exe <mode> [path-to-asi]
//   modes: quit-fault (must NOT log)  menu-crash (must log)  av-read  av-write  av-exec  stack  illegal  div0  cpp-throw  worker-av  none
#include <windows.h>
#include <intrin.h>

#include <cstdio>
#include <cstring>

namespace {

volatile int g_zero = 0;

int Recurse(int depth) {
    volatile char pad[4096];
    pad[0] = static_cast<char>(depth);
    return Recurse(depth + 1) + pad[0];
}

void Crash(const char* mode) {
    if (!std::strcmp(mode, "av-read")) {
        volatile int* p = reinterpret_cast<volatile int*>(0x10);
        std::printf("%d\n", *p);
    } else if (!std::strcmp(mode, "av-write")) {
        *reinterpret_cast<volatile int*>(0x00800000) = 1;
    } else if (!std::strcmp(mode, "av-exec")) {
        reinterpret_cast<void (*)()>(0)();
    } else if (!std::strcmp(mode, "stack")) {
        std::printf("%d\n", Recurse(0));
    } else if (!std::strcmp(mode, "illegal")) {
        __ud2();
    } else if (!std::strcmp(mode, "div0")) {
        std::printf("%d\n", 1 / g_zero);
    } else if (!std::strcmp(mode, "cpp-throw")) {
        throw 42;
    } else if (!std::strcmp(mode, "quit-fault") || !std::strcmp(mode, "menu-crash")) {
        // quit-fault: the player chose Exit (quit=1), then something faults during teardown.
        // menu-crash: quit=0, an ordinary crash while the game (e.g. its menu) is running.
        *reinterpret_cast<volatile int*>(0xC17050) = !std::strcmp(mode, "quit-fault") ? 1 : 0;
        *reinterpret_cast<volatile int*>(0x10) = 1;
    } else if (!std::strcmp(mode, "worker-av")) {
        HANDLE t = CreateThread(nullptr, 0,
                                [](LPVOID) -> DWORD {
                                    *reinterpret_cast<volatile int*>(0x4) = 7;
                                    return 0;
                                },
                                nullptr, 0, nullptr);
        WaitForSingleObject(t, INFINITE);
    }
}

} // namespace

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "av-read";

    // Fake RsGlobal: the plugin reads an int32 at 0xC17050 (RsGlobal.quit, as in gta_sa.exe).
    // Map it first, before the heap grows over that address.
    if (!std::strcmp(mode, "quit-fault") || !std::strcmp(mode, "menu-crash")) {
        if (!VirtualAlloc(reinterpret_cast<void*>(0xC10000), 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) {
            std::printf("could not map the fake RsGlobal page (error %lu)\n", GetLastError());
            return 3;
        }
    }
    const char* asi = argc > 2 ? argv[2] : "CrashLogger.SA.asi";

    // A handled first-chance AV must NOT produce a log.
    __try {
        *reinterpret_cast<volatile int*>(0x20) = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::printf("handled AV ok\n");
    }

    HMODULE h = LoadLibraryA(asi);
    if (!h) {
        std::printf("could not load %s (error %lu)\n", asi, GetLastError());
        return 2;
    }
    std::printf("plugin loaded, giving the worker thread time to start\n");
    Sleep(500);

    __try {
        *reinterpret_cast<volatile int*>(0x24) = 1; // handled AV after the plugin is active
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::printf("handled AV after load ok\n");
    }

    std::printf("crashing: %s\n", mode);
    std::fflush(stdout);
    Crash(mode);
    std::printf("survived?!\n");
    return 0;
}
