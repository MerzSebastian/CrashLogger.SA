@echo off
rem Builds CrashLogger.SA.asi (32-bit, static CRT), runs the unit tests and copies the plugin with its
rem ini and symbol file into .\out. Uses the x86 MSVC toolchain and the CMake/Ninja that ship with Visual Studio.
setlocal
call tools\vsenv.bat || exit /b 1
cd /d "%~dp0"
cmake -S . -B build\dev -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build\dev || exit /b 1
ctest --test-dir build\dev --output-on-failure || exit /b 1
if not exist out mkdir out
copy /y build\dev\CrashLogger.SA.asi out\ >nul || exit /b 1
copy /y data\CrashLogger.SA.ini out\ >nul || exit /b 1
copy /y data\gta_sa_1.0_us.symbols out\ >nul || exit /b 1
echo.
echo Built out\CrashLogger.SA.asi
