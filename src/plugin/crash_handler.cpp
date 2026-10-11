// Crash handler.
//
// Design notes
// ------------
// * Two entry points, as requested: a vectored exception handler (VEH) and an
//   unhandled-exception filter (UEF).
//
//   A VEH sees every first-chance exception, including the countless ones that
//   the game or other mods handle with SEH. Writing a "crash log" for each of
//   those would be wrong, so the VEH's main job is different: on any fatal-looking
//   exception it re-installs OUR filter as the top-level filter. If another plugin
//   called SetUnhandledExceptionFilter after us, the VEH runs before that plugin's
//   filter ever could and takes the slot back (the replaced filter is remembered
//   and chained to after we have logged). The log itself is written by the UEF,
//   i.e. only when nothing handled the exception.
//   Optional ini switch LogFirstChance=1 additionally logs fatal-looking
//   first-chance exceptions straight from the VEH (for crashes that are swallowed
//   by someone's __except or that kill the process without reaching a filter).
//
// * The report is NOT built on the faulting thread. On a stack overflow that
//   thread has a few KB of stack left, and its heap may be corrupted. A dedicated
//   worker thread, created at startup with its own stack and statically
//   allocated buffers, builds the report while the faulting thread waits.
//   If the worker is not up yet (crash during startup) the report is built inline.
//
// * Nothing on the crash path allocates from the CRT heap. Text goes through
//   TextBuffer into static storage, files are written with CreateFile/WriteFile,
//   modules are read from the PEB. Each stage is wrapped in its own SEH guard.
#include "crash_handler.hpp"

#include "../core/path_utils.hpp"
#include "../core/report.hpp"
#include "../core/stack_scan.hpp"
#include "config.hpp"
#include "game_addresses.hpp"
#include "game_reader.hpp"
#include "internal_threads.hpp"
#include "safe_memory.hpp"
#include "streaming_monitor.hpp"
#include "system_info.hpp"

#include <dbghelp.h>
#include <shlobj.h>

