#include "config.hpp"

#include "../core/text_buffer.hpp"

#include <windows.h>

namespace sacl {

namespace {

bool ReadBool(const char* section, const char* key, bool def, const char* path) {
    return GetPrivateProfileIntA(section, key, def ? 1 : 0, path) != 0;
}

uint32_t ReadUInt(const char* section, const char* key, uint32_t def, uint32_t lo, uint32_t hi,
                  const char* path) {
    int v = static_cast<int>(GetPrivateProfileIntA(section, key, static_cast<int>(def), path));
    if (v < 0) return def;
    uint32_t u = static_cast<uint32_t>(v);
    return u < lo ? lo : (u > hi ? hi : u);
}

// File names must stay plain names: no directories, so a hostile ini cannot make the
// handler write somewhere unexpected.
void ReadFileName(const char* section, const char* key, char* dst, size_t cap, const char* path) {
    char tmp[128];
    GetPrivateProfileStringA(section, key, dst, tmp, sizeof(tmp), path);
    for (const char* p = tmp; *p; ++p) {
        if (*p == '\\' || *p == '/' || *p == ':') return;
    }
    if (tmp[0]) CopyString(dst, cap, tmp);
}

} // namespace

void LoadConfig(const char* iniPath, Config& c) {
    ReadFileName("General", "LogFile", c.logFileName, sizeof(c.logFileName), iniPath);
    ReadFileName("General", "DumpFile", c.dumpFileName, sizeof(c.dumpFileName), iniPath);
    c.writeMinidump = ReadBool("General", "WriteMinidump", c.writeMinidump, iniPath);
    c.dumpDataSegments = ReadBool("General", "DumpDataSegments", c.dumpDataSegments, iniPath);
    c.fullMemoryDump = ReadBool("General", "FullMemoryDump", c.fullMemoryDump, iniPath);
    c.showMessageBox = ReadBool("General", "ShowMessageBox", c.showMessageBox, iniPath);
    c.ignoreShutdownCrashes = ReadBool("General", "IgnoreShutdownCrashes", c.ignoreShutdownCrashes, iniPath);
    c.terminateAfterReport = ReadBool("General", "TerminateAfterReport", c.terminateAfterReport, iniPath);
    c.logFirstChance = ReadBool("General", "LogFirstChance", c.logFirstChance, iniPath);

    c.monitorStreaming = ReadBool("Streaming", "Monitor", c.monitorStreaming, iniPath);
    c.sampleIntervalMs = ReadUInt("Streaming", "SampleIntervalMs", c.sampleIntervalMs, 5, 1000, iniPath);
    c.historySize = ReadUInt("Streaming", "HistorySize", c.historySize, 1, 64, iniPath);

    ReadFileName("Symbols", "File", c.symbolFile, sizeof(c.symbolFile), iniPath);
    c.symbolMaxGap = ReadUInt("Symbols", "MaxGap", c.symbolMaxGap, 0, 0x10000, iniPath);
}

} // namespace sacl
