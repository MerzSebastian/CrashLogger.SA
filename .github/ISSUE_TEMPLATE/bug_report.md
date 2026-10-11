---
name: Bug report
about: The logger itself misbehaves (no report after a crash, wrong module or function names, a crash caused by the logger, ...)
labels: bug
---

**What happened, and what did you expect?**

**Setup**
- Plugin version (first lines of `crashlog.txt`, "CrashLogger.SA x.y.z"):
- Game: 1.0 US exe? Other mods that handle crashes or hook the same places (CLEO, modloader, widescreen fix, ...):
- ASI loader:

**The report**

Attach `crashlog.txt` (next to `gta_sa.exe`; if the game is in `Program Files`, Windows may have put it in
`%LocalAppData%\VirtualStore\Program Files (x86)\Rockstar Games\Grand Theft Auto San Andreas\`).
The report contains your Windows version, folder names and the list of loaded mods: look through it and remove what you do not want to share.
Do not attach the `.dmp` unless asked; it contains memory of the game.

**Your `CrashLogger.SA.ini`** (if you changed it)
