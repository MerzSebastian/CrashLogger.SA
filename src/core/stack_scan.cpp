#include "stack_scan.hpp"

namespace sacl {

namespace {

// Total length of "FF modrm [sib] [disp]" for the given ModRM (and, if present, SIB)
// byte, 32-bit addressing.
unsigned CallRmLength(uint8_t modrm, uint8_t sib) noexcept {
    unsigned mod = modrm >> 6;
    unsigned rm = modrm & 7;
    unsigned len = 2; // opcode + modrm
    bool hasSib = mod != 3 && rm == 4;
    if (hasSib) len += 1;
    if (mod == 0 && rm == 5) len += 4;                      // disp32 only
    else if (mod == 0 && hasSib && (sib & 7) == 5) len += 4; // SIB with no base: disp32
    else if (mod == 1) len += 1;                            // disp8
    else if (mod == 2) len += 4;                            // disp32
    return len;
}

} // namespace

bool LooksLikeCallReturn(const uint8_t prev[8]) noexcept {
    // call rel32: E8 xx xx xx xx -> opcode is 5 bytes before the return address.
    if (prev[3] == 0xE8) return true;

    // call r/m32: FF /2. Try every plausible instruction length.
    for (unsigned len = 2; len <= 7; ++len) {
        uint8_t opcode = prev[8 - len];
        if (opcode != 0xFF) continue;
        uint8_t modrm = prev[8 - len + 1];
        if (((modrm >> 3) & 7) != 2) continue;
        // The SIB byte (if any) sits right after ModRM; for len == 2 there is none.
        uint8_t sib = len > 2 ? prev[8 - len + 2] : 0;
        if (CallRmLength(modrm, sib) == len) return true;
    }
    return false;
}

bool DirectCallTarget(const uint8_t prev[8], uint32_t returnAddress, uint32_t* target) noexcept {
    if (prev[3] != 0xE8) return false;
    uint32_t rel = static_cast<uint32_t>(prev[4]) | (static_cast<uint32_t>(prev[5]) << 8) |
                   (static_cast<uint32_t>(prev[6]) << 16) | (static_cast<uint32_t>(prev[7]) << 24);
    if (target) *target = returnAddress + rel; // wraps modulo 2^32, like the CPU
    return true;
}

} // namespace sacl
