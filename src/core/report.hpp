// Crash report formatting. Pure data in, text out: no Windows headers, no heap,
// so it can be unit-tested and called from the crash handler alike.
#pragma once

#include "exception_info.hpp"
#include "game_state.hpp"
#include "module_table.hpp"
#include "symbol_table.hpp"
#include "text_buffer.hpp"

#include <cstddef>
#include <cstdint>

namespace sacl {

constexpr const char* kVersionString = "1.0.0";
constexpr uint32_t kMaxStackDump = 32;  // dwords printed raw
constexpr uint32_t kMaxFrames = 64;

struct StackFrame {
    uint32_t address = 0;     // candidate return address
    uint32_t stackOffset = 0; // distance from ESP where it was found
};

struct MemoryInfo {
    bool valid = false;
    uint64_t userSpaceTotal = 0;
    uint64_t addressFree = 0;
    uint64_t addressLargestFree = 0;
    uint64_t addressCommitted = 0;
    uint64_t addressReserved = 0;
    uint64_t physicalTotal = 0;
    uint64_t physicalAvailable = 0;
    uint64_t pageFileTotal = 0;
    uint64_t pageFileAvailable = 0;
};

struct CrashInfo {
    char timestamp[32] = {}; // "YYYY-MM-DD HH:MM:SS"
    const char* source = "";
    char exePath[260] = {};
    char osVersion[64] = {};

    uint32_t faultingThreadId = 0;
    uint32_t mainThreadId = 0;

    ExceptionData exception;
    Registers regs;

    bool codeBytesValid = false;
    uint8_t codeBytes[16] = {};

    uint32_t stackDump[kMaxStackDump] = {};
    uint32_t stackDumpCount = 0;
    StackFrame frames[kMaxFrames];
    uint32_t frameCount = 0;

    MemoryInfo memory;
    GameState game;

    const ModuleTable* modules = nullptr;
    const SymbolTable* symbols = nullptr; // may be null / empty
    uint32_t symbolMaxGap = 1024;
    uint32_t streamingHistoryLimit = 32;
};

// Prints "name.exe+0x1234 (Function+0x10)" for addresses inside a known module,
// "<not in any module>" otherwise. Function names come from `symbols` and are only
// looked up for the main executable. Returns true if the address is in a module.
bool FormatLocation(TextBuffer& out, uint32_t address, const ModuleTable& modules,
                    const SymbolTable* symbols, uint32_t symbolMaxGap);

// Writes the complete crashlog.txt body.
void WriteReport(TextBuffer& out, const CrashInfo& info);

// "ZF PF IF" style decoding of an EFLAGS value.
void FormatEflags(TextBuffer& out, uint32_t eflags);

// Rendering helpers exposed for tests.
void FormatByteSize(TextBuffer& out, uint64_t bytes); // "123.4 MB"

} // namespace sacl
