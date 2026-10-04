/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/client/audio/shared/JKAMusicPreload.h"
#include <cassert>
#include <csetjmp>
#include <map>
#include <string>
#include <type_traits>

namespace music=fntech3::audio::jka::music;
struct Stream {int id;};
static std::jmp_buf recovery;
struct IO {
    std::map<Stream*,std::string> live;
    int opened=0,closed=0,failOpenAt=0;
    bool closeDrop=false,openDrop=false;
    music::Cache<Stream> *owner=nullptr;
    Stream *Open(const char *name){
        ++opened;
        if(openDrop&&opened==failOpenAt){owner->Clear(*this);std::longjmp(recovery,1);}
        if(opened==failOpenAt)return nullptr;
        auto *stream=new Stream{opened};live[stream]=name;return stream;
    }
    void Close(Stream *stream){
        assert(live.erase(stream)==1);++closed;delete stream;
        if(closeDrop){closeDrop=false;owner->Clear(*this);std::longjmp(recovery,1);}
    }
};
#ifdef _MSC_VER
#pragma warning(push)
// MSVC diagnoses setjmp in C++ under /EHsc. This test's recovery frame has
// only references; the jumped-over provider branch has no automatic owners.
#pragma warning(disable:4611)
#endif
static void RecoveryCases(music::Cache<Stream> &cache,IO &io,const music::Names &names){
    static_assert(std::is_trivially_destructible_v<music::Names>);
    static_assert(std::is_trivially_destructible_v<music::Cache<Stream>>);
    // Keep non-trivial automatic test objects outside the setjmp frame.
    // The cache and provider have static lifetime; names is unchanged here.
    io.failOpenAt=io.opened+2;io.openDrop=true;
    if(!setjmp(recovery)){cache.Prepare(io,names);assert(false);}
    io.openDrop=false;assert(!cache.Count()&&io.live.empty());cache.Clear(io);
    io.failOpenAt=0;cache.Prepare(io,names);io.closeDrop=true;
    if(!setjmp(recovery)){cache.Clear(io);assert(false);}
    assert(!cache.Count()&&io.live.empty()&&io.closed==io.opened-2);cache.Clear(io);
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
int main(){
    music::Names names;
    assert(music::PrepareNames("music/mp/action","",names)==music::Error::None);
    assert(!std::strcmp(names.intro.data(),"music/mp/action.mp3")&&names.intro==names.loop);
    const auto before=names;
    for(const char *bad:{"/music/x","../x.mp3","music/../x.mp3","music//x.mp3","music/./x.mp3","C:\\x.mp3","music/x\n.mp3"}){
        assert(music::PrepareNames(bad,nullptr,names)==music::Error::UnsafePath&&names.intro==before.intro);
    }
    assert(music::PrepareNames("ffa1",nullptr,names)==music::Error::DynamicSet);
    std::array<char,64> longName;longName.fill('a');assert(music::FileName(longName.data(),names.intro)==music::Error::NameTooLong);
    const std::string limit="music/"+std::string(54,'a');
    assert(music::PrepareNames(limit.c_str(),nullptr,names)==music::Error::NameTooLong);
    assert(music::PrepareNames(nullptr,"../unused",names)==music::Error::None&&!names.intro[0]&&!names.loop[0]);
    assert(music::PrepareNames("music/a.mp3","music/b.mp3",names)==music::Error::None);
    static music::Cache<Stream> cache;static IO io;io.owner=&cache;
    cache.Prepare(io,names);assert(cache.Count()==2&&io.live.size()==2&&io.opened==2);
    assert(!cache.Take("music/missing.mp3")&&cache.Count()==2);
    auto *intro=cache.Take("MUSIC\\A.MP3");assert(intro&&intro->id==1&&cache.Count()==1&&io.opened==2);
    assert(!cache.Take("music/a.mp3"));io.Close(intro);
    auto *loop=cache.Take("music/b.mp3");assert(loop&&loop->id==2&&!cache.Count());io.Close(loop);
    assert(io.live.empty()&&io.closed==2);cache.Clear(io);assert(io.closed==2);
    assert(music::PrepareNames("music/a.mp3","MUSIC/A.MP3",names)==music::Error::None);
    cache.Prepare(io,names);assert(cache.Count()==1&&io.opened==3);cache.Clear(io);
    // Failure after a first open and reentrant engine recovery close only the
    // cache's actual owners. No callback claims an unavailable decoder.
    assert(music::PrepareNames("music/a.mp3","music/b.mp3",names)==music::Error::None);
    io.failOpenAt=io.opened+2;cache.Prepare(io,names);assert(cache.Count()==1);cache.Clear(io);assert(io.live.empty());
    RecoveryCases(cache,io,names);
}
