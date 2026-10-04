#include "../code/compat/jka_ambient_sets.hpp"
#include <cassert>
#include <iostream>
#include <iterator>
#include <sstream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace fntech3::jka::ambient;

static void CheckCatalog() {
    Catalog c;Failure f;
    assert(c.Select("dupe")&&c.Select("general")&&c.Select("local"));
    assert(c.Select("dupe")&&c.SelectedCount()==3);
    const char *text=
        "; authoring paths never become runtime paths\r\n"
        "amsdir w:/do/not/open\r\nTYPE ambientSet\n"
        "outdir c:/do/not/open\nbasedir ../do/not/open\n"
        "bmodelSet dupe\nsubWaves doors open close\n"
        "localSet local\nvolRange 12 3\nradius 55\n"
        "timeBetweenWaves 9 2\nloopedWave world/hum\n"
        "generalSet general\n"
        "generalSet dupe\nsubWaves weather one two\n"
        "localSet dupe\nradius 99\n"
        "generalSet skipped\nunknownField is ignored when not selected\n";
    assert(c.Parse(text,f)&&!f.reason&&c.Definitions().size()==5);
    assert(c.Definitions()[0].name=="general"&&c.Definitions()[1].name=="dupe");
    assert(c.Definitions()[2].name=="local"&&c.Definitions()[3].name=="dupe");
    assert(c.Definitions()[4].name=="dupe");
    auto *d=c.Find("dupe");assert(d&&d->kind==Kind::BrushModel&&d->waves.size()==2);
    assert(d->waves[0]=="sound/doors/open.wav"&&d->waves[1]=="sound/doors/close.wav");
    d=c.Find("general");assert(d&&d->minimumSeconds==10&&d->maximumSeconds==25);
    assert(d->minimumVolume==255&&d->maximumVolume==255&&d->radius==250);
    d=c.Find("local");assert(d&&d->minimumSeconds==2&&d->maximumSeconds==9);
    assert(d->minimumVolume==3&&d->maximumVolume==12&&d->radius==55&&d->loop=="sound/world/hum.wav");
    assert(!c.Find("LOCAL")&&!c.Find("missing"));
    const auto old=c.Definitions().size();
    for(const char *bad:{
        "type other\n", "generalSet general\n", "type ambientset\n",
        "type ambientset\ngeneralSet general\nvolRange -1 2\n",
        "type ambientset\ngeneralSet general\nvolRange 0 256\n",
        "type ambientset\ngeneralSet general\ntimeBetweenWaves 0 2147484\n",
        "type ambientset\ngeneralSet general\nloopedWave ../../outside\n",
        "type ambientset\ngeneralSet general\nloopedWave c:/outside\n",
        "type ambientset\ngeneralSet general\nloopedWave /outside\n",
        "type ambientset\ngeneralSet general\nsubWaves dir one two three four five six seven eight nine\n",
        "type ambientset\ngeneralSet general\nsubWaves ../dir one\n",
        "type ambientset\ngeneralSet general\nradius 12\n",
        "type ambientset\nbmodelSet dupe\nloopedWave invalid\n",
        "type ambientset\ngeneralSet general\nnewField unsupported\n",
        "type ambientset\nlocalSet local\nradius 2147483648\n"}) {
        assert(!c.Parse(bad,f)&&f.reason&&c.Definitions().size()==old&&c.Find("dupe"));
    }
    auto nul=std::string(text);nul.push_back('\0');assert(!c.Parse(nul,f));
    assert(!c.Parse(std::string(MaxTextBytes+1,'x'),f));
    assert(!c.Select("")&&!c.Select("has space")&&!c.Select(std::string(NameBytes,'x')));
    // Clearing precache names preserves already parsed definitions until reparse.
    assert(c.Select("#CLEAR")&&c.SelectedCount()==0&&c.Find("dupe"));
    assert(c.Parse("type ambientset\n",f)&&c.Definitions().empty());
    for(std::size_t n=0;n<MaxNames;++n)assert(c.Select("set"+std::to_string(n)));
    assert(!c.Select("overflow")&&c.Select("set0"));c.Clear();
    assert(!c.SelectedCount()&&c.Definitions().empty());
    assert(c.Select("one"));
    assert(c.Parse("type ambientset\ngeneralSet one\nvolRange 3 4\nvolRange +8 +7\n"
        "subWaves misc a b\nsubWaves misc c\n",f));
    d=c.Find("one");assert(d&&d->minimumVolume==7&&d->maximumVolume==8&&d->waves.size()==3);
    std::string many="type ambientset\n";
    for(std::size_t n=0;n<=MaxDefinitions;++n)many+="generalSet unselected\n";
    assert(!c.Parse(many,f)&&c.Find("one"));
    assert(!c.Parse("type ambientset\ngeneralSet one\nsubWaves misc a b c d e f g h\nsubWaves misc i\n",f));
    int value=0;assert(Integer("+3",value)&&value==3);
    assert(!Integer("+-3",value)&&!Integer("3tail",value)&&!Integer("",value));
}

int main(int argc,char **) {
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    CheckCatalog();
    if(argc==1){std::cout<<"authored parser checks passed\n";return 0;}
    const std::string text{std::istreambuf_iterator<char>(std::cin),std::istreambuf_iterator<char>()};
    Catalog c;Failure f;
    std::istringstream lines(text);std::string line;std::size_t groups=0;
    while(std::getline(lines,line)){
        std::istringstream words(line);std::string kind,name;words>>kind>>name;
        if(kind=="generalSet"||kind=="localSet"||kind=="bmodelSet"){
            assert(c.Select(name));++groups;
        }
    }
    if(!c.Parse(text,f)){std::cerr<<"line "<<f.line<<": "<<f.reason<<'\n';return 2;}
    std::size_t general=0,local=0,brush=0,waves=0,loops=0;
    for(const auto &d:c.Definitions()){
        if(d.kind==Kind::General)++general;
        else if(d.kind==Kind::Local)++local;else ++brush;
        waves+=d.waves.size();loops+=!d.loop.empty();
    }
    assert(groups==234&&c.SelectedCount()==231&&c.Definitions().size()==234);
    assert(general==90&&local==86&&brush==58&&waves+loops==551);
    std::cout<<"{\"groups\":"<<groups<<",\"selected\":"<<c.SelectedCount()
        <<",\"parsed\":"<<c.Definitions().size()<<",\"general\":"<<general
        <<",\"local\":"<<local<<",\"brush\":"<<brush<<",\"waveReferences\":"<<(waves+loops)<<"}\n";
}
