#include "game_reader.hpp"

#include "game_addresses.hpp"
#include "safe_memory.hpp"
#include "streaming_monitor.hpp"

#include <windows.h>

namespace sacl {

namespace {

bool IsPlausibleHeapPointer(uint32_t p) {
    // Below 64 KB is the NULL page; the top 64 KB is reserved by Windows.
    return p >= 0x10000u && p < 0xFFFF0000u;
}

void ReadScripts(GameState& g) {
    uint32_t script = 0;
    if (!SafeRead(gta::kActiveScriptsHead, script)) return;
    g.scriptsValid = true;

    for (uint32_t walked = 0; script && walked < gta::kMaxScriptsWalked; ++walked) {
        if (!IsPlausibleHeapPointer(script)) break;

        char name[8];
        uint32_t next = 0;
        if (!SafeRead(script + gta::kScriptName, name) || !SafeRead(script + gta::kScriptNext, next)) {
            break;
        }

        if (g.scriptCount < kMaxScripts) {
            ScriptEntry& e = g.scripts[g.scriptCount];
            uint32_t n = 0;
            for (; n < 8 && name[n] >= 0x20 && name[n] < 0x7F; ++n) e.name[n] = name[n];
            e.name[n] = '\0';

            uint32_t baseIp = 0, curIp = 0;
            if (SafeRead(script + gta::kScriptBaseIP, baseIp) &&
                SafeRead(script + gta::kScriptCurrentIP, curIp) && baseIp != 0 && curIp >= baseIp) {
                e.ipValid = true;
                e.ipOffset = curIp - baseIp;
            }
            uint8_t flag = 0;
            if (SafeRead(script + gta::kScriptIsMission, flag)) e.isMission = flag != 0;
            if (SafeRead(script + gta::kScriptUseMissionCleanup, flag)) e.useMissionCleanup = flag != 0;
        }
        ++g.scriptCount;
        script = next;
    }

    // If the "is mission" byte is set on every script, the offset is wrong for this
    // exe/mod setup; the flag carries no information then, so drop it.
    uint32_t shown = g.scriptCount < kMaxScripts ? g.scriptCount : kMaxScripts;
    bool allSet = shown > 0;
    for (uint32_t i = 0; i < shown; ++i) allSet = allSet && g.scripts[i].isMission;
    if (allSet && shown > 1) {
        for (uint32_t i = 0; i < shown; ++i) g.scripts[i].isMission = false;
        g.missionFlagUnreliable = true;
    }
}

void ReadPlayer(GameState& g) {
    uint8_t focus = 0;
    if (!SafeRead(gta::kPlayerInFocus, focus) || focus > 1) return;

    uint32_t ped = 0;
    if (!SafeRead(gta::kPlayersArray + focus * gta::kPlayerInfoSize, ped) || !IsPlausibleHeapPointer(ped)) {
        return;
    }

    // Position: from the matrix if the entity has one, otherwise the simple transform.
    uint32_t matrix = 0;
    float pos[3];
    bool havePos = false;
    if (SafeRead(ped + gta::kPlaceableMatrix, matrix) && matrix != 0) {
        havePos = IsPlausibleHeapPointer(matrix) && SafeRead(matrix + gta::kMatrixPosition, pos);
    } else {
        havePos = SafeRead(ped + gta::kPlaceablePosition, pos);
    }
    if (!havePos) return;

    g.playerValid = true;
    for (int i = 0; i < 3; ++i) g.position[i] = pos[i];

    uint16_t model = 0;
    if (SafeRead(ped + gta::kEntityModelIndex, model)) g.playerModel = model;

    uint8_t areaCode = 0;
    int32_t currentArea = 0;
    if (SafeRead(gta::kCurrentArea, currentArea)) {
        g.areaValid = true;
        g.currentArea = currentArea;
        if (SafeRead(ped + gta::kEntityAreaCode, areaCode)) g.playerArea = areaCode;
    }

    uint8_t pedFlags = 0;
    if (SafeRead(ped + gta::kPedFlagsByte1, pedFlags)) {
        g.inVehicle = (pedFlags & 1) != 0;
        uint32_t vehicle = 0;
        if (g.inVehicle && SafeRead(ped + gta::kPedVehicle, vehicle) && IsPlausibleHeapPointer(vehicle)) {
            uint16_t vehModel = 0;
            if (SafeRead(vehicle + gta::kEntityModelIndex, vehModel)) {
                g.vehicleValid = true;
                g.vehicleModel = vehModel;
            }
        } else {
            g.vehicleValid = true; // on foot is a valid answer
        }
    }
}

void ReadStreaming(GameState& g, uint32_t historyLimit) {
    uint32_t used = 0, avail = 0;
    if (SafeRead(gta::kStreamingMemoryUsed, used) && SafeRead(gta::kStreamingMemoryAvailable, avail)) {
        g.streamingMemoryValid = true;
        g.streamingMemoryUsed = used;
        g.streamingMemoryAvailable = avail;
    }

    // Requests that are still pending right now.
    __try {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(gta::kStreamingInfoArray));
        for (uint32_t i = 0; i < gta::kStreamingInfoCount; ++i) {
            uint8_t st = base[i * gta::kStreamingInfoSize + gta::kStreamingInfoLoadState];
            if (st == gta::kLoadStateRequested || st == gta::kLoadStateChannelled ||
                st == gta::kLoadStateFinishing) {
                if (g.inFlightCount < kMaxStreamEntries) {
                    g.inFlight[g.inFlightCount].modelId = i;
                    g.inFlight[g.inFlightCount].state = st;
                    ++g.inFlightCount;
                }
                ++g.inFlightTotal;
            }
        }
        g.inFlightValid = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }

    if (historyLimit > kMaxStreamEntries) historyLimit = kMaxStreamEntries;
    g.historyCount = CopyStreamingHistory(g.history, historyLimit);
}

// Each reader gets its own guard so one bad pointer cannot hide the others.
template <typename F>
bool Guarded(F&& fn) noexcept {
    __try {
        fn();
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace

void ReadGameState(GameState& g, uint32_t historyLimit) noexcept {
    uint32_t signature = 0;
    g.versionKnown = true;
    g.versionOk = SafeRead(gta::kVersionCheckAddr, signature) && signature == gta::kVersionCheckValue;
    if (!g.versionOk) return;

    uint32_t t = 0;
    if (SafeRead(gta::kTimeInMilliseconds, t)) {
        g.timeValid = true;
        g.gameTimeMs = t;
    }

    Guarded([&] { ReadScripts(g); });
    Guarded([&] { ReadPlayer(g); });
    Guarded([&] { ReadStreaming(g, historyLimit); });
}

} // namespace sacl
