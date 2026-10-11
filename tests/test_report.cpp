#include "report.hpp"
#include "test_framework.hpp"

#include <memory>

using namespace sacl;

namespace {

constexpr uint32_t kExeBase = 0x400000;

struct Scenario {
    ModuleTable modules;
    SymbolTable symbols;
    std::unique_ptr<CrashInfo> info{new CrashInfo()};
    std::unique_ptr<char[]> storage{new char[256 * 1024]};

    Scenario() {
        modules.Add(kExeBase, 0x800000, 0x4C2B5C, true, "gta_sa.exe", "C:\\Games\\GTA SA\\gta_sa.exe", "1.0.0.0");
        modules.Add(0x10000000, 0x20000, 0x5E000000, false, "CrashLogger.SA.asi",
                    "C:\\Games\\GTA SA\\CrashLogger.SA.asi", "1.0.0.0");
        modules.Add(0x74000000, 0x100000, 0, false, "kernel32.dll", "C:\\Windows\\SysWOW64\\kernel32.dll", "");
        // One sized (exact) and one unsized (approximate) symbol.
        const char* syms = "0x4087E0 0x40 CStreaming::RequestModel\n0x53BEE0 CGame::Process\n";
        symbols.LoadFromText(syms, std::char_traits<char>::length(syms));

        CrashInfo& ci = *info;
        CopyString(ci.timestamp, sizeof(ci.timestamp), "2026-10-08 12:34:56");
        CopyString(ci.exePath, sizeof(ci.exePath), "C:\\Games\\GTA SA\\gta_sa.exe");
        CopyString(ci.osVersion, sizeof(ci.osVersion), "Windows 10.0 build 26200");
        ci.source = "unhandled exception filter";
        ci.faultingThreadId = 100;
        ci.mainThreadId = 100;
        ci.exception.code = 0xC0000005u;
        ci.exception.address = 0x4087E0 + 0x1A;
        ci.exception.numParams = 2;
        ci.exception.params[0] = 0;
        ci.exception.params[1] = 0x10;
        ci.regs.eax = 0x1;
        ci.regs.esp = 0x19FF00;
        ci.regs.eip = ci.exception.address;
        ci.regs.eflags = 0x10246;
        ci.modules = &modules;
        ci.symbols = &symbols;
    }

    std::string Render() {
        TextBuffer out(storage.get(), 256 * 1024);
        WriteReport(out, *info);
        return std::string(out.Data(), out.Size());
    }
};

std::string Location(uint32_t addr, const Scenario& s) {
    char buf[256];
    TextBuffer tb(buf, sizeof(buf));
    FormatLocation(tb, addr, s.modules, &s.symbols, 1024);
    return std::string(tb.Data(), tb.Size());
}

} // namespace

TEST(location_module_offset_and_symbol) {
    Scenario s;
    CHECK_STR(Location(0x4087E0, s), "gta_sa.exe+0x87E0 (CStreaming::RequestModel+0x0)");
    CHECK_STR(Location(0x4087FA, s), "gta_sa.exe+0x87FA (CStreaming::RequestModel+0x1A)");
}

TEST(location_unsized_symbol_is_marked_approximate) {
    Scenario s;
    CHECK_STR(Location(0x53BEE0 + 0x200, s), "gta_sa.exe+0x13C0E0 (~CGame::Process+0x200)");
    // Past the gap limit (1024) there is no symbol at all.
    CHECK_STR(Location(0x53BEE0 + 0x2000, s), "gta_sa.exe+0x13DEE0");
}

TEST(location_sized_symbol_is_bounded) {
    Scenario s;
    // Size is 0x40, so one byte past the end is outside the function.
    CHECK_STR(Location(0x4087E0 + 0x3F, s), "gta_sa.exe+0x881F (CStreaming::RequestModel+0x3F)");
    CHECK_STR(Location(0x4087E0 + 0x40, s), "gta_sa.exe+0x8820");
}

TEST(location_non_exe_module_gets_no_symbol) {
    Scenario s;
    // 0x10000000+0x87E0 would collide with the exe's symbol if offsets were mixed up.
    CHECK_STR(Location(0x10000000 + 0x87E0, s), "CrashLogger.SA.asi+0x87E0");
    CHECK_STR(Location(0x74000010, s), "kernel32.dll+0x10");
}

TEST(location_outside_all_modules) {
    Scenario s;
    CHECK_STR(Location(0x00001234, s), "<not in any module>");
    CHECK_STR(Location(0xFFFFFFFFu, s), "<not in any module>");
}

TEST(location_module_boundaries) {
    Scenario s;
    CHECK_STR(Location(0x400000 + 0x7FFFFF, s), "gta_sa.exe+0x7FFFFF");
    CHECK_STR(Location(0x400000 + 0x800000, s), "<not in any module>");
    CHECK_STR(Location(0x3FFFFF, s), "<not in any module>");
}

