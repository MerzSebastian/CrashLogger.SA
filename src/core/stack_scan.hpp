// Heuristics for the stack walk. GTA SA is built without frame pointers or
// unwind info we can rely on, so we scan the raw stack for values that look like
// return addresses: they point into a module and are directly preceded by a
// CALL instruction.
#pragma once

#include <cstdint>

namespace sacl {

// `prev` holds the 8 bytes immediately before the candidate address, i.e.
// prev[7] is the last byte of the would-be CALL instruction. Recognises:
//   E8 rel32                      call rel32
//   FF /2 with any ModRM/SIB/disp call r/m32 (2..7 bytes)
bool LooksLikeCallReturn(const uint8_t prev[8]) noexcept;

// If the instruction before `returnAddress` is `call rel32`, stores its target and
// returns true. Used to discard stale stack words: a genuine return address after a
// direct call must have a call target that lands in mapped code.
bool DirectCallTarget(const uint8_t prev[8], uint32_t returnAddress, uint32_t* target) noexcept;

} // namespace sacl
