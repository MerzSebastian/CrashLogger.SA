#include "path_utils.hpp"
#include "test_framework.hpp"

using namespace sacl;

TEST(protected_directory_detection) {
    CHECK(IsUnderProtectedDirectory("C:\\Program Files (x86)\\Rockstar Games\\GTA San Andreas\\gta_sa.exe"));
    CHECK(IsUnderProtectedDirectory("c:\\program files\\Game\\x"));
    CHECK(IsUnderProtectedDirectory("C:\\Windows\\System32\\x"));
    CHECK(!IsUnderProtectedDirectory("D:\\Games\\GTA San Andreas\\gta_sa.exe"));
    CHECK(!IsUnderProtectedDirectory("C:\\Users\\me\\Documents\\x"));
    CHECK(!IsUnderProtectedDirectory(nullptr));
}

TEST(virtual_store_path) {
    char out[260];
    CHECK(ToVirtualStorePath("C:\\Program Files (x86)\\GTA\\crashlog.txt", "C:\\Users\\me\\AppData\\Local", out,
                             sizeof(out)));
    CHECK_STR(out, "C:\\Users\\me\\AppData\\Local\\VirtualStore\\Program Files (x86)\\GTA\\crashlog.txt");
    // A trailing backslash on the base must not double up.
    CHECK(ToVirtualStorePath("C:\\Program Files\\a.txt", "C:\\L\\", out, sizeof(out)));
    CHECK_STR(out, "C:\\L\\VirtualStore\\Program Files\\a.txt");
}

TEST(virtual_store_path_rejects_bad_input) {
    char out[16];
    CHECK(!ToVirtualStorePath("relative\\path.txt", "C:\\L", out, sizeof(out)));
    CHECK(!ToVirtualStorePath("\\\\server\\share\\a.txt", "C:\\L", out, sizeof(out)));
    CHECK(!ToVirtualStorePath("C:\\Program Files\\a-very-long-name.txt", "C:\\Users\\me\\AppData\\Local", out,
                              sizeof(out)));
    CHECK_STR(out, "");
}

TEST(win32_error_descriptions) {
    CHECK_STR(DescribeWin32Error(5), "access denied");
    CHECK_STR(DescribeWin32Error(32), "file in use by another process");
    CHECK(DescribeWin32Error(424242) == nullptr);
}