namespace sacl {

namespace {

enum class Source { UnhandledFilter, FirstChance };

constexpr DWORD kWorkerTimeoutMs = 120 * 1000;
constexpr size_t kReportCapacity = 384 * 1024;
constexpr uint32_t kStackScanBytes = 8 * 1024;
constexpr LONG kMaxFirstChanceLogs = 3;

enum : LONG { kIdle = 0, kBusy = 1, kFinished = 2 };

struct Request {
    EXCEPTION_POINTERS* pointers = nullptr;
    DWORD threadId = 0;
    Source source = Source::UnhandledFilter;
};

// ---- Everything below is static: no allocation on the crash path. ----
Config g_config;
SymbolTable g_symbols;
ModuleTable g_modules;
CrashInfo g_info;
char g_reportStorage[kReportCapacity];
uint8_t g_stackCopy[kStackScanBytes];
EXCEPTION_RECORD g_recordCopy;
CONTEXT g_contextCopy;

// Output directories, tried in this order until a file can be created:
// the game folder, the game's "GTA San Andreas User Files" folder, %TEMP%.
constexpr int kMaxOutputDirs = 3;
char g_outputDirs[kMaxOutputDirs][MAX_PATH];
char g_exePath[MAX_PATH];

HINSTANCE g_self;
DWORD g_mainThreadId;
Request g_request;
HANDLE g_requestEvent;
HANDLE g_doneEvent;
volatile LONG g_state = kIdle;
volatile LONG g_workerReady = 0;
volatile LONG g_symbolsReady = 0;
volatile LONG g_firstChanceLogs = 0;
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                          PMINIDUMP_EXCEPTION_INFORMATION,
                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
MiniDumpWriteDumpFn g_miniDumpWriteDump = nullptr;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// Directory (with trailing backslash) of the module's file.
void DirectoryOf(const char* path, char* out, size_t capacity) {
    CopyString(out, capacity, path);
    char* last = nullptr;
    for (char* p = out; *p; ++p) {
        if (*p == '\\' || *p == '/') last = p;
    }
    if (last) last[1] = '\0';
    else out[0] = '\0';
}

void JoinPath(char* out, size_t capacity, const char* dir, const char* name) {
    TextBuffer tb(out, capacity);
    tb.Append(dir);
    tb.Append(name);
}

bool WriteAll(HANDLE h, const void* data, size_t size) {
    const char* p = static_cast<const char*>(data);
    while (size > 0) {
        DWORD chunk = size > 0x100000 ? 0x100000 : static_cast<DWORD>(size);
        DWORD written = 0;
        if (!WriteFile(h, p, chunk, &written, nullptr) || written == 0) return false;
        p += written;
        size -= written;
    }
    return true;
}

bool AppendToFile(const char* path, const void* data, size_t size) {
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = WriteAll(h, data, size);
    CloseHandle(h);
    return ok;
}

// What happened when trying to place the output files; shown to the user.
struct OutputOutcome {
    DWORD firstError = 0; // error from the first directory that failed
    char firstFailedPath[MAX_PATH] = {};
    bool fellBack = false; // the first directory did not work
};
OutputOutcome g_outcome;

struct OutputFile {
    HANDLE handle = INVALID_HANDLE_VALUE;
    char path[MAX_PATH] = {};
};

// Creates `fileName` in the first output directory that accepts it.
bool OpenOutputFile(const char* fileName, OutputFile& file) {
    bool first = true;
    for (int i = 0; i < kMaxOutputDirs; ++i) {
        if (!g_outputDirs[i][0]) continue;
        JoinPath(file.path, sizeof(file.path), g_outputDirs[i], fileName);
        HANDLE h = CreateFileA(file.path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            file.handle = h;
            return true;
        }
        if (first && g_outcome.firstError == 0) {
            g_outcome.firstError = GetLastError();
            CopyString(g_outcome.firstFailedPath, sizeof(g_outcome.firstFailedPath), file.path);
        }
        g_outcome.fellBack = true;
        first = false;
    }
    file.path[0] = '\0';
    return false;
}

// True if Windows would silently redirect writes below protected folders (UAC file
// virtualization for non-elevated legacy 32-bit processes).
bool IsFileVirtualizationActive() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    DWORD enabled = 0, returned = 0;
    constexpr int kTokenVirtualizationEnabled = 24;
    BOOL ok = GetTokenInformation(token, static_cast<TOKEN_INFORMATION_CLASS>(kTokenVirtualizationEnabled),
                                  &enabled, sizeof(enabled), &returned);
    CloseHandle(token);
    return ok && enabled != 0;
}

bool StackShowsProcessExit(const EXCEPTION_POINTERS* ep) noexcept;

// Start addresses of the OS functions that run during process exit. A return address
// into one of them on the faulting thread's stack means "this fault happened while
// the process was already exiting".
constexpr int kMaxExitFunctions = 4;
constexpr uint32_t kExitFunctionSpan = 0x300;
uint32_t g_exitFunctions[kMaxExitFunctions];
int g_exitFunctionCount = 0;

void AddExitFunction(const char* module, const char* name) {
    HMODULE m = GetModuleHandleA(module);
    FARPROC p = m ? GetProcAddress(m, name) : nullptr;
    if (p && g_exitFunctionCount < kMaxExitFunctions) {
        g_exitFunctions[g_exitFunctionCount++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
    }
}

template <typename F>
bool Stage(F&& fn) noexcept {
    __try {
        fn();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Report assembly
// ---------------------------------------------------------------------------

void FillExceptionAndRegisters(CrashInfo& ci, const EXCEPTION_POINTERS* ep) {
    if (!SafeRead(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ep->ExceptionRecord)), g_recordCopy)) {
        return;
    }
    ExceptionData& e = ci.exception;
    e.code = g_recordCopy.ExceptionCode;
    e.flags = g_recordCopy.ExceptionFlags;
    e.address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_recordCopy.ExceptionAddress));
    e.numParams = g_recordCopy.NumberParameters < 15 ? g_recordCopy.NumberParameters : 15;
    for (uint32_t i = 0; i < e.numParams; ++i) {
        e.params[i] = static_cast<uint32_t>(g_recordCopy.ExceptionInformation[i]);
    }

    if (!SafeRead(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ep->ContextRecord)), g_contextCopy)) {
        return;
    }
    const CONTEXT& c = g_contextCopy;
    Registers& r = ci.regs;
    r.eax = c.Eax; r.ebx = c.Ebx; r.ecx = c.Ecx; r.edx = c.Edx;
    r.esi = c.Esi; r.edi = c.Edi; r.ebp = c.Ebp; r.esp = c.Esp;
    r.eip = c.Eip; r.eflags = c.EFlags;
    r.cs = static_cast<uint16_t>(c.SegCs); r.ds = static_cast<uint16_t>(c.SegDs);
    r.es = static_cast<uint16_t>(c.SegEs); r.fs = static_cast<uint16_t>(c.SegFs);
    r.gs = static_cast<uint16_t>(c.SegGs); r.ss = static_cast<uint16_t>(c.SegSs);
}

