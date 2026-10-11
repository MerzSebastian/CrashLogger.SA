// Crash-time queries of process/OS state. None of these allocate from the heap or
// take loader/heap locks that the crashed thread could be holding.
#pragma once

#include "../core/module_table.hpp"
#include "../core/report.hpp"

namespace sacl {

// Walks the loader's module list (PEB) directly instead of using ToolHelp/PSAPI,
// which allocate and can deadlock if the crashed thread owned the heap lock.
void SnapshotModules(ModuleTable& table) noexcept;

void QueryMemoryInfo(MemoryInfo& info) noexcept;

// Looks up the version via RtlGetVersion (GetVersionEx lies on newer Windows).
void QueryOsVersion(char* out, size_t capacity) noexcept;

void FormatLocalTime(char* out, size_t capacity) noexcept;

} // namespace sacl
