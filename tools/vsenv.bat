@echo off
rem Sets up the Visual Studio C++ environment for 32-bit (x86) output in the calling cmd session,
rem plus the CMake and Ninja that ship with Visual Studio.
rem Usage:  call tools\vsenv.bat || exit /b 1
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (echo vswhere.exe not found - install Visual Studio with the C++ x86 tools & exit /b 1)
rem vswhere runs as a plain command (not inside for /f): its path contains parentheses, which cmd mangles there.
set "VSTMP=%TEMP%\CrashLogger.SA.vsdir.txt"
"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%VSTMP%"
set "VSDIR="
set /p VSDIR=<"%VSTMP%"
del "%VSTMP%" 2>nul
if not defined VSDIR (echo Visual Studio with the C++ x86 toolset not found & exit /b 1)
call "%VSDIR%\VC\Auxiliary\Build\vcvarsamd64_x86.bat" >nul || exit /b 1
set "PATH=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
exit /b 0