void CollectCodeBytes(CrashInfo& ci) {
    ci.codeBytesValid = SafeCopy(ci.codeBytes, reinterpret_cast<const void*>(static_cast<uintptr_t>(ci.regs.eip)),
                                 sizeof(ci.codeBytes));
}

void CollectStack(CrashInfo& ci) {
    const uint32_t esp = ci.regs.esp;

    // Copy page by page so a stack that ends before kStackScanBytes still yields
    // everything that is readable.
    uint32_t got = 0;
    while (got < kStackScanBytes) {
        uint32_t addr = esp + got;
        uint32_t toPageEnd = 0x1000 - (addr & 0xFFF);
        uint32_t chunk = kStackScanBytes - got < toPageEnd ? kStackScanBytes - got : toPageEnd;
        if (!SafeCopy(g_stackCopy + got, reinterpret_cast<const void*>(static_cast<uintptr_t>(addr)), chunk)) break;
        got += chunk;
    }

    const uint32_t* words = reinterpret_cast<const uint32_t*>(g_stackCopy);
    const uint32_t wordCount = got / 4;

    ci.stackDumpCount = wordCount < kMaxStackDump ? wordCount : kMaxStackDump;
    for (uint32_t i = 0; i < ci.stackDumpCount; ++i) ci.stackDump[i] = words[i];

    for (uint32_t i = 0; i < wordCount && ci.frameCount < kMaxFrames; ++i) {
        uint32_t candidate = words[i];
        if (!g_modules.Find(candidate)) continue;
        uint8_t prev[8];
        if (!SafeCopy(prev, reinterpret_cast<const void*>(static_cast<uintptr_t>(candidate - 8)), sizeof(prev))) {
            continue;
        }
        if (!LooksLikeCallReturn(prev)) continue;
        uint32_t callTarget = 0;
        if (DirectCallTarget(prev, candidate, &callTarget) && !g_modules.Find(callTarget)) {
            continue; // "call rel32" into unmapped memory: a stale stack word, not a return address
        }
        ci.frames[ci.frameCount].address = candidate;
        ci.frames[ci.frameCount].stackOffset = i * 4;
        ++ci.frameCount;
    }
}

void BuildReport(const Request& req, TextBuffer& out) {
    CrashInfo& ci = g_info;
    ci = CrashInfo(); // static storage: this is a plain re-initialisation, no allocation

    ci.source = req.source == Source::UnhandledFilter
                    ? "unhandled exception filter (the exception was not handled by anyone)"
                    : "vectored handler, first chance (LogFirstChance=1; may have been handled)";
    CopyString(ci.exePath, sizeof(ci.exePath), g_exePath);
    ci.faultingThreadId = req.threadId;
    ci.mainThreadId = g_mainThreadId;
    ci.streamingHistoryLimit = g_config.historySize;
    ci.symbolMaxGap = g_config.symbolMaxGap;

    Stage([&] { FormatLocalTime(ci.timestamp, sizeof(ci.timestamp)); });
    Stage([&] { QueryOsVersion(ci.osVersion, sizeof(ci.osVersion)); });
    Stage([&] { SnapshotModules(g_modules); });
    ci.modules = &g_modules;
    ci.symbols = g_symbolsReady ? &g_symbols : nullptr;

    Stage([&] { FillExceptionAndRegisters(ci, req.pointers); });
    Stage([&] { CollectCodeBytes(ci); });
    Stage([&] { CollectStack(ci); });
    Stage([&] { QueryMemoryInfo(ci.memory); });
    Stage([&] { ReadGameState(ci.game, g_config.historySize); });

    // Formatting itself only touches our own memory, but guard it anyway.
    if (!Stage([&] { WriteReport(out, ci); })) {
        out.Append("\n[report generation failed part-way]\n");
    }
}

// ---------------------------------------------------------------------------
// Minidump
// ---------------------------------------------------------------------------

bool WriteMiniDump(const Request& req, OutputFile& file, uint64_t* sizeOut) {
    if (!g_miniDumpWriteDump) return false;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = req.threadId;
    mei.ExceptionPointers = req.pointers;
    mei.ClientPointers = FALSE;

    int type = MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
               MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithProcessThreadData;
    if (g_config.dumpDataSegments) type |= MiniDumpWithDataSegs;
    if (g_config.fullMemoryDump) type |= MiniDumpWithFullMemory;

    BOOL ok = g_miniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file.handle,
                                  static_cast<MINIDUMP_TYPE>(type), &mei, nullptr, nullptr);
    LARGE_INTEGER size = {};
    GetFileSizeEx(file.handle, &size);
    if (sizeOut) *sizeOut = static_cast<uint64_t>(size.QuadPart);
    return ok != FALSE;
}