TEST(eflags_decoding) {
    char buf[64];
    TextBuffer tb(buf, sizeof(buf));
    FormatEflags(tb, 0x10246);
    CHECK_STR(std::string(tb.Data()), "PF ZF IF");
    TextBuffer tb2(buf, sizeof(buf));
    FormatEflags(tb2, 0);
    CHECK_STR(std::string(tb2.Data()), "");
}

TEST(byte_size_formatting) {
    char buf[64];
    TextBuffer tb(buf, sizeof(buf));
    FormatByteSize(tb, 0);
    tb.Append("|");
    FormatByteSize(tb, 1536 * 1024);
    tb.Append("|");
    FormatByteSize(tb, 3ull * 1024 * 1024 * 1024 + 512 * 1024 * 1024);
    CHECK_STR(std::string(tb.Data()), "0.0 MB|1.5 MB|3584.0 MB");
}

TEST(report_exception_section) {
    Scenario s;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "Code          : 0xC0000005  EXCEPTION_ACCESS_VIOLATION");
    CHECK_CONTAINS(r, "Fault address : 0x004087FA  gta_sa.exe+0x87FA (CStreaming::RequestModel+0x1A)");
    CHECK_CONTAINS(r, "Access        : read of address 0x00000010");
    CHECK_CONTAINS(r, "Hint          : NULL pointer plus small offset");
    CHECK_CONTAINS(r, "(main thread)");
    CHECK_CONTAINS(r, "Code at EIP   : <unreadable>");
}

TEST(report_write_and_execute_access) {
    Scenario s;
    s.info->exception.params[0] = 1;
    s.info->exception.params[1] = 0x00800000;
    CHECK_CONTAINS(s.Render(), "Access        : write of address 0x00800000");
    s.info->exception.params[0] = 8;
    CHECK_CONTAINS(s.Render(), "Access        : execute (DEP) of address");
}

TEST(report_non_access_violation_has_no_access_line) {
    Scenario s;
    s.info->exception.code = 0xC0000094u;
    s.info->exception.numParams = 0;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "EXCEPTION_INT_DIVIDE_BY_ZERO");
    CHECK(!sacl_test::Contains(r, "Access        :"));
}

TEST(report_registers) {
    Scenario s;
    s.info->regs.ebx = 0x4087E0;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "EAX = 0x00000001");
    CHECK_CONTAINS(r, "EBX = 0x004087E0   gta_sa.exe+0x87E0 (CStreaming::RequestModel+0x0)");
    CHECK_CONTAINS(r, "EFLAGS = 0x00010246  [PF ZF IF]");
}

TEST(report_stack_trace) {
    Scenario s;
    s.info->stackDumpCount = 5;
    for (uint32_t i = 0; i < 5; ++i) s.info->stackDump[i] = 0x1000 + i;
    s.info->frames[0].address = 0x53BEE0 + 4;
    s.info->frames[0].stackOffset = 0x1C;
    s.info->frameCount = 1;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "Raw stack (ESP = 0x0019FF00):");
    CHECK_CONTAINS(r, "0019FF00: 00001000 00001001 00001002 00001003");
    CHECK_CONTAINS(r, "0019FF10: 00001004\n");
    CHECK_CONTAINS(r, "#00  EIP");
    CHECK_CONTAINS(r, "#01  ESP+0x1C");
    CHECK_CONTAINS(r, "gta_sa.exe+0x13BEE4 (~CGame::Process+0x4)");
}

TEST(report_memory_section) {
    Scenario s;
    s.info->memory.valid = true;
    s.info->memory.userSpaceTotal = 4096ull << 20;
    s.info->memory.addressFree = 512ull << 20;
    s.info->memory.addressLargestFree = 128ull << 20;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "Address space : 512.0 MB free, largest free block 128.0 MB (of 4096.0 MB user space)");
}

TEST(report_module_list) {
    Scenario s;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "3 modules loaded:");
    CHECK_CONTAINS(r, "00400000 - 00BFFFFF  004C2B5C  1.0.0.0");
    CHECK_CONTAINS(r, "gta_sa.exe [exe]");
    CHECK_CONTAINS(r, "CrashLogger.SA.asi");
    CHECK_CONTAINS(r, "kernel32.dll");
}

TEST(report_game_state_not_read) {
    Scenario s;
    CHECK_CONTAINS(s.Render(), "Game state was not read.");
}

TEST(report_game_state_wrong_exe_version) {
    Scenario s;
    s.info->game.versionKnown = true;
    s.info->game.versionOk = false;
    CHECK_CONTAINS(s.Render(), "not GTA SA 1.0 US");
}

