#pragma once

#include "../core/game_state.hpp"

namespace sacl {

// Fills `state` from game memory. Every read is SEH-guarded; fields that cannot be
// read stay marked invalid. Never throws, never allocates.
void ReadGameState(GameState& state, uint32_t historyLimit) noexcept;

} // namespace sacl
