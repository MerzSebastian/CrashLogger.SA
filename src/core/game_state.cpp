#include "game_state.hpp"

namespace sacl {

const char* StreamingIdKind(uint32_t id) noexcept {
    // Boundaries of the CStreaming info array in GTA SA 1.0.
    if (id < 20000) return "model";
    if (id < 25000) return "txd";
    if (id < 25255) return "col";
    if (id < 25511) return "ipl";
    if (id < 25575) return "dat";
    if (id < 25755) return "ifp";
    if (id < 25975) return "rrr";
    if (id < 26230) return "scm";
    if (id < 26316) return "other";
    return "invalid";
}

const char* StreamingStateName(uint8_t state) noexcept {
    switch (state) {
    case 0: return "not loaded";
    case 1: return "loaded";
    case 2: return "requested";
    case 3: return "channelled";
    case 4: return "finishing";
    default: return "?";
    }
}

} // namespace sacl
