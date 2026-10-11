cmake_minimum_required(VERSION 3.20)

project(CrashLoggerSA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SACL_BUILD_PLUGIN "Build the CrashLogger.SA.asi plugin (needs a 32-bit MSVC toolchain)" ON)
option(SACL_BUILD_TESTS "Build the unit tests" ON)

# Static CRT: the .asi must not depend on a Visual C++ redistributable being
# installed next to a 2004 game.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------------------
# Portable core: formatting, symbol lookup, stack heuristics. No Windows headers.
# ---------------------------------------------------------------------------
add_library(sacl_core STATIC
    src/core/text_buffer.cpp
    src/core/exception_info.cpp
    src/core/module_table.cpp
    src/core/symbol_table.cpp
    src/core/stack_scan.cpp
    src/core/path_utils.cpp
    src/core/game_state.cpp
    src/core/report.cpp)
target_include_directories(sacl_core PUBLIC src/core)
target_compile_definitions(sacl_core PRIVATE _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(sacl_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(sacl_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# The ASI plugin (Win32 only: GTA SA is a 32-bit process)
# ---------------------------------------------------------------------------
if(SACL_BUILD_PLUGIN)
    if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(WARNING
            "CrashLogger.SA.asi must be built with MSVC for Win32 (x86). "
            "Skipping the plugin target; configure with -A Win32 or use the 'vs-x86' preset.")
    else()
        enable_language(RC)
        add_library(CrashLoggerSA SHARED
            src/plugin/dllmain.cpp
            src/plugin/config.cpp
            src/plugin/crash_handler.cpp
            src/plugin/game_reader.cpp
            src/plugin/streaming_monitor.cpp
            src/plugin/system_info.cpp
            src/plugin/version.rc)
        target_link_libraries(CrashLoggerSA PRIVATE sacl_core shell32 advapi32)
        target_compile_definitions(CrashLoggerSA PRIVATE
            WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)
        target_compile_options(CrashLoggerSA PRIVATE /W4 /permissive- /EHsc /GS-)
        set_target_properties(CrashLoggerSA PROPERTIES
            OUTPUT_NAME "CrashLogger.SA"
            SUFFIX ".asi"
            PREFIX "")

        # Put the data files next to the built plugin so it can be copied as a folder.
        add_custom_command(TARGET CrashLoggerSA POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/data/CrashLogger.SA.ini
                ${CMAKE_SOURCE_DIR}/data/gta_sa_1.0_us.symbols
                $<TARGET_FILE_DIR:CrashLoggerSA>
            COMMENT "Copying ini and symbol file next to CrashLogger.SA.asi")

        if(SACL_BUILD_TESTS)
            # Stand-in for gta_sa.exe that loads the .asi and crashes on purpose.
            add_executable(sacl_crash_host tests/crash_host/main.cpp)
            target_compile_options(sacl_crash_host PRIVATE /W4 /EHsc)
            # Fixed, high image base so the fake RsGlobal page (0xC17000) is always free.
            target_link_options(sacl_crash_host PRIVATE /DYNAMICBASE:NO /FIXED /BASE:0x30000000)
            add_dependencies(sacl_crash_host CrashLoggerSA)
        endif()

        install(TARGETS CrashLoggerSA RUNTIME DESTINATION . LIBRARY DESTINATION .)
        install(FILES data/CrashLogger.SA.ini data/gta_sa_1.0_us.symbols DESTINATION .)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests (run on any architecture: they only use the portable core)
# ---------------------------------------------------------------------------
if(SACL_BUILD_TESTS)
    enable_testing()
    add_executable(sacl_tests
        tests/test_main.cpp
        tests/test_text_buffer.cpp
        tests/test_exception_info.cpp
        tests/test_symbol_table.cpp
        tests/test_stack_scan.cpp
        tests/test_path_utils.cpp
        tests/test_report.cpp)
    target_link_libraries(sacl_tests PRIVATE sacl_core)
    target_compile_definitions(sacl_tests PRIVATE
        SACL_SOURCE_DIR="${CMAKE_SOURCE_DIR}" _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(sacl_tests PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME sacl_tests COMMAND sacl_tests)
endif()
