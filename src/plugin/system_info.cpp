#include "system_info.hpp"

#include "safe_memory.hpp"

#include <windows.h>
#include <intrin.h>

namespace sacl {

static_assert(sizeof(void*) == 4, "the loader structure layouts below are 32-bit only");

namespace {

// Documented-by-reverse-engineering 32-bit loader structures. Only the fields we
// read are named; layout has been stable since Windows XP.
struct UnicodeString32 {
    USHORT length; // bytes
    USHORT maximumLength;
    PWSTR buffer;
};

struct LdrDataTableEntry32 {
    LIST_ENTRY inLoadOrderLinks;
    LIST_ENTRY inMemoryOrderLinks;
    LIST_ENTRY inInitializationOrderLinks;
    PVOID dllBase;
    PVOID entryPoint;
    ULONG sizeOfImage;
    UnicodeString32 fullDllName;
    UnicodeString32 baseDllName;
};

struct PebLdrData32 {
    ULONG length;
    BOOLEAN initialized;
    PVOID ssHandle;
    LIST_ENTRY inLoadOrderModuleList;
};

// Reads the 4 part file version straight out of the mapped image's resources
// (no file I/O, no allocation).
bool ReadFileVersion(HMODULE module, char* out, size_t capacity) noexcept {
    out[0] = '\0';
    __try {
        HRSRC res = FindResourceW(module, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(16 /* RT_VERSION */));
        if (!res) return false;
        HGLOBAL global = LoadResource(module, res);
        const BYTE* data = global ? static_cast<const BYTE*>(LockResource(global)) : nullptr;
        DWORD size = SizeofResource(module, res);
        if (!data) return false;

        // VS_FIXEDFILEINFO follows a short header; find it by its signature.
        for (DWORD i = 0; i + sizeof(VS_FIXEDFILEINFO) <= size && i < 128; i += 4) {
            const VS_FIXEDFILEINFO* ffi = reinterpret_cast<const VS_FIXEDFILEINFO*>(data + i);
            if (ffi->dwSignature == 0xFEEF04BD) {
                TextBuffer tb(out, capacity);
                tb.Printf("%u.%u.%u.%u", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
                          HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
                return true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    out[0] = '\0';
    return false;
}

uint32_t ReadTimestamp(uint32_t base) noexcept {
    IMAGE_DOS_HEADER dos;
    if (!SafeRead(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS32 nt;
    if (!SafeRead(base + static_cast<uint32_t>(dos.e_lfanew), nt) || nt.Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    return nt.FileHeader.TimeDateStamp;
}

void WideToUtf8(const UnicodeString32& us, char* out, size_t capacity) noexcept {
    out[0] = '\0';
    if (!us.buffer || us.length == 0 || capacity == 0) return;
    int n = WideCharToMultiByte(CP_UTF8, 0, us.buffer, us.length / 2, out, static_cast<int>(capacity) - 1,
                                nullptr, nullptr);
    out[n > 0 ? n : 0] = '\0';
}

const char* BaseName(const char* path) noexcept {
    const char* name = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') name = p + 1;
    }
    return name;
}

void SnapshotModulesImpl(ModuleTable& table) noexcept {
    const BYTE* peb = reinterpret_cast<const BYTE*>(static_cast<uintptr_t>(__readfsdword(0x30)));
    const PebLdrData32* ldr = *reinterpret_cast<PebLdrData32* const*>(peb + 0x0C);
    if (!ldr) return;

    const LIST_ENTRY* head = &ldr->inLoadOrderModuleList;
    const HMODULE exe = GetModuleHandleW(nullptr);
    int guard = 0;
    for (const LIST_ENTRY* it = head->Flink; it && it != head && guard < 1024; it = it->Flink, ++guard) {
        const LdrDataTableEntry32* e = reinterpret_cast<const LdrDataTableEntry32*>(it);
        if (!e->dllBase) continue;

        char path[260];
        WideToUtf8(e->fullDllName, path, sizeof(path));
        char version[32];
        ReadFileVersion(static_cast<HMODULE>(e->dllBase), version, sizeof(version));

        uint32_t base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(e->dllBase));
        table.Add(base, e->sizeOfImage, ReadTimestamp(base), e->dllBase == exe, BaseName(path), path,
                  version);
    }
}

} // namespace

void SnapshotModules(ModuleTable& table) noexcept {
    table.Clear();
    __try {
        SnapshotModulesImpl(table);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // The loader list can be mid-update if another thread is loading a DLL;
        // keep whatever was collected so far.
    }
}

void QueryMemoryInfo(MemoryInfo& info) noexcept {
    SYSTEM_INFO si;
    GetSystemInfo(&si);

    uint64_t lo = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    uint64_t hi = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress) + 1ull;
    info.userSpaceTotal = hi - lo;

    uint64_t addr = lo;
    MEMORY_BASIC_INFORMATION mbi;
    while (addr < hi && VirtualQuery(reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(addr)), &mbi, sizeof(mbi)) ==
                            sizeof(mbi)) {
        uint64_t size = mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            info.addressFree += size;
            if (size > info.addressLargestFree) info.addressLargestFree = size;
        } else if (mbi.State == MEM_COMMIT) {
            info.addressCommitted += size;
        } else if (mbi.State == MEM_RESERVE) {
            info.addressReserved += size;
        }
        uint64_t next = addr + size;
        if (next <= addr) break;
        addr = next;
    }

    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        info.physicalTotal = ms.ullTotalPhys;
        info.physicalAvailable = ms.ullAvailPhys;
        info.pageFileTotal = ms.ullTotalPageFile;
        info.pageFileAvailable = ms.ullAvailPageFile;
    }
    info.valid = true;
}

void QueryOsVersion(char* out, size_t capacity) noexcept {
    TextBuffer tb(out, capacity);
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto fn = ntdll ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    OSVERSIONINFOW vi = {};
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (fn && fn(&vi) == 0) {
        tb.Printf("Windows %u.%u build %u", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber);
    } else {
        tb.Append("unknown");
    }
    if (ntdll && GetProcAddress(ntdll, "wine_get_version")) tb.Append(" (Wine)");
}

void FormatLocalTime(char* out, size_t capacity) noexcept {
    SYSTEMTIME st;
    GetLocalTime(&st);
    TextBuffer tb(out, capacity);
    tb.Printf("%04u-%02u-%02u %02u:%02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
              st.wSecond);
}

} // namespace sacl
