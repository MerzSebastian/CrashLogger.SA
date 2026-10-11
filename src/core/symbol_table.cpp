#include "symbol_table.hpp"

#include <algorithm>
#include <cstdio>

namespace sacl {

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

// Parses a hex number with optional 0x prefix. Rejects empty/over-long input.
bool ParseHex(const std::string& s, uint32_t& out) {
    size_t i = 0;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) i = 2;
    if (i >= s.size() || s.size() - i > 8) return false;
    uint32_t v = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        uint32_t d;
        if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
        else return false;
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

bool HasHexPrefix(const std::string& s) {
    return s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
}

// Splits one line into whitespace separated tokens, dropping comments.
void Tokenize(const char* begin, const char* end, std::vector<std::string>& tokens) {
    tokens.clear();
    const char* p = begin;
    while (p < end) {
        while (p < end && IsSpace(*p)) ++p;
        if (p >= end) break;
        if (*p == '#' || (*p == '/' && p + 1 < end && p[1] == '/')) break;
        const char* start = p;
        while (p < end && !IsSpace(*p)) ++p;
        tokens.emplace_back(start, p);
    }
}

} // namespace

void SymbolTable::Clear() {
    m_symbols.clear();
}

SymbolLoadResult SymbolTable::LoadFromText(const char* text, size_t length) {
    SymbolLoadResult result;
    std::vector<Symbol> parsed;
    std::vector<std::string> tokens;

    const char* p = text;
    const char* end = text + length;
    while (p < end) {
        const char* lineEnd = p;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;
        Tokenize(p, lineEnd, tokens);
        p = lineEnd < end ? lineEnd + 1 : end;

        if (tokens.empty()) continue;

        Symbol sym;
        size_t nameStart = 1;
        if (tokens.size() < 2 || !ParseHex(tokens[0], sym.address) || sym.address == 0) {
            ++result.skipped;
            continue;
        }
        // "addr size name": the size token must carry an explicit 0x prefix so a
        // name that happens to look like hex (e.g. "Dead") is never taken as one.
        uint32_t size = 0;
        if (tokens.size() >= 3 && HasHexPrefix(tokens[1]) && ParseHex(tokens[1], size)) {
            sym.size = size;
            nameStart = 2;
        }
        for (size_t i = nameStart; i < tokens.size(); ++i) {
            if (i > nameStart) sym.name += ' ';
            sym.name += tokens[i];
        }
        parsed.push_back(std::move(sym));
    }

    // Stable sort keeps file order for equal addresses so the first one wins.
    std::stable_sort(parsed.begin(), parsed.end(),
                     [](const Symbol& a, const Symbol& b) { return a.address < b.address; });

    for (Symbol& s : parsed) {
        if (!m_symbols.empty() && m_symbols.back().address == s.address) {
            ++result.skipped;
            continue;
        }
        m_symbols.push_back(std::move(s));
        ++result.loaded;
    }
    return result;
}

bool SymbolTable::LoadFromFile(const char* path, SymbolLoadResult* result) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::string data;
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
    std::fclose(f);

    // LoadFromText merges into whatever is already present; keep the contract
    // simple by always replacing.
    Clear();
    SymbolLoadResult r = LoadFromText(data.data(), data.size());
    if (result) *result = r;
    return true;
}

SymbolMatch SymbolTable::Find(uint32_t va, uint32_t maxGap) const noexcept {
    SymbolMatch match;
    auto it = std::upper_bound(m_symbols.begin(), m_symbols.end(), va,
                               [](uint32_t v, const Symbol& s) { return v < s.address; });
    if (it == m_symbols.begin()) return match;
    --it;

    uint32_t offset = va - it->address;
    if (it->size != 0) {
        if (offset >= it->size) return match;
    } else {
        if (offset > maxGap) return match;
        match.approximate = true;
    }
    match.symbol = &*it;
    match.offset = offset;
    return match;
}

} // namespace sacl