// ---------------------------------------------------------------------------
// Request processing (runs on the worker thread, or inline as a fallback)
// ---------------------------------------------------------------------------

void ShowCrashMessage(bool logOk, const char* logPath, bool dumpOk, const char* dumpPath) {
    char msg[1600];
    TextBuffer mb(msg, sizeof(msg));
    mb.Append("GTA San Andreas has crashed.\n\n");
    if (logOk) mb.Printf("Crash log:\n%s\n", logPath);
    else mb.Append("The crash log could NOT be written anywhere.\n");
    if (g_config.writeMinidump) {
        if (dumpOk) mb.Printf("\nMinidump:\n%s\n", dumpPath);
        else mb.Append("\nThe minidump could not be written.\n");
    }

    if (g_outcome.fellBack) {
        mb.Printf("\nCould not write to:\n%s\n(error %u", g_outcome.firstFailedPath,
                  static_cast<unsigned>(g_outcome.firstError));
        if (const char* why = DescribeWin32Error(g_outcome.firstError)) mb.Printf(": %s", why);
        mb.Append(logOk ? "), so the files were saved in the fallback folder above.\n" : ").\n");
    } else if (logOk && IsUnderProtectedDirectory(logPath) && IsFileVirtualizationActive()) {
        // The write "succeeded", but Windows may have put the file somewhere else.
        char local[MAX_PATH], redirected[MAX_PATH];
        DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH);
        if (n > 0 && n < MAX_PATH && ToVirtualStorePath(logPath, local, redirected, sizeof(redirected))) {
            mb.Printf("\nThe game is in a protected folder. If the files are not there, Windows "
                      "redirected them to:\n%s\n",
                      redirected);
        }
    }
    MessageBoxA(nullptr, msg, "CrashLogger.SA", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

void ProcessRequest(const Request& req) {
    g_outcome = OutputOutcome();
    TextBuffer out(g_reportStorage, sizeof(g_reportStorage));
    BuildReport(req, out);

    OutputFile logFile;
    bool logOk = false;
    if (OpenOutputFile(g_config.logFileName, logFile)) {
        logOk = WriteAll(logFile.handle, out.Data(), out.Size());
        FlushFileBuffers(logFile.handle);
        CloseHandle(logFile.handle);
        logFile.handle = INVALID_HANDLE_VALUE;
    }

    bool dumpOk = false;
    OutputFile dumpFile;
    if (g_config.writeMinidump && req.source == Source::UnhandledFilter) {
        uint64_t dumpSize = 0;
        if (OpenOutputFile(g_config.dumpFileName, dumpFile)) {
            Stage([&] { dumpOk = WriteMiniDump(req, dumpFile, &dumpSize); });
            CloseHandle(dumpFile.handle);
            dumpFile.handle = INVALID_HANDLE_VALUE;
        }
        char note[600];
        TextBuffer nb(note, sizeof(note));
        if (dumpOk) {
            nb.Printf("\nMinidump      : %s (%u KB)\n", dumpFile.path, static_cast<uint32_t>(dumpSize / 1024));
        } else {
            nb.Printf("\nMinidump      : FAILED to write %s\n", g_config.dumpFileName);
        }
        if (logOk) AppendToFile(logFile.path, nb.Data(), nb.Size());
    }

    if (g_config.showMessageBox && req.source == Source::UnhandledFilter) {
        ShowCrashMessage(logOk, logFile.path, dumpOk, dumpFile.path);
    }
}

void ProcessRequestGuarded(const Request& req) noexcept {
    __try {
        ProcessRequest(req);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // "Never crash in the handler": swallow, whatever happened.
    }
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void LoadDbgHelp() {
    // Load from System32 explicitly: games love shipping their own (ancient) dbghelp.dll.
    char path[MAX_PATH];
    UINT n = GetSystemDirectoryA(path, MAX_PATH);
    if (n == 0 || n + 14 >= MAX_PATH) return;
    TextBuffer tb(path + n, MAX_PATH - n);
    tb.Append("\\dbghelp.dll");
    HMODULE m = LoadLibraryA(path);
    if (m) g_miniDumpWriteDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(m, "MiniDumpWriteDump"));
}

void LoadSymbols(const char* asiDir, const char* exeDir) {
    // Search next to the plugin first, then the game folder, then game\scripts.
    char path[MAX_PATH];
    const char* dirs[3] = {asiDir, exeDir, nullptr};
    char scriptsDir[MAX_PATH];
    JoinPath(scriptsDir, sizeof(scriptsDir), exeDir, "scripts\\");
    dirs[2] = scriptsDir;
    for (const char* dir : dirs) {
        JoinPath(path, sizeof(path), dir, g_config.symbolFile);
        if (g_symbols.LoadFromFile(path)) {
            InterlockedExchange(&g_symbolsReady, 1);
            return;
        }
    }
}

// "Documents\GTA San Andreas User Files\" is writable even when the game itself sits
// in a protected folder, and it only exists if the game has run on this account.
void FindUserFilesDirectory() {
    char docs[MAX_PATH];
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs))) return;
    char candidate[MAX_PATH];
    TextBuffer tb(candidate, sizeof(candidate));
    tb.Printf("%s\\GTA San Andreas User Files\\", docs);
    DWORD attrs = GetFileAttributesA(candidate);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        CopyString(g_outputDirs[1], MAX_PATH, candidate);
    }
}

