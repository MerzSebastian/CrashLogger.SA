// Maps GTA SA 1.0 US virtual addresses to function names using a plain-text
// data file (see data/gta_sa_1.0_us.symbols and tools/gen_symbols.py).
//
// File format, one symbol per line:
//     0x004087E0 CStreaming::RequestModel
//     0x004087E0 0x1C0 CStreaming::RequestModel     (optional size in bytes)
// '#' and '//' start a comment. Addresses are absolute VAs for an exe loaded at
// its preferred base 0x400000.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sacl {

struct Symbol {
    uint32_t address = 0;
    uint32_t size = 0; // 0 = unknown
    std::string name;
};

struct SymbolMatch {
    const Symbol* symbol = nullptr; // nullptr = no match
    uint32_t offset = 0;            // address - symbol->address
    bool approximate = false;       // symbol has no size, match is "nearest preceding within maxGap"
};

struct SymbolLoadResult {
    size_t loaded = 0;
    size_t skipped = 0; // malformed lines and duplicate addresses
};

class SymbolTable {
public:
    static constexpr uint32_t kExeImageBase = 0x400000;

    void Clear();
    SymbolLoadResult LoadFromText(const char* text, size_t length);
    // Returns false if the file cannot be opened.
    bool LoadFromFile(const char* path, SymbolLoadResult* result = nullptr);

    // Allocation-free; safe to call from the crash handler once loading is done.
    SymbolMatch Find(uint32_t virtualAddress, uint32_t maxGap) const noexcept;

    size_t Size() const noexcept { return m_symbols.size(); }
    const std::vector<Symbol>& Symbols() const noexcept { return m_symbols; }

private:
    std::vector<Symbol> m_symbols; // sorted by address, unique
};

} // namespace sacl
