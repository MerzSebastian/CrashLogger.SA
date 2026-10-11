#include "symbol_table.hpp"
#include "test_framework.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace sacl;

namespace {
SymbolLoadResult Load(SymbolTable& t, const std::string& text) {
    return t.LoadFromText(text.data(), text.size());
}
} // namespace

TEST(symbols_parse_basic_and_comments) {
    SymbolTable t;
    SymbolLoadResult r = Load(t,
                              "# header comment\n"
                              "\n"
                              "0x004087E0 CStreaming::RequestModel\n"
                              "0040EA10 CStreaming::LoadAllRequestedModels   // trailing comment\n"
                              "  0x53BEE0\tCGame::Process\r\n");
    CHECK(r.loaded == 3);
    CHECK(r.skipped == 0);
    CHECK(t.Size() == 3);
    CHECK_STR(t.Symbols()[0].name, "CStreaming::RequestModel");
    CHECK(t.Symbols()[0].address == 0x4087E0u);
    CHECK_STR(t.Symbols()[1].name, "CStreaming::LoadAllRequestedModels");
}

TEST(symbols_sorted_regardless_of_file_order) {
    SymbolTable t;
    Load(t, "0x500000 C\n0x100000 A\n0x300000 B\n");
    CHECK(t.Symbols()[0].address == 0x100000u);
    CHECK(t.Symbols()[1].address == 0x300000u);
    CHECK(t.Symbols()[2].address == 0x500000u);
}

TEST(symbols_malformed_lines_are_skipped) {
    SymbolTable t;
    SymbolLoadResult r = Load(t,
                              "zzzz not_hex\n"
                              "0x401000\n"            // no name
                              "0x0 NullAddress\n"      // address 0 is rejected
                              "0x123456789 TooLong\n"  // more than 32 bits
                              "0x401000 Good\n");
    CHECK(r.loaded == 1);
    CHECK(r.skipped == 4);
    CHECK(t.Size() == 1);
}

TEST(symbols_duplicate_address_first_wins) {
    SymbolTable t;
    SymbolLoadResult r = Load(t, "0x401000 First\n0x401000 Second\n");
    CHECK(r.loaded == 1);
    CHECK(r.skipped == 1);
    CHECK_STR(t.Symbols()[0].name, "First");
}

TEST(symbols_optional_size_column) {
    SymbolTable t;
    Load(t, "0x401000 0x40 Sized\n0x402000 Unsized\n0x403000 Dead\n");
    CHECK(t.Symbols()[0].size == 0x40);
    CHECK_STR(t.Symbols()[0].name, "Sized");
    CHECK(t.Symbols()[1].size == 0);
    // A name that merely looks like hex must not be eaten as a size.
    CHECK_STR(t.Symbols()[2].name, "Dead");
}

TEST(symbols_find_exact_and_offset) {
    SymbolTable t;
    Load(t, "0x401000 A\n0x402000 B\n");
    SymbolMatch m = t.Find(0x401000, 0x1000);
    CHECK(m.symbol != nullptr);
    CHECK(m.offset == 0);
    m = t.Find(0x401234, 0x1000);
    CHECK(m.symbol != nullptr);
    CHECK_STR(m.symbol->name, "A");
    CHECK(m.offset == 0x234);
    m = t.Find(0x402010, 0x1000);
    CHECK_STR(m.symbol->name, "B");
    CHECK(m.offset == 0x10);
}

TEST(symbols_unsized_match_is_approximate_and_gap_limited) {
    SymbolTable t;
    Load(t, "0x401000 A\n");
    SymbolMatch m = t.Find(0x401010, 0x100);
    CHECK(m.symbol != nullptr);
    CHECK(m.approximate);
    m = t.Find(0x401101, 0x100); // 0x101 past the symbol, gap limit is 0x100
    CHECK(m.symbol == nullptr);
}

TEST(symbols_sized_match_is_exact_and_bounded) {
    SymbolTable t;
    Load(t, "0x401000 0x20 A\n");
    SymbolMatch m = t.Find(0x40101F, 0xFFFF);
    CHECK(m.symbol != nullptr);
    CHECK(!m.approximate);
    m = t.Find(0x401020, 0xFFFF); // one past the end: size wins over maxGap
    CHECK(m.symbol == nullptr);
}

TEST(symbols_find_before_first_and_in_empty_table) {
    SymbolTable t;
    CHECK(t.Find(0x401000, 0x1000).symbol == nullptr);
    Load(t, "0x401000 A\n");
    CHECK(t.Find(0x400FFF, 0x1000).symbol == nullptr);
}

TEST(symbols_load_from_file_and_missing_file) {
    SymbolTable t;
    CHECK(!t.LoadFromFile("this/file/does/not/exist.symbols"));

    std::string path = std::string(SACL_SOURCE_DIR) + "/data/gta_sa_1.0_us.symbols";
    SymbolLoadResult r;
    CHECK(t.LoadFromFile(path.c_str(), &r));
    CHECK(r.loaded > 0);
    CHECK(r.skipped == 0); // the shipped file must be clean
    SymbolMatch m = t.Find(0x4087E0, 16);
    CHECK(m.symbol != nullptr);
    if (m.symbol) CHECK_STR(m.symbol->name, "CStreaming::RequestModel");
}
