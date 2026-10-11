// Hard-coded addresses and struct offsets for GTA San Andreas 1.0 US (HOODLUM
// "compact" exe, image base 0x400000).
//
// These come from public community documentation of the game. Nothing here is
// Rockstar code or data - only numbers.
// Every read goes through SafeRead(), so a wrong value produces a missing field
// in the log, never a second crash. If you find one that is wrong, fix it here;
// this is the only file that knows about game memory layout.
#pragma once

#include <cstdint>

namespace sacl::gta {

// Version check: this dword is 0x94BF in the 1.0 US exe. (1.0 EU has it at 0x8245BC instead.)
constexpr uint32_t kVersionCheckAddr = 0x82457C;
constexpr uint32_t kVersionCheckValue = 0x94BF;

// --- RsGlobal (RenderWare skeleton app state) ---
// RsGlobalType { appName; maximumWidth; maximumHeight; frameLimit; RwBool quit; ... }
// RsGlobal.quit (int32) becomes non-zero when the player exits from the menu, closes the
// window, or WM_QUIT arrives (RsEventHandler(rsQUITAPP) @ 0x619B60 sets it).
constexpr uint32_t kRsGlobal = 0xC17040;
constexpr uint32_t kRsGlobalQuit = kRsGlobal + 0x10;

// --- CTimer ---
constexpr uint32_t kTimeInMilliseconds = 0xB7CB84; // CTimer::m_snTimeInMilliseconds

// --- CGame ---
constexpr uint32_t kCurrentArea = 0xB72914; // CGame::currArea (int, 0 = outside)

// --- Scripts ---
constexpr uint32_t kActiveScriptsHead = 0xA8B42C; // CTheScripts::pActiveScripts (CRunningScript*)
// CRunningScript offsets
constexpr uint32_t kScriptNext = 0x00;
constexpr uint32_t kScriptName = 0x08;            // char[8], not necessarily NUL terminated
constexpr uint32_t kScriptBaseIP = 0x10;
constexpr uint32_t kScriptCurrentIP = 0x14;
constexpr uint32_t kScriptUseMissionCleanup = 0xCE; // bool
constexpr uint32_t kScriptIsMission = 0xE4;         // bool
constexpr uint32_t kMaxScriptsWalked = 128;         // loop guard against a corrupted list

// --- Player / entities ---
constexpr uint32_t kPlayerInFocus = 0xB7CD74; // CWorld::PlayerInFocus (uint8)
constexpr uint32_t kPlayersArray = 0xB7CD98;  // CWorld::Players (CPlayerInfo[2])
constexpr uint32_t kPlayerInfoSize = 0x190;   // sizeof(CPlayerInfo); m_pPed is at +0
// CPlaceable / CEntity / CPed offsets
constexpr uint32_t kPlaceablePosition = 0x04; // CSimpleTransform::pos (used when no matrix)
constexpr uint32_t kPlaceableMatrix = 0x14;   // CMatrix* ; position at matrix+0x30
constexpr uint32_t kMatrixPosition = 0x30;
constexpr uint32_t kEntityModelIndex = 0x22;  // uint16
constexpr uint32_t kEntityAreaCode = 0x2F;    // uint8
constexpr uint32_t kPedFlagsByte1 = 0x46D;    // bit 0 = bInVehicle
constexpr uint32_t kPedVehicle = 0x58C;       // CVehicle*

// --- CStreaming ---
constexpr uint32_t kStreamingInfoArray = 0x8E4CC0; // CStreaming::ms_aInfoForModel
constexpr uint32_t kStreamingInfoSize = 20;        // sizeof(CStreamingInfo)
constexpr uint32_t kStreamingInfoLoadState = 0x10; // uint8 inside CStreamingInfo
constexpr uint32_t kStreamingInfoCount = 26316;
constexpr uint32_t kStreamingMemoryUsed = 0x8E4CB4;      // CStreaming::ms_memoryUsed (bytes)
constexpr uint32_t kStreamingMemoryAvailable = 0x8A5A80; // CStreaming::ms_memoryAvailable (bytes)

// eStreamingLoadState
constexpr uint8_t kLoadStateRequested = 2;
constexpr uint8_t kLoadStateChannelled = 3;
constexpr uint8_t kLoadStateFinishing = 4;

} // namespace sacl::gta
