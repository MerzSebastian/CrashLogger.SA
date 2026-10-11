# Changelog

## 1.0.0

First release.

- Writes a text report when the game crashes: exception and fault address (with the module and offset), registers,
  stack and a heuristic call stack, loaded modules, game state (time, mission, running scripts, player), memory use,
  the most recent streaming requests and system information.
- Optional minidump (`crashlog.dmp`), also with the exe's global data; a full-memory dump is available for deep debugging.
- Function names for addresses in the game exe from a small symbol file you can extend (`tools/gen_symbols.py`).
- Ignores the harmless exceptions the game throws while closing, ends the frozen process after a report and can show a
  message box with the log location.
- Settings in `CrashLogger.SA.ini`; works alongside other plugins.

Known limits: only GTA San Andreas 1.0 US (HOODLUM-compatible exe).
