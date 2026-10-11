// Portable (no <windows.h>) description of an exception, plus helpers that turn
// raw exception data into human-readable text.
#pragma once

#include <cstdint>

namespace sacl {

struct ExceptionData {
    uint32_t code = 0;
    uint32_t flags = 0;
    uint32_t address = 0;
    uint32_t numParams = 0;
    uint32_t params[15] = {};
};

struct Registers {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    uint32_t esi = 0, edi = 0, ebp = 0, esp = 0;
    uint32_t eip = 0, eflags = 0;
    uint16_t cs = 0, ds = 0, es = 0, fs = 0, gs = 0, ss = 0;
};

enum class AccessKind { None, Read, Write, Execute, Unknown };

// Windows exception code -> symbolic name, e.g. "EXCEPTION_ACCESS_VIOLATION".
const char* ExceptionCodeName(uint32_t code) noexcept;

// True for codes that almost certainly mean "the process is going down", used
// to decide what the optional first-chance logging mode reacts to.
bool IsFatalExceptionCode(uint32_t code) noexcept;

// Interprets ExceptionInformation[0] of ACCESS_VIOLATION / IN_PAGE_ERROR.
AccessKind GetAccessKind(const ExceptionData& e) noexcept;
const char* AccessKindName(AccessKind kind) noexcept;

// Returns a short hint for addresses that look like the result of a common bug
// (NULL + small offset, MSVC debug fill patterns), or nullptr.
const char* DescribeSuspiciousAddress(uint32_t address) noexcept;

} // namespace sacl
