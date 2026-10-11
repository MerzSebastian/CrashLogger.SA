// Plain-data snapshot of the game at the moment of the crash. Filled in by the
// Windows layer (plugin/game_reader.cpp) and printed by the report formatter.
#pragma once

#include <cstdint>

namespace sacl {

constexpr uint32_t kMaxScripts = 32;
constexpr uint32_t kMaxStreamEntries = 64;

struct ScriptEntry {
    char name[9] = {};
    bool isMission = false;
    bool useMissionCleanup = false;
    bool ipValid = false;
    uint32_t ipOffset = 0; // current IP relative to the script's base IP
};

struct StreamEntry {
    uint32_t modelId = 0;
    uint8_t state = 0;    // eStreamingLoadState
    uint32_t ageMs = 0;   // time between the request being seen and the crash
};

struct GameState {
    // 1.0 US signature check. If it failed, nothing below was read.
    bool versionKnown = false;
    bool versionOk = false;

    bool timeValid = false;
    uint32_t gameTimeMs = 0;

    bool scriptsValid = false;
    bool missionFlagUnreliable = false; // every script claimed to be a mission
    uint32_t scriptCount = 0; // total walked (may exceed kMaxScripts)
    ScriptEntry scripts[kMaxScripts];

    bool playerValid = false;
    float position[3] = {};
    bool areaValid = false;
    int32_t currentArea = 0;   // CGame::currArea (0 = outside)
    int32_t playerArea = 0;    // ped->m_nAreaCode
    uint32_t playerModel = 0;  // skin model id

    bool vehicleValid = false;
    bool inVehicle = false;
    uint32_t vehicleModel = 0;

    bool streamingMemoryValid = false;
    uint32_t streamingMemoryUsed = 0;      // bytes
    uint32_t streamingMemoryAvailable = 0; // bytes

    bool inFlightValid = false;
    uint32_t inFlightTotal = 0; // may exceed kMaxStreamEntries
    uint32_t inFlightCount = 0;
    StreamEntry inFlight[kMaxStreamEntries];

    uint32_t historyCount = 0; // newest first
    StreamEntry history[kMaxStreamEntries];
};

// Rough classification of a CStreaming info index (model, TXD, COL, ...).
const char* StreamingIdKind(uint32_t id) noexcept;
// eStreamingLoadState -> text.
const char* StreamingStateName(uint8_t state) noexcept;

} // namespace sacl
