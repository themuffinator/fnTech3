/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_skin.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace fntech3::jka::skin;
static void Fixtures() {
    Definition parsed;
    const std::string text="// authored fixture\n tag_head,\n ROOT,*off\n"
        "root,material/root\n hidden,material/hidden\n descend,material/descend\n"
        "bolt,*off\n absent,material/absent\n /* end */";
    assert(Parse(text.data(),text.size(),parsed)==Error::None&&parsed.surfaces.size()==6);
    const std::vector<ModelSurface> model{{"Root",0},{"hidden",2},{"descend",256},{"bolt",1}};
    const auto selected=Visibility(parsed,model);
    assert(selected.size()==2&&selected.at(2)==0&&selected.at(3)==3);
    assert(!selected.count(0)&&!selected.count(1));
    const std::string off="root,*off\n";assert(Parse(off.data(),off.size(),parsed)==Error::None);
    assert(Visibility(parsed,model).at(0)==2);
    const auto before=parsed.surfaces[0].name;
    for(const std::string bad:{"root material/root","root,","root,\"unclosed","/* unterminated","root,../escape","../root,material/root"}) {
        assert(Parse(bad.data(),bad.size(),parsed)!=Error::None&&parsed.surfaces.size()==1&&parsed.surfaces[0].name==before);
    }
    const std::string quoted="\"ROOT\",\"*OFF\"\n";
    assert(Parse(quoted.data(),quoted.size(),parsed)==Error::None&&parsed.surfaces[0].off);
    const std::string longToken(64,'a');const auto longSkin=longToken+",material/root";
    assert(Parse(longSkin.data(),longSkin.size(),parsed)==Error::Token);
    std::string maximum;
    for(std::size_t n=0;n<MaxSurfaces;++n)maximum+="surface,material/root\n";
    assert(Parse(maximum.data(),maximum.size(),parsed)==Error::None&&parsed.surfaces.size()==MaxSurfaces);
    maximum+="extra,material/extra\n";assert(Parse(maximum.data(),maximum.size(),parsed)==Error::Surfaces&&parsed.surfaces.size()==MaxSurfaces);
    const std::string nullData("root,material/root\0tail",23);
    assert(Parse(nullData.data(),nullData.size(),parsed)==Error::Character);
    assert(Parse(nullptr,0,parsed)==Error::Size);
    const std::string huge(MaxBytes+1,' ');assert(Parse(huge.data(),huge.size(),parsed)==Error::Size);
    assert(Path("models/players/test/model.skin")&&Path("*off"));
    for(const auto *name:{"/absolute.skin","C:relative.skin","../escape.skin","models//skin","models/./skin","models\\skin","parts|head|torso"})assert(!Path(name));
}
int main(int argc,char **argv) {
    if(argc==2&&std::string(argv[1])=="--corpus") {
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);
#endif
        const std::string data((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
        Definition parsed;const auto error=Parse(data.data(),data.size(),parsed);
        if(error!=Error::None){std::cerr<<ErrorName(error)<<'\n';return 1;}
        std::size_t off=0;for(const auto &surface:parsed.surfaces)off+=surface.off?1:0;
        std::cout<<parsed.surfaces.size()<<' '<<off<<'\n';return 0;
    }
    assert(argc==1);Fixtures();return 0;
}
