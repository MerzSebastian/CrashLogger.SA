#pragma once

#include <windows.h>

namespace sacl {

// Installs the vectored handler and the unhandled-exception filter, and starts the
// worker threads. `self` is this plugin's module handle. Safe to call from
// DllMain: it only registers handlers, creates threads and reads the ini.
void InstallCrashHandler(HINSTANCE self);

} // namespace sacl
