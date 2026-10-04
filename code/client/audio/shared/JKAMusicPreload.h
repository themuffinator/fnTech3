/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_AUDIO_JKA_MUSIC_PRELOAD_H
#define FNTECH3_AUDIO_JKA_MUSIC_PRELOAD_H
#include <array>
#include <cstddef>
#include <cstring>

namespace fntech3::audio::jka::music {
inline constexpr std::size_t NameBytes=64;
using Name=std::array<char,NameBytes>;
struct Names {Name intro{},loop{};};
enum class Error {None,NameTooLong,UnsafePath,DynamicSet};

inline unsigned char Fold(unsigned char value) {
    if(value=='\\')return '/';
    return value>='A'&&value<='Z'?static_cast<unsigned char>(value+'a'-'A'):value;
}
inline bool Same(const Name &name,const char *other) {
    if(!other)return !name[0];
    for(std::size_t i=0;i<NameBytes;++i){
        if(Fold(static_cast<unsigned char>(name[i]))!=Fold(static_cast<unsigned char>(other[i])))return false;
        if(!name[i])return true;
    }
    return false;
}

inline Error FileName(const char *text,Name &out) {
    Name staged{};
    if(!text){out=staged;return Error::None;}
    std::size_t count=0;
    while(count<NameBytes&&text[count]){staged[count]=text[count];++count;}
    if(count==NameBytes)return Error::NameTooLong;
    if(!count){out=staged;return Error::None;}
    if(staged[0]=='/'||staged[0]=='\\')return Error::UnsafePath;
    std::size_t part=0,lastPart=0;bool path=false;
    for(std::size_t i=0;i<=count;++i){
        const auto c=static_cast<unsigned char>(staged[i]);
        if(c==':'||(c&&c<32)||c==127)return Error::UnsafePath;
        if(!c||c=='/'||c=='\\'){
            const auto length=i-part;
            if(!length||(length==1&&staged[part]=='.')||
                (length==2&&staged[part]=='.'&&staged[part+1]=='.'))return Error::UnsafePath;
            if(c){path=true;part=i+1;lastPart=part;}
        }
    }
    // Bare names select Raven's dynamic-music sets. Their file/state adapter
    // is distinct from this explicit-track preload and remains a named gate.
    if(!path)return Error::DynamicSet;
    bool extension=false;
    for(std::size_t i=lastPart;i<count;++i)if(staged[i]=='.')extension=true;
    if(!extension){
        if(count+4>=NameBytes)return Error::NameTooLong;
        std::memcpy(staged.data()+count,".mp3",5);
    }
    out=staged;return Error::None;
}

inline Error PrepareNames(const char *intro,const char *loop,Names &out) {
    Names staged;
    auto error=FileName(intro,staged.intro);
    if(error!=Error::None)return error;
    if(!staged.intro[0]){out=staged;return Error::None;}
    error=FileName(loop&&*loop?loop:intro,staged.loop);
    if(error!=Error::None)return error;
    out=staged;return Error::None;
}

// Holds actual decoder owners, never invented handles. Open initializes the
// decoder at its first sample; Take transfers that owner to a normal Fn music
// player without opening it again. The bounded cache never queues playback.
template<class Stream> class Cache {
    struct Entry {Name name{};Stream *stream=nullptr;};
    std::array<Entry,2> entries_{};
public:
    template<class IO> void Clear(IO &io) {
        for(auto &entry:entries_){
            auto *stream=entry.stream;entry.stream=nullptr;entry.name={};
            if(stream)io.Close(stream); // Release ownership before error/reentry.
        }
    }
    template<class IO> void Prepare(IO &io,const Names &names) {
        Clear(io);
        if(!names.intro[0])return;
        entries_[0].name=names.intro;
        entries_[0].stream=io.Open(entries_[0].name.data());
        if(!Same(names.intro,names.loop.data())){
            entries_[1].name=names.loop;
            entries_[1].stream=io.Open(entries_[1].name.data());
        }
    }
    Stream *Take(const char *name) {
        for(auto &entry:entries_)if(entry.stream&&Same(entry.name,name)){
            auto *stream=entry.stream;entry.stream=nullptr;entry.name={};return stream;
        }
        return nullptr;
    }
    int Count() const {int count=0;for(const auto &entry:entries_)if(entry.stream)++count;return count;}
};
} // namespace fntech3::audio::jka::music
#endif
