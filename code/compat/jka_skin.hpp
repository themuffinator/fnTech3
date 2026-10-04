/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_SKIN_HPP
#define FNTECH3_JKA_SKIN_HPP
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/* Independently authored bounded skin declarations. The Fn renderer keeps
 * material registration; this reader preserves the same file's visibility
 * declarations for the owned Ghoul2 model. No renderer handle is invented.
 * Retail jamp 0x4ee7e0..0x4ee8bf applies the ordered declarations, resetting
 * old overrides and changing only OFF(2) and NODESCENDANTS(256). */
namespace fntech3::jka::skin {
inline constexpr std::size_t MaxBytes=1024*1024, MaxName=64;
// Fn's skin renderer holds 256 surfaces. Refuse excess rather than silently
// binding a visibility declaration its material renderer discarded.
inline constexpr std::size_t MaxSurfaces=256;
enum class Error { None, Size, Character, Token, Syntax, Surfaces };
inline const char *ErrorName(Error value) {
    switch(value) {
    case Error::None:return "ok";
    case Error::Size:return "skin file exceeds the bounded reader";
    case Error::Character:return "invalid skin character";
    case Error::Token:return "skin token exceeds MAX_QPATH";
    case Error::Syntax:return "malformed skin declaration";
    case Error::Surfaces:return "skin exceeds the Fn material surface budget";
    }
    return "unknown skin error";
}
inline std::string Lower(std::string value) {
    for(char &c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
inline bool Path(const std::string &value) {
    if(value.empty()||value.size()>=MaxName||value.front()=='/')return false;
    std::size_t part=0;
    for(std::size_t n=0;n<=value.size();++n) {
        if(n==value.size()||value[n]=='/') {
            const auto count=n-part;
            if(!count||(count==1&&value[part]=='.')||(count==2&&value[part]=='.'&&value[part+1]=='.'))return false;
            part=n+1;
        } else {
            const auto c=static_cast<unsigned char>(value[n]);
            if(c<=32||c>=127||c==':'||c=='\\'||c=='"'||c==','||c=='|'||c=='{'||c=='}')return false;
        }
    }
    return true;
}
struct Surface {std::string name,shader;bool off{};};
struct Definition {std::vector<Surface> surfaces;};
struct ModelSurface {std::string name;std::uint32_t flags{};};

class Reader {
    const char *data_;std::size_t size_,at_{};
    Error error_=Error::None;
    bool Skip() {
        for(;;) {
            while(at_<size_&&static_cast<unsigned char>(data_[at_])<=32)++at_;
            if(at_+1>=size_||data_[at_]!='/')return true;
            if(data_[at_+1]=='/') {
                at_+=2;while(at_<size_&&data_[at_]!='\n')++at_;
            } else if(data_[at_+1]=='*') {
                at_+=2;
                while(at_+1<size_&&(data_[at_]!='*'||data_[at_+1]!='/'))++at_;
                if(at_+1>=size_){error_=Error::Syntax;return false;}
                at_+=2;
            } else return true;
        }
    }
public:
    Reader(const char *data,std::size_t size):data_(data),size_(size){}
    bool End() {return Skip()&&at_==size_;}
    Error Status()const{return error_;}
    bool Comma() {
        if(!Skip()||at_==size_||data_[at_]!=','){error_=Error::Syntax;return false;}
        ++at_;return true;
    }
    bool Word(std::string &out) {
        out.clear();if(!Skip())return false;
        if(at_==size_||data_[at_]==','){error_=Error::Syntax;return false;}
        const bool quoted=data_[at_]=='"';if(quoted)++at_;
        bool closed=!quoted;
        while(at_<size_) {
            const char c=data_[at_];
            if(quoted&&c=='"'){++at_;closed=true;break;}
            if(!quoted&&(static_cast<unsigned char>(c)<=32||c==','))break;
            if(out.size()+1>=MaxName){error_=Error::Token;return false;}
            out.push_back(c);++at_;
        }
        if(!closed||out.empty()){error_=Error::Syntax;return false;}
        return true;
    }
};

inline Error Parse(const char *data,std::size_t bytes,Definition &output) {
    if(!data||bytes>MaxBytes)return Error::Size;
    for(std::size_t n=0;n<bytes;++n) {
        const auto c=static_cast<unsigned char>(data[n]);
        if(c==0||c>=127||(c<32&&c!='\n'&&c!='\r'&&c!='\t'))return Error::Character;
    }
    Reader reader(data,bytes);Definition staged;
    while(!reader.End()) {
        if(reader.Status()!=Error::None)return reader.Status();
        Surface surface;
        if(!reader.Word(surface.name)||!reader.Comma())return reader.Status();
        surface.name=Lower(std::move(surface.name));
        if(!Path(surface.name))return Error::Syntax;
        // Fn's skin registration ignores tag declarations after their comma;
        // the stock tag entries carry no shader token.
        if(surface.name.find("tag_")!=std::string::npos)continue;
        if(!reader.Word(surface.shader))return reader.Status();
        if(!Path(surface.shader))return Error::Syntax;
        surface.off=Lower(surface.shader)=="*off";
        if(staged.surfaces.size()>=MaxSurfaces)return Error::Surfaces;
        staged.surfaces.push_back(std::move(surface));
    }
    if(reader.Status()!=Error::None)return reader.Status();
    output=std::move(staged);return Error::None;
}

inline std::map<int,std::uint32_t> Visibility(const Definition &skin,
    const std::vector<ModelSurface> &surfaces)
{
    std::map<int,std::uint32_t> output;
    for(const auto &binding:skin.surfaces) {
        for(std::size_t n=0;n<surfaces.size();++n)if(Lower(surfaces[n].name)==binding.name) {
            const auto flags=surfaces[n].flags;
            // A material cannot turn a model's default OFF surface on.
            if(binding.off||!(flags&2u)) {
                const auto selected=(flags&~0x102u)|(binding.off?2u:0u);
                if(selected==flags)output.erase(static_cast<int>(n));
                else output[static_cast<int>(n)]=selected;
            }
            break;
        }
    }
    return output;
}
} // namespace fntech3::jka::skin
#endif
