// A snapshot of the loaded modules, filled in by the Windows layer at crash time
// and consumed by the (portable) report formatter.
#pragma once

#include <cstddef>
#include <cstdint>

namespace sacl {

struct ModuleInfo {
    uint32_t base = 0;
    uint32_t size = 0;
    uint32_t timestamp = 0; // PE TimeDateStamp
    bool isMainExe = false;
    char name[64] = {};
    char version[32] = {};
    char path[260] = {};
};

class ModuleTable {
public:
    static constexpr size_t kMaxModules = 512;

    void Clear() noexcept { m_count = 0; }

    // Returns false when the table is full or the range is empty.
    bool Add(uint32_t base, uint32_t size, uint32_t timestamp, bool isMainExe, const char* name,
             const char* path, const char* version) noexcept;

    // Module containing `address`, or nullptr.
    const ModuleInfo* Find(uint32_t address) const noexcept;
    const ModuleInfo* MainExe() const noexcept;

    size_t Count() const noexcept { return m_count; }
    const ModuleInfo& At(size_t i) const noexcept { return m_modules[i]; }

private:
    ModuleInfo m_modules[kMaxModules];
    size_t m_count = 0;
};

} // namespace sacl
