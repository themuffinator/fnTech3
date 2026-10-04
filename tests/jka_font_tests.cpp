/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_font_format.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
namespace font = fntech3::jka::font;
void Word(std::vector<unsigned char> &bytes, unsigned at, unsigned value, unsigned width) {
    for (unsigned n = 0; n < width; ++n) bytes[at+n] = static_cast<unsigned char>(value >> (n*8));
}

int Synthetic() {
    std::vector<unsigned char> bytes(font::FileBytes);
    Word(bytes, 7168, 14, 2); // observed arialnb zero-height header
    Word(bytes, 'A'*font::GlyphBytes+4, 3, 2);
    font::Font raw, decoded;
    CHECK(fntech3::raven::font::Read(bytes.data(), bytes.size(), raw));
    CHECK(raw.height == 0 && raw.ascender == 0 && raw.descender == 0);
    CHECK(font::Read(bytes.data(), bytes.size(), decoded));
    CHECK(decoded.height == 14 && decoded.ascender == 11 && decoded.descender == 3);
    CHECK(font::Width(decoded, "A^2AA", .5f) == 5);
    CHECK(font::Width(decoded, "AA\nA", .5f) == 3);
    CHECK(font::Width(decoded, "AAA\r", 1) == 9);
    CHECK(font::Characters("A^2AA\r\n") == 3);
    const auto expected = decoded;
    Word(bytes, 7170, 0xffff, 2);
    CHECK(!font::Read(bytes.data(), bytes.size(), decoded));
    CHECK(!std::memcmp(&expected, &decoded, sizeof(decoded)));
    Word(bytes, 7170, 0, 2);
    for (std::size_t length = 0; length < bytes.size(); ++length) {
        CHECK(!font::Read(bytes.data(), length, decoded));
        CHECK(!std::memcmp(&expected, &decoded, sizeof(decoded)));
    }
    std::string path = "unchanged";
    CHECK(font::Path("fonts/arialnb", path) && path == "fonts/arialnb");
    CHECK(font::Path("FONTS/arialnb", path) && path == "fonts/arialnb");
    for (const char *bad : {"", "../arialnb", "fonts/../arialnb", "C:/arialnb", "a\\b", "a\n", "a\x7f"}) {
        CHECK(!font::Path(bad, path)); CHECK(path == "fonts/arialnb");
    }
    CHECK(!font::Path(nullptr, path));
    char unterminated[64]; std::memset(unterminated, 'a', sizeof(unterminated));
    CHECK(!font::Path(unterminated, path));
    return 0;
}

int main(int argc, char **argv) {
    if (const auto result = Synthetic()) return result;
    if (argc == 2 && !std::strcmp(argv[1], "--retail-stdin")) {
#if defined(_WIN32)
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        std::vector<unsigned char> bytes(font::FileBytes);
        unsigned count = 0;
        while (const auto size = std::fread(bytes.data(), 1, bytes.size(), stdin)) {
            font::Font decoded;
            CHECK(size == bytes.size() && font::Read(bytes.data(), size, decoded));
            std::printf("font %u: pointSize=%d height=%d ascender=%d descender=%d\n", count++,
                decoded.pointSize, decoded.height, decoded.ascender, decoded.descender);
        }
        CHECK(count == 7);
    }
    return 0;
}
