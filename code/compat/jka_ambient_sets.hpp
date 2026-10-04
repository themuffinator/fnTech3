/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_AMBIENT_SETS_HPP
#define FNTECH3_JKA_AMBIENT_SETS_HPP
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

/* Engine-owned definitions, never Raven's private STL objects. MP retail
 * imports49/50 build a selected-name table and parse sound/sound.txt even in
 * silent probes. Its three category passes determine set IDs. Wave paths are
 * handed to Fn's sound registration; playback is a separate contract. */
namespace fntech3::jka::ambient {
enum class Kind { General, Local, BrushModel };
struct Definition {
    Kind kind{};
    std::string name, loop;
    std::vector<std::string> waves;
    // Retail AddSet 0x467270: seconds 10..25, byte volume 255, radius 250.
    std::uint32_t minimumSeconds{10}, maximumSeconds{25};
    int minimumVolume{255}, maximumVolume{255}, radius{250};
};
struct Failure { std::size_t line{}; const char *reason{}; };
inline constexpr std::size_t MaxTextBytes=1024u*1024u, MaxDefinitions=4096, MaxNames=512;
inline constexpr std::size_t NameBytes=64, MaxWaves=8;

inline bool EqualKeyword(std::string_view a, std::string_view b) {
    if(a.size()!=b.size())return false;
    for(std::size_t n=0;n<a.size();++n){
        const auto fold=[](unsigned char c){return c>='A'&&c<='Z' ? static_cast<unsigned char>(c+32) : c;};
        if(fold(static_cast<unsigned char>(a[n]))!=fold(static_cast<unsigned char>(b[n])))return false;
    }
    return true;
}
inline bool ValidName(std::string_view name) {
    if(name.empty()||name.size()>=NameBytes)return false;
    for(unsigned char c:name)if(c<=32||c==127||c=='"'||c==';')return false;
    return true;
}
inline bool ValidWave(std::string_view path) {
    if(path.empty()||path.size()>=NameBytes||path.front()=='/'||path.front()=='\\')return false;
    std::size_t start=0;
    for(std::size_t n=0;n<=path.size();++n){
        if(n<path.size()&&(static_cast<unsigned char>(path[n])<33||path[n]==127||path[n]==':'||path[n]=='"'))return false;
        if(n==path.size()||path[n]=='/'||path[n]=='\\'){
            const auto part=path.substr(start,n-start);
            if(part.empty()||part=="."||part=="..")return false;
            start=n+1;
        }
    }
    return true;
}
inline bool Integer(std::string_view text, int &value) {
    if(text.empty())return false;
    if(text.front()=='+'){
        text.remove_prefix(1);
        if(text.empty()||text.front()=='-'||text.front()=='+')return false;
    }
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size();
}

class Catalog {
public:
    bool Select(std::string_view name) {
        if(EqualKeyword(name,"#clear")){selected_.clear();return true;}
        if(!ValidName(name))return false;
        if(selected_.find(std::string(name))!=selected_.end())return true;
        if(selected_.size()>=MaxNames)return false;
        selected_.emplace(name);return true;
    }
    void Clear() { selected_.clear();definitions_.clear();lookup_.clear(); }
    std::size_t SelectedCount()const{return selected_.size();}
    const std::vector<Definition> &Definitions()const{return definitions_;}
    const Definition *Find(std::string_view name)const{
        const auto found=lookup_.find(std::string(name));
        return found==lookup_.end()?nullptr:&definitions_[found->second];
    }
    bool Parse(std::string_view text, Failure &failure) {
        failure={};
        const auto reject=[&](std::size_t line,const char *reason){failure={line,reason};return false;};
        if(text.size()>MaxTextBytes)return reject(0,"sound-set text exceeds the engine bound");
        if(text.find('\0')!=std::string_view::npos)return reject(0,"embedded NUL in sound-set text");
        std::vector<Definition> pending;
        Definition *current=nullptr;
        bool typed=false, declared=false;
        std::size_t line=0,offset=0,declarations=0;
        while(offset<text.size()){
            ++line;
            const auto end=text.find_first_of("\r\n",offset);
            auto content=text.substr(offset,(end==std::string_view::npos?text.size():end)-offset);
            offset=end==std::string_view::npos?text.size():end+1;
            if(end!=std::string_view::npos&&text[end]=='\r'&&offset<text.size()&&text[offset]=='\n')++offset;
            content=content.substr(0,content.find(';'));
            std::vector<std::string_view> words;
            for(std::size_t at=0;at<content.size();){
                while(at<content.size()&&(content[at]==' '||content[at]=='\t'))++at;
                const auto begin=at;
                while(at<content.size()&&content[at]!=' '&&content[at]!='\t')++at;
                if(at>begin)words.push_back(content.substr(begin,at-begin));
                if(words.size()>MaxWaves+2)return reject(line,"too many sound-set line tokens");
            }
            if(words.empty())continue;
            const auto key=words[0];
            const bool general=key=="generalSet",local=key=="localSet",brush=key=="bmodelSet";
            if(general||local||brush){
                if(!typed)return reject(line,"set declaration precedes ambientset type");
                if(words.size()!=2||!ValidName(words[1]))return reject(line,"invalid sound-set declaration");
                if(++declarations>MaxDefinitions)return reject(line,"too many sound-set definitions");
                declared=true;current=nullptr;
                if(selected_.find(std::string(words[1]))==selected_.end())continue;
                pending.emplace_back();current=&pending.back();
                current->kind=general?Kind::General:local?Kind::Local:Kind::BrushModel;
                current->name=words[1];
                continue;
            }
            if(EqualKeyword(key,"type")){
                if(declared||typed||words.size()!=2||!EqualKeyword(words[1],"ambientset"))
                    return reject(line,"invalid ambientset type");
                typed=true;continue;
            }
            if(EqualKeyword(key,"amsdir")||EqualKeyword(key,"outdir")||EqualKeyword(key,"basedir")){
                if(declared||words.size()!=2)return reject(line,"invalid inert authoring header");
                continue; // These authoring paths are never passed to a filesystem.
            }
            if(!declared)return reject(line,"sound-set field without a declaration");
            if(!current)continue; // Retail skips fields of names not selected for this map.
            int field=-1;
            if(EqualKeyword(key,"timeBetweenWaves"))field=0;
            else if(EqualKeyword(key,"volRange"))field=1;
            else if(EqualKeyword(key,"radius"))field=2;
            else if(EqualKeyword(key,"loopedWave"))field=3;
            else if(EqualKeyword(key,"subWaves"))field=4;
            else return reject(line,"unrecognized sound-set field");
            if((current->kind==Kind::BrushModel&&field!=4)||
               (current->kind==Kind::General&&field==2))
                return reject(line,"unsupported field for this sound-set category");
            if(field<=2){
                const std::size_t arity=field==2?2:3;
                int first=0,second=0;
                if(words.size()!=arity||!Integer(words[1],first)||
                    (arity==3&&!Integer(words[2],second))||first<0||second<0)
                    return reject(line,"invalid sound-set integer range");
                if(arity==3&&first>second)std::swap(first,second);
                if(field==0){
                    if(second>2147483)return reject(line,"sound-set interval exceeds millisecond range");
                    current->minimumSeconds=static_cast<std::uint32_t>(first);
                    current->maximumSeconds=static_cast<std::uint32_t>(second);
                }else if(field==1){
                    if(second>255)return reject(line,"sound-set volume exceeds byte range");
                    current->minimumVolume=first;current->maximumVolume=second;
                }else current->radius=first;
            }else if(field==3){
                if(words.size()!=2)return reject(line,"invalid loopedWave arity");
                current->loop="sound/"+std::string(words[1])+".wav";
                if(!ValidWave(current->loop))return reject(line,"invalid contained loopedWave path");
            }else{
                if(words.size()<3||words.size()>MaxWaves+2)return reject(line,"invalid subWaves arity");
                if(current->waves.size()+words.size()-2>MaxWaves)
                    return reject(line,"too many sound-set subwaves");
                for(std::size_t n=2;n<words.size();++n){
                    auto wave="sound/"+std::string(words[1])+"/"+std::string(words[n])+".wav";
                    if(!ValidWave(wave))return reject(line,"invalid contained subWaves path");
                    current->waves.push_back(std::move(wave));
                }
            }
        }
        if(!typed)return reject(0,"missing ambientset type");
        std::vector<Definition> chosen;
        std::map<std::string,std::size_t> lookup;
        for(Kind kind:{Kind::General,Kind::Local,Kind::BrushModel})for(auto &definition:pending){
            if(definition.kind!=kind||selected_.find(definition.name)==selected_.end())continue;
            lookup[definition.name]=chosen.size();chosen.push_back(std::move(definition));
        }
        for(const auto &name:selected_)if(lookup.find(name)==lookup.end())
            return reject(0,"selected sound set is missing from definitions");
        definitions_=std::move(chosen);lookup_=std::move(lookup);return true;
    }
private:
    std::set<std::string> selected_;
    std::vector<Definition> definitions_;
    std::map<std::string,std::size_t> lookup_;
};
}
#endif
