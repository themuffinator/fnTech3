/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_sp_model_precache.hpp"
#include <cassert>
#include <map>
#include <stdexcept>

namespace sp=fntech3::jka::sp;
namespace model=fntech3::jka::model;
using Bytes=std::vector<unsigned char>;
static void Word(Bytes &bytes,std::size_t offset,std::uint32_t value) {
	assert(offset+4<=bytes.size());
	for (int i=0;i<4;++i) bytes[offset+std::size_t(i)]=static_cast<unsigned char>(value>>(i*8));
}
static void Name(Bytes &bytes,std::size_t offset,const char *text) {
	assert(std::strlen(text)<64 && offset+64<=bytes.size());
	std::memcpy(bytes.data()+offset,text,std::strlen(text)+1);
}
static Bytes Skeleton() {
	/* One bone, one frame and one independent compressed pose. */
	Bytes file(294,0);Word(file,0,model::SkeletonIdent);Word(file,4,6);
	Name(file,8,"skeleton");Word(file,72,0x3f800000);Word(file,76,1);
	Word(file,80,276);Word(file,84,1);Word(file,88,280);Word(file,92,104);Word(file,96,294);
	Word(file,100,4);Name(file,104,"root");Word(file,172,0xffffffffu);
	return file;
}
static Bytes Mesh(const char *animation="models/test/animation") {
	/* One surface and vertex, one referenced bone; no triangles. */
	Bytes file(404,0);Word(file,0,model::MeshIdent);Word(file,4,6);
	Name(file,8,"mesh");Name(file,72,animation);Word(file,140,1);Word(file,144,1);
	Word(file,148,312);Word(file,152,1);Word(file,156,168);Word(file,160,404);
	Word(file,164,4);Name(file,168,"surface");Name(file,236,"shader");Word(file,304,0xffffffffu);
	Word(file,312,92);Word(file,316,4);Word(file,328,std::uint32_t(-320));Word(file,332,1);
	Word(file,336,44);Word(file,344,44);Word(file,348,1);Word(file,352,40);Word(file,356,84);
	return file;
}
class Files final : public fntech3::ghoul2::Files {
public:
	std::map<std::string,Bytes> files;
	int reads=0;
	bool changed=false,throwRead=false,oversize=false;
	std::size_t Read(const char *name,void *destination,std::size_t capacity) override {
		++reads;
		if (throwRead) throw std::runtime_error("filesystem drop");
		if (oversize) return fntech3::ghoul2::MaxModelBytes+1;
		const auto found=files.find(name);if(found==files.end()) return 0;
		const auto size=found->second.size();
		if (destination && changed) return size-1;
		if (destination && capacity>=size) std::memcpy(destination,found->second.data(),size);
		return size;
	}
};
int main() {
	Files files;files.files["models/test/animation.gla"]=Skeleton();files.files["models/test/model.glm"]=Mesh();
	sp::ModelPrecache registry(files);
	sp::ModelPrecache small(files,293);
	assert(small.Register("models/test/animation.gla").error==sp::PrecacheError::Budget);
	small.Clear();files.reads=0;
	assert(!registry.Register(nullptr).handle && !registry.Register("").handle);
	assert(!registry.Register(std::string(64,'a').c_str()).handle && files.reads==0);
	for (const char *path:{"../test.gla","/absolute.gla","C:/test.gla","\\absolute.gla"})
		assert(registry.Register(path).error==sp::PrecacheError::Name);
	assert(registry.Count()==0 && registry.Register("*unserved.gla").error==sp::PrecacheError::BuiltinSkeleton);
	assert(registry.Register("models/test.md3").error==sp::PrecacheError::Unsupported);
	assert(registry.Register("#model").error==sp::PrecacheError::Unsupported && files.reads==0);
	assert(registry.Register("MODELS/TEST/ANIMATION.GLA").handle==1);
	assert(registry.Register("models/test/animation.gla").handle==1 && files.reads==2);
	assert(registry.Resolve(1)->skeleton.numBones==1 && registry.Resolve(1)->bytes.size()==294);
	assert(registry.Register("models/test/model.glm").handle==2 && files.reads==4);
	assert(registry.Resolve(2)->animationHandle==1 && registry.Resolve(2)->mesh.numBones==1);
	assert(!std::strcmp(registry.AnimationFileName(2),"models/test/animation"));
	assert(!std::strcmp(registry.AnimationInternalName(1),"skeleton"));
	for (int handle:{-1,0,1024}) {
		assert(!std::strcmp(registry.AnimationFileName(handle),"") && !std::strcmp(registry.AnimationInternalName(handle),""));
	}
	assert(!std::strcmp(registry.AnimationFileName(1),"") && !std::strcmp(registry.AnimationInternalName(2),""));
	char *stable=registry.AnimationFileName(2);
	assert(!registry.Resolve(0) && !registry.Resolve(-1) && !registry.Resolve(1024));
	const int before=files.reads;
	assert(registry.Register("missing.gla").error==sp::PrecacheError::Missing);
	assert(!std::strcmp(registry.AnimationFileName(3),"") && !std::strcmp(registry.AnimationInternalName(3),""));
	assert(stable==registry.AnimationFileName(2) && !std::strcmp(stable,"models/test/animation"));
	stable[0]='M';assert(registry.Resolve(2)->bytes[72]=='m');stable[0]='m';
	assert(registry.Register("MISSING.GLA").error==sp::PrecacheError::Missing && files.reads==before+1);
	files.files["bad.gla"]=Skeleton();Word(files.files["bad.gla"],4,7);
	assert(registry.Register("bad.gla").error==sp::PrecacheError::Malformed);
	files.files["truncated.gla"]=Skeleton();files.files["truncated.gla"].resize(293);
	assert(registry.Register("truncated.gla").error==sp::PrecacheError::Malformed);
	files.files["mismatch.glm"]=Mesh();Word(files.files["mismatch.glm"],140,2);
	assert(registry.Register("mismatch.glm").error==sp::PrecacheError::Malformed);
	files.files["builtin.glm"]=Mesh("*default");
	const auto builtin=registry.Register("builtin.glm");assert(builtin.handle>0);
	const int builtinHandle=registry.Resolve(builtin.handle)->animationHandle;
	const auto *generated=registry.Resolve(builtinHandle);assert(generated && generated->generatedDefault);
	assert(generated->bytes.empty() && generated->skeleton.numBones==1 && generated->skeleton.numFrames==1 && generated->skeleton.scale==1);
	assert(!std::strcmp(registry.AnimationFileName(builtin.handle),"*default"));
	assert(!std::strcmp(registry.AnimationInternalName(builtinHandle),"*default"));
	assert(generated->generatedDefault->parent==-1 && generated->generatedDefault->children==0 && generated->generatedDefault->frameIndex==0);
	assert(!std::strcmp(generated->generatedDefault->boneName,"ModView internal default"));
	for (int i=0;i<12;++i) {
		const float expected=(i==0 || i==5 || i==10) ? 1.0f : 0.0f;
		assert(generated->generatedDefault->rest[i]==expected && generated->generatedDefault->inverseRest[i]==expected);
	}
	assert(generated->generatedDefault->quaternion[0]==1);
	for(int i=0;i<3;++i) assert(generated->generatedDefault->quaternion[i+1]==0 && generated->generatedDefault->translation[i]==0);
	const int beforeDefault=files.reads;
	assert(registry.Register("*DEFAULT.GLA").handle==builtinHandle && files.reads==beforeDefault);
	registry.Clear();
	assert(registry.Register("*DEFAULT.GLA").error==sp::PrecacheError::Missing);
	assert(registry.Register("*default.gla").error==sp::PrecacheError::Missing);
	registry.Clear();
	assert(registry.Register("*default.gla").handle==1 && registry.Register("*DEFAULT.GLA").handle==1);
	registry.Clear();assert(registry.Count()==0 && !registry.Resolve(1));
	/* A new mesh reserves its handle before recursively precaching its GLA.
	 * The retail registry's exact reservation order is observable by callers. */
	assert(registry.Register("models/test/model.glm").handle==1);
	assert(registry.Resolve(1)->animationHandle==2 && registry.Register("models/test/animation.gla").handle==2);
	registry.Clear();files.changed=true;
	assert(registry.Register("models/test/animation.gla").error==sp::PrecacheError::Missing);
	assert(!registry.Resolve(1));registry.Clear();files.changed=false;files.oversize=true;
	assert(registry.Register("oversize.gla").error==sp::PrecacheError::Missing);
	registry.Clear();files.oversize=false;files.throwRead=true;
	try { registry.Register("models/test/model.glm");assert(false); }
	catch (const std::runtime_error &) { assert(registry.Count()==1 && !registry.Resolve(1)); }
	registry.Clear();assert(registry.Count()==0);files.throwRead=false;
	/* Failed files consume the same bounded positive slots as retail. A
	 * repeat of a failed name still finds the cache when all slots are used. */
	for (std::size_t i=1;i<sp::ModelPrecache::Slots-1;++i)
		assert(registry.Register(("missing"+std::to_string(i)).c_str()).error==sp::PrecacheError::Missing);
	assert(registry.Register("models/test/animation.gla").handle==1023 && registry.Resolve(1023));
	const int fullReads=files.reads;
	assert(registry.Count()==1023 && registry.Register("overflow").error==sp::PrecacheError::Full);
	assert(registry.Register("MISSING1").error==sp::PrecacheError::Missing && files.reads==fullReads);
	registry.Clear();assert(registry.Register("models/test/animation.gla").handle==1);
	return 0;
}
