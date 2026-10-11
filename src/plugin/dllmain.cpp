// CrashLogger.SA - crash logger ASI plugin for GTA San Andreas 1.0 US.
// Load with Ultimate ASI Loader. GPL-3.0-or-later, see LICENSE.
#include "crash_handler.hpp"

#include <windows.h>

BOOL APIENTRY DllMain(HINSTANCE instance, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        sacl::InstallCrashHandler(instance);
    }
    return TRUE;
}