TEST(report_game_state_full) {
    Scenario s;
    GameState& g = s.info->game;
    g.versionKnown = g.versionOk = true;
    g.timeValid = true;
    g.gameTimeMs = 123456;

    g.scriptsValid = true;
    g.scriptCount = 2;
    CopyString(g.scripts[0].name, sizeof(g.scripts[0].name), "main");
    g.scripts[0].ipValid = true;
    g.scripts[0].ipOffset = 0x1A2B;
    CopyString(g.scripts[1].name, sizeof(g.scripts[1].name), "SWEET1");
    g.scripts[1].isMission = true;
    g.scripts[1].ipValid = true;
    g.scripts[1].ipOffset = 0x40;

    g.playerValid = true;
    g.position[0] = 2495.5f;
    g.position[1] = -1687.25f;
    g.position[2] = 13.5f;
    g.playerModel = 0;
    g.areaValid = true;
    g.currentArea = 3;
    g.playerArea = 3;
    g.vehicleValid = true;
    g.inVehicle = true;
    g.vehicleModel = 411;

    g.streamingMemoryValid = true;
    g.streamingMemoryUsed = 64u << 20;
    g.streamingMemoryAvailable = 128u << 20;

    g.inFlightValid = true;
    g.inFlightTotal = 3;
    g.inFlightCount = 2;
    g.inFlight[0] = {411, 2, 0};
    g.inFlight[1] = {20123, 3, 0};

    g.historyCount = 2;
    g.history[0] = {411, 2, 15};
    g.history[1] = {7, 3, 480};
    s.info->streamingHistoryLimit = 32;

    std::string r = s.Render();
    CHECK_CONTAINS(r, "Game time     : 123456 ms");
    CHECK_CONTAINS(r, "Mission       : SWEET1 (script offset 0x40)");
    CHECK_CONTAINS(r, "Active scripts: 2");
    CHECK_CONTAINS(r, "main");
    CHECK_CONTAINS(r, "[mission]");
    CHECK_CONTAINS(r, "Player        : x=2495.50 y=-1687.25 z=13.50");
    CHECK_CONTAINS(r, "Interior      : area 3 (player area code 3)");
    CHECK_CONTAINS(r, "Vehicle       : model 411");
    CHECK_CONTAINS(r, "Streaming mem : 64.0 MB used of 128.0 MB (50%)");
    CHECK_CONTAINS(r, "in flight at crash time: 3");
    CHECK_CONTAINS(r, "20123  txd    channelled");
    CHECK_CONTAINS(r, "... 1 more");
    CHECK_CONTAINS(r, "Last 2 streaming requests seen (newest first");
    CHECK_CONTAINS(r, "15 ms before crash");
    CHECK_CONTAINS(r, "480 ms before crash");
}

TEST(report_game_state_no_mission_and_on_foot) {
    Scenario s;
    GameState& g = s.info->game;
    g.versionKnown = g.versionOk = true;
    g.scriptsValid = true;
    g.scriptCount = 1;
    CopyString(g.scripts[0].name, sizeof(g.scripts[0].name), "main");
    g.playerValid = true;
    g.vehicleValid = true;
    g.inVehicle = false;
    g.areaValid = true;
    g.currentArea = 0;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "Mission       : none running");
    CHECK_CONTAINS(r, "Vehicle       : on foot");
    CHECK_CONTAINS(r, "area 0 (outside)");
}

TEST(report_history_respects_limit) {
    Scenario s;
    GameState& g = s.info->game;
    g.versionKnown = g.versionOk = true;
    g.historyCount = 5;
    for (uint32_t i = 0; i < 5; ++i) g.history[i] = {100 + i, 2, i};
    s.info->streamingHistoryLimit = 2;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "Last 2 streaming requests");
    CHECK_CONTAINS(r, "100  model");
    CHECK_CONTAINS(r, "101  model");
    CHECK(!sacl_test::Contains(r, "102  model"));
}

TEST(report_survives_missing_module_table) {
    Scenario s;
    s.info->modules = nullptr;
    std::string r = s.Render();
    CHECK_CONTAINS(r, "<no module information>");
    CHECK_CONTAINS(r, "End of crash log");
}

TEST(report_truncation_is_reported_not_fatal) {
    Scenario s;
    char tiny[200];
    TextBuffer out(tiny, sizeof(tiny));
    WriteReport(out, *s.info);
    CHECK(out.Truncated());
    CHECK(tiny[sizeof(tiny) - 1] == '\0' || out.Size() < sizeof(tiny));
}

TEST(streaming_id_kinds) {
    CHECK_STR(StreamingIdKind(0), "model");
    CHECK_STR(StreamingIdKind(19999), "model");
    CHECK_STR(StreamingIdKind(20000), "txd");
    CHECK_STR(StreamingIdKind(25000), "col");
    CHECK_STR(StreamingIdKind(25255), "ipl");
    CHECK_STR(StreamingIdKind(25575), "ifp");
    CHECK_STR(StreamingIdKind(26316), "invalid");
    CHECK_STR(StreamingStateName(2), "requested");
    CHECK_STR(StreamingStateName(99), "?");
}
