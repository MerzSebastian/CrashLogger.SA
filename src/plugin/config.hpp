#pragma once

#include <cstdint>

namespace sacl {

struct Config {
    char logFileName[64] = "crashlog.txt";
    char dumpFileName[64] = "crashlog.dmp";
    char symbolFile[128] = "gta_sa_1.0_us.symbols";

    bool writeMinidump = true;
    bool dumpDataSegments = true; // include the exe's globals in the minidump
    bool fullMemoryDump = false;
    bool showMessageBox = true;
    bool ignoreShutdownCrashes = true; // GTA SA often faults while exiting; not a real crash
    bool terminateAfterReport = true;  // end the process once the report is done instead of letting Windows hang
    bool logFirstChance = false;  // log fatal-looking exceptions even if they might be handled

    bool monitorStreaming = true;
    uint32_t sampleIntervalMs = 25;
    uint32_t historySize = 32;

    uint32_t symbolMaxGap = 1024;
};

// Reads `iniPath`; missing file or keys leave the defaults.
void LoadConfig(const char* iniPath, Config& cfg);

} // namespace sacl
