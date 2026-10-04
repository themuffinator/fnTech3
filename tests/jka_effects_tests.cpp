/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "code/compat/jka_effects.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace fntech3::jka::effects;
static const char *fixture =
    "// Own format fixture\nrepeatDelay 600\n"
    "PaRtIcLe\n{\nname \"Own particle\"\nlife 100 200\n"
    "origin 1 2 3 4 5 6\nshaders\n[\ngfx/own/a\n\"gfx/own/b\"\n]\n"
    "size\n{\nstart 2\nend 4 6\nflags linear\n}\n}\n"
    "sound\n{\nsound sound/own/test.mp3\n}\n"
    "fxrunner\n{\nplayfx own/child\n}\n"
    "forcefeedback\n{\nname ignored\n}\n";
static void Reject(std::string_view text) {
    Definition output; output.repeatDelay=177;
    Failure failure;
    const bool parsed=Parse(text,output,failure);
    if(parsed)std::cerr << "unexpected fixture acceptance: " << text << '\n';
    assert(!parsed);
    assert(output.repeatDelay==177 && output.primitives.empty() && failure.reason && failure.line);
}
int main(int argc,char **argv) {
    if (argc==2 && std::string_view(argv[1])=="--corpus") {
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);
#endif
        const std::string input{std::istreambuf_iterator<char>(std::cin),{}};
        Definition output; Failure failure;
        if (!Parse(input,output,failure)) {
            std::cerr << failure.line << ": " << failure.reason << '\n';return 1;
        }
        std::size_t media=0,numbers=0;
        for (const auto &primitive:output.primitives) {media+=primitive.media.size();numbers+=primitive.numbers.size();}
        std::cout << "{\"primitives\":" << output.primitives.size() << ",\"media\":" << media
            << ",\"numbers\":" << numbers << ",\"ignoredGroups\":" << output.ignoredGroups.size() << "}\n";
        return 0;
    }
    Name name;
    assert(ResolveName("own/test",name)&&name.key=="own/test"&&name.file=="effects/own/test.efx");
    assert(ResolveName("effects/Own/Test.efx",name)&&name.key=="effects/own/test"&&name.file=="effects/Own/Test.efx");
    assert(ResolveName("own/test.extra.efx",name)&&name.key=="own/test"&&name.file=="effects/own/test.extra.efx");
    for (const auto *path:{"", "/absolute", "../outside", "a/../b", "a/./b", "a//b", "c:/b", "a\\b", "a b", "a/"})
        assert(!ResolveName(path,name));
    assert(!ResolveName(std::string(NameBytes,'x'),name));
    Definition definition; Failure failure;
    assert(Parse(fixture,definition,failure)&&definition.repeatDelay==600&&definition.primitives.size()==3);
    assert(definition.ignoredGroups.size()==1&&definition.ignoredGroups[0]=="forcefeedback");
    assert(definition.primitives[0].kind==Kind::Particle&&definition.primitives[0].media.size()==2);
    assert(definition.primitives[0].numbers.size()==4&&definition.primitives[0].numbers[1].values.size()==6);
    assert(definition.primitives[0].declaration.pairs[0].values[0]=="Own particle");
    assert(definition.primitives[1].media[0].kind==MediaKind::Sound);
    assert(definition.primitives[2].media[0].kind==MediaKind::Play);
    assert(Parse("ignored 1\nrepeatDelay 900\nparticle\n{\n}\n",definition,failure)&&definition.repeatDelay==300);
    assert(Parse("/* comment\n */\nparticle {\nlife 50\n}\n",definition,failure));
    assert(Parse("particle\n{\nunknown\n[\n]\n}\n",definition,failure));
    for (const auto *text:{"", "/*", "/* comment", "particle", "particle\n{\n", "}\n", "particle\n{\nlife\n}\n",
        "particle\n{\nshaders\n[\ngfx/own\n", "particle\n{\nname \"unfinished\n}\n",
        "particle\n{\nname \"done\" trailing\n}\n", "particle\n{\nlife nan\n}\n",
        "particle\n{\nlife 1e400\n}\n", "particle\n{\norigin 1 2\n}\n", "particle\n{\nlife 1 2 3\n}\n",
        "particle\n{\nshaders ../outside\n}\n", "particle\n{\nshaders \"a//b\"\n}\n",
        "fxrunner\n{\nplayfx /absolute\n}\n", "repeatDelay 1.5\n", "repeatDelay 2147483648\n"}) Reject(text);
    Reject(std::string(MaxTextBytes+1,'x'));
    Reject("particle\n{\nname "+std::string(MaxTokenBytes+1,'x')+"\n}\n");
    auto binary=std::string("particle\n{\nname a");binary.push_back('\0');binary+="b\n}\n";Reject(binary);
    std::string nested;
    for(std::size_t n=0;n<MaxDepth+1;++n)nested+="own\n{\n";
    for(std::size_t n=0;n<MaxDepth+1;++n)nested+="}\n";
    Reject(nested);
    std::string many;
    for(std::size_t n=0;n<MaxPrimitives+1;++n)many+="particle\n{\n}\n";
    Reject(many);
    std::string list="particle\n{\nshaders\n[\n";
    for(std::size_t n=0;n<MaxListValues+1;++n)list+="gfx/own\n";
    Reject(list+"]\n}\n");
    many.clear();for(std::size_t n=0;n<MaxNodes+1;++n)many+="own 1\n";
    Reject(many);
}