DWORD WINAPI WorkerThread(LPVOID) {
    RegisterInternalThread(GetCurrentThreadId());

    // Slow, allocating work happens here, once, long before any crash.
    LoadDbgHelp();
    char self[MAX_PATH], asiDir[MAX_PATH], exeDir[MAX_PATH];
    GetModuleFileNameA(g_self, self, MAX_PATH);
    DirectoryOf(self, asiDir, sizeof(asiDir));
    DirectoryOf(g_exePath, exeDir, sizeof(exeDir));
    LoadSymbols(asiDir, exeDir);
    FindUserFilesDirectory();

    InterlockedExchange(&g_workerReady, 1);
    for (;;) {
        WaitForSingleObject(g_requestEvent, INFINITE);
        ProcessRequestGuarded(g_request);
        SetEvent(g_doneEvent);
    }
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

// The game quits through RsGlobal.quit: the menu's Exit entry (RsEventHandler(rsQUITAPP)),
// the window's close button / Alt+F4 and WM_QUIT all set it before teardown starts, and the
// teardown is what tends to fault. Faults after that point are not gameplay crashes.
// Nothing else is used to guess "the game is closing" - guessing hid real crashes.
bool IsShuttingDown(const EXCEPTION_POINTERS* ep) {
    if (!g_config.ignoreShutdownCrashes) return false;

    int32_t quit = 0;
    if (SafeRead(gta::kRsGlobalQuit, quit) && quit != 0) return true;

    // A fault inside ExitProcess / DLL detach is a shutdown fault by definition.
    return ep && StackShowsProcessExit(ep);
}

// True if the faulting thread is (somewhere below) ExitProcess.
bool StackShowsProcessExit(const EXCEPTION_POINTERS* ep) noexcept {
    static CONTEXT ctx; // static: this may run on a nearly exhausted (overflowed) stack
    if (!SafeRead(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ep->ContextRecord)), ctx)) return false;
    uint32_t addr = ctx.Esp;
    uint32_t scanned = 0;
    while (scanned < 16 * 1024) {
        uint32_t toPageEnd = 0x1000 - (addr & 0xFFF);
        static uint32_t words[0x1000 / 4];
        if (!SafeCopy(words, reinterpret_cast<const void*>(static_cast<uintptr_t>(addr)), toPageEnd)) break;
        for (uint32_t i = 0; i < toPageEnd / 4; ++i) {
            for (int f = 0; f < g_exitFunctionCount; ++f) {
                if (words[i] >= g_exitFunctions[f] && words[i] - g_exitFunctions[f] < kExitFunctionSpan) return true;
            }
        }
        addr += toPageEnd;
        scanned += toPageEnd;
    }
    return false;
}

