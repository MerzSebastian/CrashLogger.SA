// Records which streaming requests the game made, so the crash log can show the
// last N model IDs.
//
// We deliberately do NOT detour CStreaming::RequestModel: an inline hook needs a
// correct instruction-length decoder and collides with other mods that hook the
// same function. Instead a low-priority thread polls the load state of every
// CStreamingInfo entry and notes new REQUESTED/CHANNELLED transitions. The
// trade-off: requests that complete in under one sample interval can be missed.
#pragma once

#include "../core/game_state.hpp"
#include "config.hpp"

#include <cstdint>

namespace sacl {

void StartStreamingMonitor(const Config& cfg);

// Copies up to `maxEntries` records, newest first. `ageMs` is relative to now.
uint32_t CopyStreamingHistory(StreamEntry* out, uint32_t maxEntries);

} // namespace sacl
