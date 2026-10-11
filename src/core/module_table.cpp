#include "module_table.hpp"

#include "text_buffer.hpp"

namespace sacl {

bool ModuleTable::Add(uint32_t base, uint32_t size, uint32_t timestamp, bool isMainExe,
                      const char* name, const char* path, const char* version) noexcept {
    if (m_count >= kMaxModules || size == 0) return false;
    ModuleInfo& m = m_modules[m_count++];
    m.base = base;
    m.size = size;
    m.timestamp = timestamp;
    m.isMainExe = isMainExe;
    CopyString(m.name, sizeof(m.name), name);
    CopyString(m.path, sizeof(m.path), path);
    CopyString(m.version, sizeof(m.version), version);
    return true;
}

const ModuleInfo* ModuleTable::Find(uint32_t address) const noexcept {
    for (size_t i = 0; i < m_count; ++i) {
        const ModuleInfo& m = m_modules[i];
        // Subtract instead of adding so modules at the very top of the address
        // space cannot overflow.
        if (address >= m.base && address - m.base < m.size) return &m;
    }
    return nullptr;
}

const ModuleInfo* ModuleTable::MainExe() const noexcept {
    for (size_t i = 0; i < m_count; ++i) {
        if (m_modules[i].isMainExe) return &m_modules[i];
    }
    return nullptr;
}

} // namespace sacl