// Called on the faulting thread. Returns once the log has been written (or the
// attempt timed out).
void HandleException(EXCEPTION_POINTERS* ep, Source source) noexcept {
    const DWORD tid = GetCurrentThreadId();
    if (IsShuttingDown(ep)) return;

    if (source == Source::UnhandledFilter) {
        // Finished already (another thread crashed first): just wait for it to complete
        // and fall through to default handling.
        LONG prev = InterlockedCompareExchange(&g_state, kBusy, kIdle);
        if (prev != kIdle) {
            WaitForSingleObject(g_doneEvent, kWorkerTimeoutMs);
            return;
        }
    } else {
        if (InterlockedIncrement(&g_firstChanceLogs) > kMaxFirstChanceLogs) return;
        if (InterlockedCompareExchange(&g_state, kBusy, kIdle) != kIdle) return;
    }

    Request req;
    req.pointers = ep;
    req.threadId = tid;
    req.source = source;

    ResetEvent(g_doneEvent);
    if (g_workerReady) {
        g_request = req;
        SetEvent(g_requestEvent);
        WaitForSingleObject(g_doneEvent, kWorkerTimeoutMs);
    } else {
        // Startup crash, worker not running yet: do the work here. The guarded reads
        // will fault on purpose, so keep the VEH from treating them as game crashes.
        RegisterInternalThread(tid);
        ProcessRequestGuarded(req);
        UnregisterInternalThread(tid);
        SetEvent(g_doneEvent);
    }

    // A real crash is reported exactly once; first-chance logs may repeat.
    InterlockedExchange(&g_state, source == Source::UnhandledFilter ? kFinished : kIdle);
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* ep) {
    if (!IsInternalThread(GetCurrentThreadId())) {
        HandleException(ep, Source::UnhandledFilter);
        // The report is done (or this was an ignorable shutdown fault). Windows' default
        // handling after a game crash tends to leave a frozen window behind, so end the
        // process ourselves. Ignored shutdown faults are ended the same way: the process
        // is dying anyway.
        if (g_config.terminateAfterReport) TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord->ExceptionCode);
    }
    if (g_previousFilter && g_previousFilter != UnhandledFilter) {
        // Whoever we displaced still gets its turn (e.g. WER integration).
        return g_previousFilter(ep);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG CALLBACK VectoredHandler(EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (!IsFatalExceptionCode(code)) return EXCEPTION_CONTINUE_SEARCH;
    if (IsInternalThread(GetCurrentThreadId())) return EXCEPTION_CONTINUE_SEARCH;

    // Take the top-level filter slot back if somebody else grabbed it.
    LPTOP_LEVEL_EXCEPTION_FILTER displaced = SetUnhandledExceptionFilter(UnhandledFilter);
    if (displaced && displaced != UnhandledFilter) g_previousFilter = displaced;

    if (g_config.logFirstChance) HandleException(ep, Source::FirstChance);

    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void InstallCrashHandler(HINSTANCE self) {
    g_self = self;
    g_mainThreadId = GetCurrentThreadId();

    char selfPath[MAX_PATH];
    GetModuleFileNameA(self, selfPath, MAX_PATH);
    char asiDir[MAX_PATH];
    DirectoryOf(selfPath, asiDir, sizeof(asiDir));

    char iniPath[MAX_PATH];
    {
        // CrashLogger.SA.asi -> CrashLogger.SA.ini
        CopyString(iniPath, sizeof(iniPath), selfPath);
        size_t len = 0;
        while (iniPath[len]) ++len;
        if (len > 4) CopyString(iniPath + len - 4, 5, ".ini");
    }
    LoadConfig(iniPath, g_config);

    // Logs go next to gta_sa.exe, not next to the plugin.
    GetModuleFileNameA(nullptr, g_exePath, MAX_PATH);
    char exeDir[MAX_PATH];
    DirectoryOf(g_exePath, exeDir, sizeof(exeDir));
    CopyString(g_outputDirs[0], MAX_PATH, exeDir);
    {
        char temp[MAX_PATH];
        DWORD n = GetTempPathA(MAX_PATH, temp); // already ends with a backslash
        if (n > 0 && n < MAX_PATH) CopyString(g_outputDirs[2], MAX_PATH, temp);
    }

    AddExitFunction("kernel32.dll", "ExitProcess");
    AddExitFunction("ntdll.dll", "RtlExitUserProcess");
    AddExitFunction("ntdll.dll", "LdrShutdownProcess");

    g_requestEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);   // auto-reset
    g_doneEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);       // manual-reset

    // VEH first (position 1 = front of the chain), then the top-level filter.
    AddVectoredExceptionHandler(1, VectoredHandler);
    g_previousFilter = SetUnhandledExceptionFilter(UnhandledFilter);

    HANDLE worker = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    if (worker) CloseHandle(worker);

    if (g_config.monitorStreaming) StartStreamingMonitor(g_config);
}

} // namespace sacl
