#include "exception_info.hpp"
#include "test_framework.hpp"

using namespace sacl;

TEST(exception_code_names) {
    CHECK_STR(ExceptionCodeName(0xC0000005u), "EXCEPTION_ACCESS_VIOLATION");
    CHECK_STR(ExceptionCodeName(0xC00000FDu), "EXCEPTION_STACK_OVERFLOW");
    CHECK_STR(ExceptionCodeName(0xC000001Du), "EXCEPTION_ILLEGAL_INSTRUCTION");
    CHECK_STR(ExceptionCodeName(0xC0000374u), "STATUS_HEAP_CORRUPTION");
    CHECK_STR(ExceptionCodeName(0xE06D7363u), "C++ exception (MSVC)");
    CHECK_STR(ExceptionCodeName(0x12345678u), "UNKNOWN_EXCEPTION");
}

TEST(fatal_code_classification) {
    CHECK(IsFatalExceptionCode(0xC0000005u));
    CHECK(IsFatalExceptionCode(0xC00000FDu));
    CHECK(IsFatalExceptionCode(0xC000001Du));
    // Benign / debugger / C++ exceptions must never trigger first-chance logging.
    CHECK(!IsFatalExceptionCode(0xE06D7363u));
    CHECK(!IsFatalExceptionCode(0x406D1388u)); // SetThreadName
    CHECK(!IsFatalExceptionCode(0x40010006u)); // OutputDebugString
    CHECK(!IsFatalExceptionCode(0x80000003u)); // breakpoint
}

TEST(access_kind_decoding) {
    ExceptionData e;
    e.code = 0xC0000005u;
    e.numParams = 2;
    e.params[0] = 0;
    CHECK(GetAccessKind(e) == AccessKind::Read);
    e.params[0] = 1;
    CHECK(GetAccessKind(e) == AccessKind::Write);
    e.params[0] = 8;
    CHECK(GetAccessKind(e) == AccessKind::Execute);
    e.params[0] = 5;
    CHECK(GetAccessKind(e) == AccessKind::Unknown);
}

TEST(access_kind_requires_av_and_params) {
    ExceptionData e;
    e.code = 0xC000001Du; // illegal instruction: params are meaningless
    e.numParams = 2;
    CHECK(GetAccessKind(e) == AccessKind::None);
    e.code = 0xC0000005u;
    e.numParams = 1; // malformed record
    CHECK(GetAccessKind(e) == AccessKind::None);
}

TEST(suspicious_address_hints) {
    CHECK(DescribeSuspiciousAddress(0x0) != nullptr);
    CHECK(DescribeSuspiciousAddress(0x1C) != nullptr);
    CHECK(DescribeSuspiciousAddress(0xFFFF) != nullptr);
    CHECK(DescribeSuspiciousAddress(0x10000) == nullptr);
    CHECK(DescribeSuspiciousAddress(0x00400000) == nullptr);
    CHECK_CONTAINS(DescribeSuspiciousAddress(0xFEEEFEEEu), "freed");
    CHECK_CONTAINS(DescribeSuspiciousAddress(0xCDCDCDCDu), "uninitialised");
}
