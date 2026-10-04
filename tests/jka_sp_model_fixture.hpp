/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_TESTS_JKA_SP_MODEL_FIXTURE_HPP
#define FNTECH3_TESTS_JKA_SP_MODEL_FIXTURE_HPP
#include "../code/compat/jka_sp_model_precache.hpp"
#include <cassert>
#include <map>
namespace jka_sp_test {
using Bytes=std::vector<unsigned char>;
inline void Word(Bytes &bytes,std::size_t offset,std::uint32_t value) {
	assert(offset+4<=bytes.size());
	for(int i=0;i<4;++i)bytes[offset+std::size_t(i)]=static_cast<unsigned char>(value>>(i*8));
}
inline void Name(Bytes &bytes,std::size_t offset,const char *text) {
	assert(std::strlen(text)<64 && offset+64<=bytes.size());
	std::memcpy(bytes.data()+offset,text,std::strlen(text)+1);
}
/* Independently constructed format fixtures: one bone/frame, one surface and
 * vertex, no triangles. No proprietary bytes or installations are needed. */
inline Bytes Skeleton() {
	Bytes bytes(294,0);Word(bytes,0,fntech3::jka::model::SkeletonIdent);Word(bytes,4,6);
	Name(bytes,8,"skeleton");Word(bytes,72,0x3f800000);Word(bytes,76,1);
	Word(bytes,80,276);Word(bytes,84,1);Word(bytes,88,280);Word(bytes,92,104);Word(bytes,96,294);
	Word(bytes,100,4);Name(bytes,104,"root");Word(bytes,172,0xffffffffu);return bytes;
}
inline Bytes Mesh(const char *animation="models/test/animation") {
	Bytes bytes(404,0);Word(bytes,0,fntech3::jka::model::MeshIdent);Word(bytes,4,6);
	Name(bytes,8,"mesh");Name(bytes,72,animation);Word(bytes,140,1);Word(bytes,144,1);
	Word(bytes,148,312);Word(bytes,152,1);Word(bytes,156,168);Word(bytes,160,404);
	Word(bytes,164,4);Name(bytes,168,"surface");Name(bytes,236,"shader");Word(bytes,304,0xffffffffu);
	Word(bytes,312,92);Word(bytes,316,4);Word(bytes,328,std::uint32_t(-320));Word(bytes,332,1);
	Word(bytes,336,44);Word(bytes,344,44);Word(bytes,348,1);Word(bytes,352,40);Word(bytes,356,84);return bytes;
}
class Files final : public fntech3::ghoul2::Files {
public:
	std::map<std::string,Bytes> files;
	int reads=0;
	std::size_t Read(const char *name,void *destination,std::size_t capacity) override {
		++reads;const auto found=files.find(name);if(found==files.end())return 0;
		const auto size=found->second.size();
		if(destination && capacity>=size)std::memcpy(destination,found->second.data(),size);
		return size;
	}
};
}
#endif
