/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_JKA_CGAME_SKINS_HPP
#define FNTECH3_CLIENT_JKA_CGAME_SKINS_HPP
#include "../compat/jka_skin.hpp"
#include <map>

/* Actual Fn renderer handles index owned visibility declarations. Model
 * geometry is still refused by the cgame adapter; subsequent rendering must
 * consume this same per-model binding through the Fn skeletal paths. */
struct JKACgameSkinBinding {
    int renderSkin{};
    std::map<int,std::uint32_t> overrides;
};
struct JKACgameSkinState {
    std::map<int,fntech3::jka::skin::Definition> definitions;
    std::map<std::string,int> names;
    std::map<std::pair<std::uint32_t,int>,JKACgameSkinBinding> bindings;
    fileHandle_t file{};
    std::vector<char> bytes;
    fntech3::jka::skin::Definition pending;
    std::string pendingName,pendingKey;
};
std::unique_ptr<JKACgameSkinState> jkaCgameSkins;

static void CL_JKACgameSkinClose() {
    if(!jkaCgameSkins||!jkaCgameSkins->file)return;
    const auto file=jkaCgameSkins->file;jkaCgameSkins->file=0;FS_FCloseFile(file);
}
static void CL_JKACgameSkinClear() {
    CL_JKACgameSkinClose();jkaCgameSkins.reset();
}
static JKACgameSkinState &CL_JKACgameSkinState() {
    if(!jkaCgameSkins)jkaCgameSkins=std::make_unique<JKACgameSkinState>();
    return *jkaCgameSkins;
}
static intptr_t CL_JKACgameRegisterSkin(intptr_t address) {
    namespace skin=fntech3::jka::skin;
    const auto text=CL_JKACgameGhoul2Text(address);
    if(!text[0])return 0;
    if(std::strchr(text.data(),'|'))
        Com_Error(ERR_DROP,"Jedi Academy cgame composite skin names are not served yet");
    auto &state=CL_JKACgameSkinState();
    state.pendingName=text.data();state.pendingKey=skin::Lower(state.pendingName);
    const auto &name=state.pendingName;
    const auto &key=state.pendingKey;
    if(!skin::Path(name))Com_Error(ERR_DROP,"Invalid Jedi Academy cgame skin path");
    const auto found=state.names.find(key);
    if(found!=state.names.end()) {
        const int actual=re.RegisterSkin(text.data());
        if(actual!=found->second)
            Com_Error(ERR_DROP,"Jedi Academy cgame renderer changed a registered skin identity");
        return actual;
    }
    if(state.names.size()>=1024)
        Com_Error(ERR_DROP,"Jedi Academy cgame skin registry exceeds its budget");
    state.pending={};state.bytes.clear();
    // This test exactly follows the current Fn skin renderer's file choice.
    const bool file=name.size()>=5&&name.compare(name.size()-5,5,".skin")==0;
    if(file) {
        const int length=FS_FOpenFileRead(text.data(),&state.file,qfalse);
        if(length<0||!state.file) {
            CL_JKACgameSkinClose();
        } else {
            if(static_cast<std::size_t>(length)>skin::MaxBytes)
                Com_Error(ERR_DROP,"Jedi Academy cgame skin file exceeds its bounded reader");
            state.bytes.resize(static_cast<std::size_t>(length));
            const int read=length?FS_Read(state.bytes.data(),length,state.file):0;
            CL_JKACgameSkinClose();
            if(read!=length)Com_Error(ERR_DROP,"Short Jedi Academy cgame skin file read");
            const char empty=0;
            const auto error=skin::Parse(length?state.bytes.data():&empty,state.bytes.size(),state.pending);
            if(error!=skin::Error::None)
                Com_Error(ERR_DROP,"Jedi Academy cgame %s",skin::ErrorName(error));
        }
    } else state.pending.surfaces.push_back({"",name,skin::Lower(name)=="*off"});
    const int actual=re.RegisterSkin(text.data());
    if(actual<0)Com_Error(ERR_DROP,"Jedi Academy cgame renderer returned a negative skin handle");
    if(actual) {
        // Publish only what the actual renderer registered. A missing file
        // unexpectedly given a positive handle is not a fabricated definition.
        if(file&&state.pending.surfaces.empty())
            Com_Error(ERR_DROP,"Jedi Academy cgame renderer skin has no owned surface declarations");
        const auto existing=state.definitions.find(actual);
        if(existing!=state.definitions.end())
            Com_Error(ERR_DROP,"Jedi Academy cgame renderer reused a live skin identity");
        state.definitions.emplace(actual,std::move(state.pending));state.names.emplace(key,actual);
    }
    state.pending={};state.bytes.clear();state.pendingName.clear();state.pendingKey.clear();return actual;
}
static bool CL_JKACgameSkinBinding(const fntech3::ghoul2::Model &model,
    const fntech3::jka::skin::Definition &definition,JKACgameSkinBinding &binding)
{
    if(!model.meshBytes||model.mesh.numSurfaces<0||
        static_cast<std::size_t>(model.mesh.numSurfaces)!=model.surfaceNames.size())return false;
    std::vector<fntech3::jka::skin::ModelSurface> surfaces;
    const auto &bytes=*model.meshBytes;
    const fntech3::jka::model::Reader reader(bytes.data(),bytes.size());
    for(std::size_t n=0;n<model.surfaceNames.size();++n) {
        const auto offset=fntech3::jka::model::MeshHeaderBytes+n*4;
        if(!reader.Fits(offset,4))return false;
        const int stated=reader.Int(offset);
        if(stated<0||!reader.Fits(fntech3::jka::model::MeshHeaderBytes+static_cast<std::size_t>(stated),fntech3::jka::model::SurfHierarchyBytes))
            return false;
        const auto flags=reader.Unsigned(fntech3::jka::model::MeshHeaderBytes+static_cast<std::size_t>(stated)+64);
        surfaces.push_back({model.surfaceNames[n],flags});
    }
    binding.overrides=fntech3::jka::skin::Visibility(definition,surfaces);return true;
}
static bool CL_JKACgameSetSkin(std::uint32_t identity,int modelIndex,int customSkin,int renderSkin) {
    const auto *instance=CL_JKACgameGhoul2Resolve(identity);
    const auto *model=instance?instance->At(modelIndex):nullptr;
    if(!model)return false;
    if(customSkin<0||renderSkin<0)
        Com_Error(ERR_DROP,"Jedi Academy cgame Ghoul2 received a negative skin handle");
    if(!renderSkin)return CL_JKACgameGhoul2Store().SetSkin(
        reinterpret_cast<void *>(std::uintptr_t(identity)),modelIndex,customSkin);
    const auto *definition=jkaCgameSkins?&jkaCgameSkins->definitions:nullptr;
    if(!definition||definition->find(renderSkin)==definition->end())
        Com_Error(ERR_DROP,"Jedi Academy cgame Ghoul2 render skin %i has no owned Fn declaration",renderSkin);
    JKACgameSkinBinding binding;
    binding.renderSkin=renderSkin;
    if(!CL_JKACgameSkinBinding(*model,definition->at(renderSkin),binding))
        Com_Error(ERR_DROP,"Jedi Academy cgame Ghoul2 skin requires a complete bounded owned surface table");
    const auto key=std::make_pair(identity,modelIndex);
    jkaCgameSkins->bindings[key]=std::move(binding);
    return CL_JKACgameGhoul2Store().SetSkin(reinterpret_cast<void *>(std::uintptr_t(identity)),modelIndex,customSkin);
}
static void CL_JKACgameSkinCopyModel(std::uint32_t source, int sourceIndex,
    std::uint32_t destination, int destinationIndex) {
    if (!jkaCgameSkins) return;
    const auto sourceKey = std::make_pair(source, sourceIndex);
    const auto destinationKey = std::make_pair(destination, destinationIndex);
    if (sourceKey == destinationKey) return;
    auto &bindings = jkaCgameSkins->bindings;
    const auto found = bindings.find(sourceKey);
    if (found == bindings.end()) bindings.erase(destinationKey);
    else bindings[destinationKey] = found->second;
}
static void CL_JKACgameSkinDuplicateModel(std::uint32_t source, std::uint32_t destination) {
    if (!jkaCgameSkins) return;
    auto &bindings = jkaCgameSkins->bindings;
    for (auto at = bindings.lower_bound({source, INT_MIN});
        at != bindings.end() && at->first.first == source; ++at)
        bindings.emplace(std::make_pair(destination, at->first.second), at->second);
}
static void CL_JKACgameSkinCleanModel(std::uint32_t identity) {
    if(!jkaCgameSkins)return;
    auto &bindings=jkaCgameSkins->bindings;
    for(auto at=bindings.lower_bound({identity,INT_MIN});at!=bindings.end()&&at->first.first==identity;)
        at=bindings.erase(at);
}
#endif
