# CrashLogger.SA

A crash logger for **GTA San Andreas v1.0 US** (32-bit). When the game crashes it writes a readable report that says
**where** it crashed (module and offset, function name if known), **what the game was doing** (time, mission, running
scripts, streaming) and **which mods were loaded** - so a crash can be traced to a mod instead of guessed at.

## What you get

After a crash:

- `crashlog.txt` - the report (sections below).
- `crashlog.dmp` - a minidump for a debugger (optional, on by default).
- A message box with the **exact location** of both (optional).

**Where are the files?** Next to `gta_sa.exe`. If the game is installed in `C:\Program Files (x86)`, Windows often redirects
the writes to the **VirtualStore** instead, so look there:
`%LOCALAPPDATA%\VirtualStore\Program Files (x86)\Rockstar Games\Grand Theft Auto San Andreas\`.
The message box names that path, and a fallback folder if the logger could not write anywhere else. The easiest fix is a game folder
outside `Program Files`.

The report contains:

| Section | What it tells you |
|---|---|
| **Exception** | Exception code, fault address with module and offset (`GTASA.WidescreenFix.asi+0x47AB0`), read/write access, and a hint such as "NULL pointer plus small offset" |
| **Registers, stack** | CPU registers (with the module of every address they point into) and the raw stack |
| **Call stack** | A heuristic stack trace: stack words that point just after a `CALL`. It can miss frames or show stale ones |
| **Memory** | Free address space, committed memory, RAM and page file |
| **Game state** | Game time, current mission, running scripts, player, streaming memory |
| **Streaming** | The most recent streaming requests (models and textures) before the crash |
| **Modules** | Every loaded DLL and ASI with address range and version, so you see which mods were active |

## Example

An abridged report (paths and the rest removed):

```
 GTA San Andreas crash log  -  CrashLogger.SA 1.0.0
------------------------------------------------------------------------------
 EXCEPTION
------------------------------------------------------------------------------
Code          : 0xC0000005  EXCEPTION_ACCESS_VIOLATION
Fault address : 0x71B77AB0  GTASA.WidescreenFix.asi+0x47AB0
Access        : read of address 0x3BB60B7A
Thread        : 23740 (main thread)
Code at EIP   : 8A 06 46 84 C0 75 F9 6A 06 2B F2 68 70 C1 C1 71

Call stack (heuristic: stack words that point just after a CALL; may contain
stale entries and may miss frames):
  #00  EIP        0x71B77AB0  GTASA.WidescreenFix.asi+0x47AB0
  #01  ESP+0x18   0x721D647B  d3d9.dll+0x6647B
  #02  ESP+0x1C   0x007FE742  gta_sa.exe+0x3FE742
  #03  ESP+0x3C   0x0059005E  gta_sa.exe+0x19005E
```

How to read it: the fault address names the module that crashed, here a plugin. The call stack shows who called it. A fault inside
`gta_sa.exe` with a call stack through a mod usually points at that mod's data or code.

## Requirements

- GTA San Andreas **1.0 US** (compact or Hoodlum exe). On other versions the logger still works, but game state fields are left out.
- An **ASI loader** that loads `.asi` files from `scripts\` (for example the Ultimate ASI Loader). It is not included.

## Install

Unpack the zip into the game folder so that `CrashLogger.SA.asi`, `CrashLogger.SA.ini` and `gta_sa_1.0_us.symbols` end up in
`scripts\`. Start the game. To uninstall, delete the three files. See "Where are the files?" above if you cannot find the report.

## Settings

`CrashLogger.SA.ini` (next to the `.asi`; every key is optional, the values shown are the defaults):

| Section | Key | Default | |
|---|---|---|---|
| `[General]` | `LogFile` | `crashlog.txt` | Report file name (plain name, no folders) |
| | `DumpFile` | `crashlog.dmp` | Minidump file name |
| | `WriteMinidump` | 1 | Write the minidump |
| | `DumpDataSegments` | 1 | Include the exe's global data (game state) in the minidump (a few MB more) |
| | `FullMemoryDump` | 0 | Write a full-memory dump instead (1-2 GB, for deep debugging only) |
| | `IgnoreShutdownCrashes` | 1 | Do not report the exceptions the game often throws while closing |
| | `TerminateAfterReport` | 1 | End the process after a report, so no frozen game window stays behind |
| | `ShowMessageBox` | 1 | Show a message box with the log location |
| | `LogFirstChance` | 0 | Also log fatal-looking exceptions as soon as they are raised, even if something might still handle them. Use it if a crash is swallowed and no report appears |
| `[Streaming]` | `Monitor` | 1 | Record recent streaming requests (a background thread polls the streaming table); 0 turns it off completely |
| | `SampleIntervalMs` | 25 | How often to poll |
| | `HistorySize` | 32 | How many recent requests the report lists (1-64) |
| `[Symbols]` | `File` | `gta_sa_1.0_us.symbols` | Address to function name file, searched next to the `.asi`, then in the game folder, then in `scripts\` |
| | `MaxGap` | 1024 | Unsized symbols match up to this many bytes past their start (shown as `~Name+0x..`) |

## Function names

The symbol file is a plain text list of `<address> <name>` lines (a small starter list is included). Addresses inside the game exe
are shown with the nearest known function. To add more names, append lines or generate a larger file from headers that document the
game's functions:

```
python tools/gen_symbols.py path/to/headers -o data/gta_sa_1.0_us.symbols
```

## Build

Needs Visual Studio with the "Desktop development with C++" workload (x86 tools). Run `build.bat`: it configures CMake with Ninja for
32-bit, builds, runs the unit tests and copies `CrashLogger.SA.asi`, the ini and the symbol file into `out\`.

Without `build.bat`: `cmake --preset ninja-x86` (from an x86 developer prompt) or `cmake --preset vs-x86`, then build and `ctest`.

The tests (`tests/`) cover the parts that do not touch the game: text formatting, exception decoding, module and symbol lookup,
stack scanning, path handling and the report layout. `tests/crash_host` is a stand-in for `gta_sa.exe` that loads the plugin and
crashes on purpose, for manual checks.

## How it works

- The plugin installs a top-level exception filter and a vectored handler. The vectored handler only logs when `LogFirstChance=1`;
  exceptions raised on the logger's own threads are ignored.
- Every read of game memory goes through a guarded read (`SafeRead`): a wrong address gives a missing field in the report, never a
  second crash. All game addresses are in `src/plugin/game_addresses.hpp`.
- The portable core (`src/core`) does the formatting, symbol lookup and stack heuristics and has no Windows headers, so it is unit tested.
- The stack scan lists words that point just after a `CALL` instruction into a module's code. It is a heuristic, not an unwinder.

## License

Copyright (C) 2026 Sebastian Merz. Free software under the **GNU General Public License, version 3 or (at your option) any later version** - see [LICENSE](LICENSE). The plugin comes with no
warranty. It is a fan project and not affiliated with Rockstar Games or Take-Two Interactive. It contains no game assets or game code.
