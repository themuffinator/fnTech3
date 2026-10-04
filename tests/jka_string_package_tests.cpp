/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_string_package.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif
#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
namespace strings = fntech3::jka::strings;
int Synthetic() {
    strings::Table table;
    const auto text = "// header\r\nVERSION \"1\"\nCONFIG \"ignored\"\nREFERENCE Greeting\n"
        "LANG_ENGLISH \"Hello // internal \"quote\"\\nthere\" // comment\n"
        "LANG_FRENCH \"#same\"\nREFERENCE SECOND\nLANG_ENGLISH \"Base\"\nLANG_FRENCH \"Bonjour\"\nENDMARKER\n";
    CHECK(strings::Read("menu", "french", text, table));
    CHECK(strings::Lookup(table, "menu_greeting") == "Hello // internal \"quote\"\nthere");
    CHECK(strings::Lookup(table, "MENU_SECOND") == "Bonjour");
    CHECK(strings::Lookup(table, "missing").empty());
    char output[5]; std::memset(output, 0xa5, sizeof(output));
    CHECK(strings::Copy("abcdefgh", output+1, 3) && output[1] == 'a' && output[2] == 'b' && output[3] == 0);
    CHECK(static_cast<unsigned char>(output[0]) == 0xa5 && static_cast<unsigned char>(output[4]) == 0xa5);
    CHECK(!strings::Copy("a", nullptr, 3) && !strings::Copy("a", output, 0));
    CHECK(strings::Text("\x91\x92\x93\x94\x85\x96\x97\x0b\t?.", true) == "''\"\"...--. ? ");
    CHECK(strings::Text("\x80\xff", false) == "\x80\xff");
    const auto expected = table;
    for (const auto bad : {"VERSION 2\nENDMARKER", "VERSION 1\nREFERENCE x\nLANG_ENGLISH hi",
        "VERSION 1\nLANG_ENGLISH hi\nENDMARKER", "VERSION 1\nUNKNOWN x\nENDMARKER",
        "VERSION 1\nENDMARKER\nREFERENCE x", "VERSION 1\nREFERENCE x\nLANG_GERMAN hi\nENDMARKER",
        "VERSION 1\nREFERENCE x\nLANG_ENGLISH \"unfinished\nENDMARKER"}) {
        CHECK(!strings::Read("menu", "french", bad, table)); CHECK(table == expected);
    }
    const std::string longLine(strings::MaximumLine, 'a');
    CHECK(!strings::Read("menu", "english", "VERSION 1\n"+longLine+"\nENDMARKER", table));
    const char embedded[]{'V','E','R','S','I','O','N',' ','1','\0','\n'};
    CHECK(!strings::Read("menu", "english", std::string_view(embedded, sizeof(embedded)), table));
    CHECK(table == expected);
    return 0;
}
bool InputWord(unsigned &word) {
    unsigned char raw[4];
    const auto count = std::fread(raw, 1, 4, stdin);
    if (!count) return false;
    if (count != 4) std::exit(1);
    word = 0;
    for (unsigned n = 0; n < 4; ++n) word |= unsigned(raw[n]) << (n*8);
    return true;
}
int main(int argc, char **argv) {
    if (const auto result = Synthetic()) return result;
    if (argc == 2 && !std::strcmp(argv[1], "--retail-stdin")) {
#if defined(_WIN32)
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        unsigned pathLength = 0, count = 0, total = 0;
        while (InputWord(pathLength)) {
            CHECK(pathLength && pathLength <= 255);
            std::string path(pathLength, '\0'); CHECK(std::fread(path.data(), 1, pathLength, stdin) == pathLength);
            unsigned length = 0; CHECK(InputWord(length) && length <= strings::MaximumFile);
            std::string data(length, '\0'); CHECK(std::fread(data.data(), 1, length, stdin) == length);
            const auto languageEnd = path.find('/', 8);
            CHECK(path.rfind("strings/", 0) == 0 && languageEnd != std::string::npos);
            const auto package = path.substr(languageEnd+1, path.size()-languageEnd-5);
            const auto language = path.substr(8, languageEnd-8);
            strings::Table table;
            if (!strings::Read(package, language, data, table)) {
                std::fprintf(stderr, "retail parser failed: %s\n", path.c_str()); return 1;
            }
            ++count; total += static_cast<unsigned>(table.size());
        }
        CHECK(count == 215);
        std::printf("JKA StringEd: %u installed records and %u lookup entries parsed\n", count, total);
    }
    return 0;
}
