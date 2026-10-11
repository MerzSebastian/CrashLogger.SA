#include "exception_info.hpp"

namespace sacl {

namespace {
struct CodeName {
    uint32_t code;
    const char* name;
};

const CodeName kCodeNames[] = {
    {0xC0000005u, "EXCEPTION_ACCESS_VIOLATION"},
    {0xC0000006u, "EXCEPTION_IN_PAGE_ERROR"},
    {0xC0000008u, "EXCEPTION_INVALID_HANDLE"},
    {0xC000001Du, "EXCEPTION_ILLEGAL_INSTRUCTION"},
    {0xC0000025u, "EXCEPTION_NONCONTINUABLE_EXCEPTION"},
    {0xC0000026u, "EXCEPTION_INVALID_DISPOSITION"},
    {0xC000008Cu, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
    {0xC000008Du, "EXCEPTION_FLT_DENORMAL_OPERAND"},
    {0xC000008Eu, "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
    {0xC000008Fu, "EXCEPTION_FLT_INEXACT_RESULT"},
    {0xC0000090u, "EXCEPTION_FLT_INVALID_OPERATION"},
    {0xC0000091u, "EXCEPTION_FLT_OVERFLOW"},
    {0xC0000092u, "EXCEPTION_FLT_STACK_CHECK"},
    {0xC0000093u, "EXCEPTION_FLT_UNDERFLOW"},
    {0xC0000094u, "EXCEPTION_INT_DIVIDE_BY_ZERO"},
    {0xC0000095u, "EXCEPTION_INT_OVERFLOW"},
    {0xC0000096u, "EXCEPTION_PRIV_INSTRUCTION"},
    {0xC00000FDu, "EXCEPTION_STACK_OVERFLOW"},
    {0xC0000374u, "STATUS_HEAP_CORRUPTION"},
    {0xC0000409u, "STATUS_STACK_BUFFER_OVERRUN"},
    {0x80000002u, "EXCEPTION_DATATYPE_MISALIGNMENT"},
    {0x80000003u, "EXCEPTION_BREAKPOINT"},
    {0x80000004u, "EXCEPTION_SINGLE_STEP"},
    {0xE06D7363u, "C++ exception (MSVC)"},
};
} // namespace

const char* ExceptionCodeName(uint32_t code) noexcept {
    for (const CodeName& c : kCodeNames) {
        if (c.code == code) return c.name;
    }
    return "UNKNOWN_EXCEPTION";
}

bool IsFatalExceptionCode(uint32_t code) noexcept {
    switch (code) {
    case 0xC0000005u: // access violation
    case 0xC0000006u: // in-page error
    case 0xC000001Du: // illegal instruction
    case 0xC0000096u: // privileged instruction
    case 0xC000008Cu: // array bounds
    case 0xC0000094u: // integer divide by zero
    case 0xC00000FDu: // stack overflow
    case 0xC0000374u: // heap corruption
    case 0xC0000409u: // stack buffer overrun / fast fail
        return true;
    default:
        return false;
    }
}

AccessKind GetAccessKind(const ExceptionData& e) noexcept {
    if ((e.code != 0xC0000005u && e.code != 0xC0000006u) || e.numParams < 2) return AccessKind::None;
    switch (e.params[0]) {
    case 0: return AccessKind::Read;
    case 1: return AccessKind::Write;
    case 8: return AccessKind::Execute;
    default: return AccessKind::Unknown;
    }
}

const char* AccessKindName(AccessKind kind) noexcept {
    switch (kind) {
    case AccessKind::Read: return "read";
    case AccessKind::Write: return "write";
    case AccessKind::Execute: return "execute (DEP)";
    case AccessKind::Unknown: return "unknown access";
    default: return "none";
    }
}

const char* DescribeSuspiciousAddress(uint32_t a) noexcept {
    if (a < 0x10000u) return "NULL pointer plus small offset (uninitialised or freed object?)";
    switch (a) {
    case 0xCDCDCDCDu: return "MSVC debug fill: uninitialised heap memory (0xCD)";
    case 0xDDDDDDDDu: return "MSVC debug fill: freed heap memory (0xDD)";
    case 0xFDFDFDFDu: return "MSVC debug fill: guard bytes around heap block (0xFD)";
    case 0xFEEEFEEEu: return "Windows heap fill: freed memory (0xFEEEFEEE)";
    case 0xBAADF00Du: return "Windows heap fill: uninitialised LocalAlloc memory (0xBAADF00D)";
    case 0xABABABABu: return "Windows heap fill: guard bytes after heap block (0xABABABAB)";
    case 0xCCCCCCCCu: return "MSVC debug fill: uninitialised stack memory (0xCC)";
    default: return nullptr;
    }
}

} // namespace sacl
