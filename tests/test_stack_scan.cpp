#include "stack_scan.hpp"
#include "test_framework.hpp"

using sacl::LooksLikeCallReturn;

namespace {
// Builds the 8-byte window with `insn` placed so that it ends at prev[7].
bool Check(std::initializer_list<uint8_t> insn) {
    uint8_t prev[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    size_t n = insn.size();
    size_t i = 8 - n;
    for (uint8_t b : insn) prev[i++] = b;
    return LooksLikeCallReturn(prev);
}
} // namespace

TEST(call_rel32) {
    CHECK(Check({0xE8, 0x10, 0x20, 0x30, 0x40}));
}

TEST(call_register_and_indirect_forms) {
    CHECK(Check({0xFF, 0xD0}));                         // call eax
    CHECK(Check({0xFF, 0xD6}));                         // call esi
    CHECK(Check({0xFF, 0x10}));                         // call [eax]
    CHECK(Check({0xFF, 0x50, 0x08}));                   // call [eax+8]
    CHECK(Check({0xFF, 0x14, 0x85, 0x00, 0x10, 0x40, 0x00})); // call [eax*4+0x401000]
    CHECK(Check({0xFF, 0x15, 0x00, 0x10, 0x40, 0x00})); // call [0x401000]
    CHECK(Check({0xFF, 0x90, 0x00, 0x01, 0x00, 0x00})); // call [eax+0x100]
    CHECK(Check({0xFF, 0x14, 0x24}));                   // call [esp]
    CHECK(Check({0xFF, 0x54, 0x24, 0x04}));             // call [esp+4]
}

TEST(non_call_instructions_are_rejected) {
    CHECK(!Check({0x90}));                              // nop
    CHECK(!Check({0xFF, 0x30}));                        // push [eax]  (FF /6)
    CHECK(!Check({0xFF, 0x20}));                        // jmp [eax]   (FF /4)
    CHECK(!Check({0xFF, 0xE0}));                        // jmp eax
    CHECK(!Check({0xE9, 0x10, 0x20, 0x30, 0x40}));      // jmp rel32
    CHECK(!Check({0x8B, 0x45, 0x08}));                  // mov eax,[ebp+8]
}

TEST(length_must_match_modrm) {
    // FF /2 opcode bytes that do not end exactly at the return address do not count:
    // "FF 50 08" is 3 bytes, but here a stray byte follows it.
    uint8_t prev[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0xFF, 0x50, 0x08};
    CHECK(LooksLikeCallReturn(prev));
    uint8_t shifted[8] = {0x90, 0x90, 0x90, 0x90, 0xFF, 0x50, 0x08, 0xCC};
    CHECK(!LooksLikeCallReturn(shifted));
}

TEST(direct_call_target_forward_and_backward) {
    uint32_t target = 0;
    // call +0x10 with return address 0x401005 -> target 0x401015
    uint8_t fwd[8] = {0x90, 0x90, 0x90, 0xE8, 0x10, 0x00, 0x00, 0x00};
    CHECK(sacl::DirectCallTarget(fwd, 0x401005, &target));
    CHECK(target == 0x401015u);
    // call -0x100
    uint8_t back[8] = {0x90, 0x90, 0x90, 0xE8, 0x00, 0xFF, 0xFF, 0xFF};
    CHECK(sacl::DirectCallTarget(back, 0x401005, &target));
    CHECK(target == 0x400F05u);
}

TEST(direct_call_target_only_for_e8) {
    uint32_t target = 0xDEADu;
    uint8_t indirect[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0xFF, 0x50, 0x08};
    CHECK(!sacl::DirectCallTarget(indirect, 0x401005, &target));
    CHECK(target == 0xDEADu);
}
