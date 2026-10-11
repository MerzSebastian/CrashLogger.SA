#include "report.hpp"

namespace sacl {

namespace {

constexpr const char* kRule =
    "------------------------------------------------------------------------------\n";
constexpr const char* kThickRule =
    "==============================================================================\n";

void Section(TextBuffer& out, const char* title) {
    out.Append("\n");
    out.Append(kRule);
    out.Printf(" %s\n", title);
    out.Append(kRule);
}

// "0x004087E0  gta_sa.exe+0x87E0 (CStreaming::RequestModel+0x0)"
void AddressWithLocation(TextBuffer& out, uint32_t address, const CrashInfo& ci) {
    out.Printf("0x%08X  ", address);
    if (ci.modules) {
        FormatLocation(out, address, *ci.modules, ci.symbols, ci.symbolMaxGap);
    } else {
        out.Append("<no module information>");
    }
}

void Register(TextBuffer& out, const char* name, uint32_t value, const CrashInfo& ci) {
    out.Printf("  %-3s = 0x%08X", name, value);
    const ModuleInfo* m = ci.modules ? ci.modules->Find(value) : nullptr;
    if (m) {
        out.Append("   ");
        FormatLocation(out, value, *ci.modules, ci.symbols, ci.symbolMaxGap);
    }
    out.Append("\n");
}

void WriteException(TextBuffer& out, const CrashInfo& ci) {
    const ExceptionData& e = ci.exception;
    out.Printf("Code          : 0x%08X  %s\n", e.code, ExceptionCodeName(e.code));
    out.Printf("Flags         : 0x%08X%s\n", e.flags, (e.flags & 1) ? " (non-continuable)" : "");
    out.Append("Fault address : ");
    AddressWithLocation(out, e.address, ci);
    out.Append("\n");

    AccessKind kind = GetAccessKind(e);
    if (kind != AccessKind::None) {
        uint32_t target = e.params[1];
        out.Printf("Access        : %s of address 0x%08X\n", AccessKindName(kind), target);
        if (const char* hint = DescribeSuspiciousAddress(target)) {
            out.Printf("Hint          : %s\n", hint);
        }
        if (e.code == 0xC0000006u && e.numParams >= 3) {
            out.Printf("NTSTATUS      : 0x%08X (in-page I/O error)\n", e.params[2]);
        }
    } else if (e.numParams > 0) {
        out.Append("Parameters    :");
        for (uint32_t i = 0; i < e.numParams && i < 15; ++i) out.Printf(" 0x%08X", e.params[i]);
        out.Append("\n");
    }

    out.Printf("Thread        : %u%s\n", ci.faultingThreadId,
               ci.faultingThreadId == ci.mainThreadId ? " (main thread)" : " (NOT the main thread)");

    if (ci.codeBytesValid) {
        out.Append("Code at EIP   :");
        for (size_t i = 0; i < sizeof(ci.codeBytes); ++i) out.Printf(" %02X", ci.codeBytes[i]);
        out.Append("\n");
    } else {
        out.Append("Code at EIP   : <unreadable>\n");
    }
}

void WriteRegisters(TextBuffer& out, const CrashInfo& ci) {
    const Registers& r = ci.regs;
    Register(out, "EAX", r.eax, ci);
    Register(out, "EBX", r.ebx, ci);
    Register(out, "ECX", r.ecx, ci);
    Register(out, "EDX", r.edx, ci);
    Register(out, "ESI", r.esi, ci);
    Register(out, "EDI", r.edi, ci);
    Register(out, "EBP", r.ebp, ci);
    Register(out, "ESP", r.esp, ci);
    Register(out, "EIP", r.eip, ci);
    out.Printf("  EFLAGS = 0x%08X  [", r.eflags);
    FormatEflags(out, r.eflags);
    out.Append("]\n");
    out.Printf("  CS=%04X DS=%04X ES=%04X FS=%04X GS=%04X SS=%04X\n", r.cs, r.ds, r.es, r.fs, r.gs,
               r.ss);
}

void WriteStack(TextBuffer& out, const CrashInfo& ci) {
    if (ci.stackDumpCount == 0) {
        out.Append("Stack memory at ESP is unreadable.\n");
    } else {
        out.Printf("Raw stack (ESP = 0x%08X):\n", ci.regs.esp);
        for (uint32_t i = 0; i < ci.stackDumpCount; i += 4) {
            out.Printf("  %08X:", ci.regs.esp + i * 4);
            for (uint32_t j = i; j < i + 4 && j < ci.stackDumpCount; ++j) {
                out.Printf(" %08X", ci.stackDump[j]);
            }
            out.Append("\n");
        }
    }

    out.Append("\nCall stack (heuristic: stack words that point just after a CALL; may contain\n"
               "stale entries and may miss frames):\n");
    out.Append("  #00  EIP        ");
    AddressWithLocation(out, ci.regs.eip, ci);
    out.Append("\n");
    for (uint32_t i = 0; i < ci.frameCount; ++i) {
        out.Printf("  #%02u  ESP+0x%-4X ", i + 1, ci.frames[i].stackOffset);
        AddressWithLocation(out, ci.frames[i].address, ci);
        out.Append("\n");
    }
    if (ci.frameCount == 0) out.Append("  (no return addresses found)\n");
}

void WriteMemory(TextBuffer& out, const CrashInfo& ci) {
    const MemoryInfo& m = ci.memory;
    if (!m.valid) {
        out.Append("Memory information unavailable.\n");
        return;
    }
    out.Append("Address space : ");
    FormatByteSize(out, m.addressFree);
    out.Append(" free, largest free block ");
    FormatByteSize(out, m.addressLargestFree);
    out.Append(" (of ");
    FormatByteSize(out, m.userSpaceTotal);
    out.Append(" user space)\n");
    out.Append("                ");
    FormatByteSize(out, m.addressCommitted);
    out.Append(" committed, ");
    FormatByteSize(out, m.addressReserved);
    out.Append(" reserved\n");
    out.Append("Physical RAM  : ");
    FormatByteSize(out, m.physicalAvailable);
    out.Append(" free of ");
    FormatByteSize(out, m.physicalTotal);
    out.Append("\nPage file     : ");
    FormatByteSize(out, m.pageFileAvailable);
    out.Append(" free of ");
    FormatByteSize(out, m.pageFileTotal);
    out.Append("\n");
}

void WriteStreamEntries(TextBuffer& out, const StreamEntry* entries, uint32_t count, bool showAge) {
    for (uint32_t i = 0; i < count; ++i) {
        out.Printf("    %5u  %-5s  %-10s", entries[i].modelId, StreamingIdKind(entries[i].modelId),
                   StreamingStateName(entries[i].state));
        if (showAge) out.Printf("  %u ms before crash", entries[i].ageMs);
        out.Append("\n");
    }
}

void WriteGame(TextBuffer& out, const CrashInfo& ci) {
    const GameState& g = ci.game;
    if (!g.versionKnown) {
        out.Append("Game state was not read.\n");
        return;
    }
    if (!g.versionOk) {
        out.Append("The executable is not GTA SA 1.0 US; game state was NOT read because the\n"
                   "hard-coded addresses would be meaningless.\n");
        return;
    }

    if (g.timeValid) out.Printf("Game time     : %u ms\n", g.gameTimeMs);

    if (g.scriptsValid) {
        const ScriptEntry* mission = nullptr;
        uint32_t shown = g.scriptCount < kMaxScripts ? g.scriptCount : kMaxScripts;
        for (uint32_t i = 0; i < shown; ++i) {
            if (g.scripts[i].isMission) {
                mission = &g.scripts[i];
                break;
            }
        }
        if (mission) {
            out.Printf("Mission       : %s", mission->name);
            if (mission->ipValid) out.Printf(" (script offset 0x%X)", mission->ipOffset);
            out.Append("\n");
        } else {
            out.Append(g.missionFlagUnreliable
                           ? "Mission       : unknown (script flags unreliable; newest script is listed first)\n"
                           : "Mission       : none running\n");
        }
        out.Printf("Active scripts: %u\n", g.scriptCount);
        for (uint32_t i = 0; i < shown; ++i) {
            const ScriptEntry& s = g.scripts[i];
            out.Printf("    %-8s", s.name);
            if (s.ipValid) out.Printf("  offset 0x%X", s.ipOffset);
            if (s.isMission) out.Append("  [mission]");
            out.Append("\n");
        }
        if (g.scriptCount > shown) out.Printf("    ... %u more\n", g.scriptCount - shown);
    } else {
        out.Append("Scripts       : unreadable\n");
    }

    if (g.playerValid) {
        out.Printf("Player        : x=%.2f y=%.2f z=%.2f\n", g.position[0], g.position[1],
                   g.position[2]);
        out.Printf("Player model  : %u\n", g.playerModel);
        if (g.areaValid) {
            out.Printf("Interior      : area %d%s (player area code %d)\n", g.currentArea,
                       g.currentArea == 0 ? " (outside)" : "", g.playerArea);
        }
        if (g.vehicleValid && g.inVehicle) {
            out.Printf("Vehicle       : model %u\n", g.vehicleModel);
        } else {
            out.Append("Vehicle       : on foot\n");
        }
    } else {
        out.Append("Player        : unreadable (not spawned yet?)\n");
    }

    if (g.streamingMemoryValid) {
        out.Append("Streaming mem : ");
        FormatByteSize(out, g.streamingMemoryUsed);
        out.Append(" used of ");
        FormatByteSize(out, g.streamingMemoryAvailable);
        if (g.streamingMemoryAvailable) {
            out.Printf(" (%u%%)",
                       static_cast<uint32_t>((static_cast<uint64_t>(g.streamingMemoryUsed) * 100) /
                                             g.streamingMemoryAvailable));
        }
        out.Append("\n");
    } else {
        out.Append("Streaming mem : unreadable\n");
    }

    if (g.inFlightValid) {
        out.Printf("\nStreaming requests in flight at crash time: %u\n", g.inFlightTotal);
        WriteStreamEntries(out, g.inFlight, g.inFlightCount, false);
        if (g.inFlightTotal > g.inFlightCount) {
            out.Printf("    ... %u more\n", g.inFlightTotal - g.inFlightCount);
        }
    }

    uint32_t historyCount = g.historyCount;
    if (historyCount > ci.streamingHistoryLimit) historyCount = ci.streamingHistoryLimit;
    out.Printf("\nLast %u streaming requests seen (newest first; sampled, may miss very short ones):\n",
               historyCount);
    if (historyCount == 0) out.Append("    (none recorded)\n");
    WriteStreamEntries(out, g.history, historyCount, true);
}

void WriteModules(TextBuffer& out, const CrashInfo& ci) {
    if (!ci.modules) return;
    const ModuleTable& t = *ci.modules;
    out.Printf("%u modules loaded:\n", static_cast<uint32_t>(t.Count()));
    out.Append("  Base       End        Stamp     Version          Name  (path)\n");
    for (size_t i = 0; i < t.Count(); ++i) {
        const ModuleInfo& m = t.At(i);
        out.Printf("  %08X - %08X  %08X  %-15s  %s%s", m.base, m.base + m.size - 1, m.timestamp,
                   m.version[0] ? m.version : "-", m.name, m.isMainExe ? " [exe]" : "");
        out.Printf("  (%s)\n", m.path);
    }
}

} // namespace

void FormatEflags(TextBuffer& out, uint32_t eflags) {
    static const struct {
        uint32_t bit;
        const char* name;
    } kFlags[] = {{0, "CF"}, {2, "PF"}, {4, "AF"}, {6, "ZF"}, {7, "SF"},
                  {8, "TF"}, {9, "IF"}, {10, "DF"}, {11, "OF"}};
    bool first = true;
    for (const auto& f : kFlags) {
        if (eflags & (1u << f.bit)) {
            if (!first) out.AppendChar(' ');
            out.Append(f.name);
            first = false;
        }
    }
}

void FormatByteSize(TextBuffer& out, uint64_t bytes) {
    // One decimal place, computed in integers so output is exact and locale-free.
    uint64_t tenthsMb = (bytes * 10 + (1024 * 1024) / 2) / (1024 * 1024);
    out.Printf("%u.%u MB", static_cast<uint32_t>(tenthsMb / 10), static_cast<uint32_t>(tenthsMb % 10));
}

bool FormatLocation(TextBuffer& out, uint32_t address, const ModuleTable& modules,
                    const SymbolTable* symbols, uint32_t symbolMaxGap) {
    const ModuleInfo* m = modules.Find(address);
    if (!m) {
        out.Append("<not in any module>");
        return false;
    }
    uint32_t offset = address - m->base;
    out.Printf("%s+0x%X", m->name, offset);

    if (m->isMainExe && symbols && symbols->Size() > 0) {
        // The data file uses VAs for an exe at its preferred base; rebase in
        // case the loader had to move it.
        uint32_t va = SymbolTable::kExeImageBase + offset;
        SymbolMatch match = symbols->Find(va, symbolMaxGap);
        if (match.symbol) {
            out.Printf(" (%s%s+0x%X)", match.approximate ? "~" : "", match.symbol->name.c_str(),
                       match.offset);
        }
    }
    return true;
}

void WriteReport(TextBuffer& out, const CrashInfo& ci) {
    out.Append(kThickRule);
    out.Printf(" GTA San Andreas crash log  -  CrashLogger.SA %s\n", kVersionString);
    out.Append(kThickRule);
    out.Printf("Time          : %s (local)\n", ci.timestamp);
    out.Printf("Detected by   : %s\n", ci.source);
    out.Printf("Executable    : %s\n", ci.exePath);
    out.Printf("OS            : %s\n", ci.osVersion);

    Section(out, "EXCEPTION");
    WriteException(out, ci);

    Section(out, "REGISTERS");
    WriteRegisters(out, ci);

    Section(out, "STACK");
    WriteStack(out, ci);

    Section(out, "MEMORY");
    WriteMemory(out, ci);

    Section(out, "GAME STATE");
    WriteGame(out, ci);

    Section(out, "MODULES");
    WriteModules(out, ci);

    out.Append("\n");
    out.Append(kThickRule);
    out.Append(" End of crash log\n");
    out.Append(kThickRule);
    if (out.Truncated()) out.Append("[log truncated: buffer full]\n");
}

} // namespace sacl
