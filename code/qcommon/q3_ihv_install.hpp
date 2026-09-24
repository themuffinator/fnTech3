/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_INSTALL_HPP
#define FNTECH3_Q3_IHV_INSTALL_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

/* Early installation evidence, not executable authentication or a module
 * loader. The supplied IHV tree has loose BSP43 assets and Win32 function-table
 * modules beside the executable. No DLL code runs during profile detection.
 * PE layout: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format */
namespace fntech3::q3ihv::install {
inline std::uint32_t Word(const std::uint8_t *p) { return p[0] | (std::uint32_t(p[1])<<8); }
inline std::uint32_t Dword(const std::uint8_t *p) { return Word(p) | (Word(p+2)<<16); }

inline bool HasX86Export(const void *data,std::size_t size,const char *symbol) {
	if(!data || !symbol) return false;
	const auto *p=static_cast<const std::uint8_t *>(data);
	const auto fits=[size](std::size_t offset,std::size_t bytes) { return offset<=size && bytes<=size-offset; };
	if(!fits(0,64) || Word(p)!=0x5a4d) return false;
	const std::size_t nt=Dword(p+60);
	if(!fits(nt,24) || Dword(p+nt)!=0x00004550 || Word(p+nt+4)!=0x14c || !(Word(p+nt+22)&0x2000)) return false;
	const auto sectionCount=Word(p+nt+6),optionalBytes=Word(p+nt+20);
	const auto optional=nt+24;
	if(!sectionCount || sectionCount>96 || optionalBytes<104 || !fits(optional,optionalBytes)
		|| Word(p+optional)!=0x10b || !Dword(p+optional+92)) return false;
	const auto sections=optional+optionalBytes;
	if(!fits(sections,sectionCount*40)) return false;
	for(unsigned i=0;i<sectionCount;++i) {
		const auto *section=p+sections+i*40;
		if(!fits(Dword(section+20),Dword(section+16))) return false;
	}
	const auto rvaOffset=[&](std::uint32_t rva,std::size_t bytes,std::size_t &offset) {
		bool found=false;
		if(rva<Dword(p+optional+60) && bytes<=Dword(p+optional+60)-rva && fits(rva,bytes)) { offset=rva; found=true; }
		for(unsigned i=0;i<sectionCount;++i) {
			const auto *section=p+sections+i*40;
			const auto begin=Dword(section+12),length=Dword(section+16),raw=Dword(section+20);
			if(rva<begin || rva-begin>=length || bytes>length-(rva-begin)) continue;
			const std::uint64_t position=std::uint64_t(raw)+rva-begin;
			if(position>size || !fits(static_cast<std::size_t>(position),bytes) || found) return false;
			offset=static_cast<std::size_t>(position); found=true;
		}
		return found;
	};
	const auto exportRva=Dword(p+optional+96),exportBytes=Dword(p+optional+100);
	std::size_t directory=0,names=0,ordinals=0,functions=0;
	if(!exportRva || exportBytes<40 || !rvaOffset(exportRva,40,directory)) return false;
	const auto count=Dword(p+directory+24),functionCount=Dword(p+directory+20);
	if(!count || count>4096 || !functionCount || functionCount>4096
		|| !rvaOffset(Dword(p+directory+32),count*4,names)
		|| !rvaOffset(Dword(p+directory+36),count*2,ordinals)
		|| !rvaOffset(Dword(p+directory+28),functionCount*4,functions)) return false;
	const auto wanted=std::strlen(symbol); if(!wanted || wanted>=64) return false;
	bool match=false;
	for(unsigned i=0;i<count;++i) {
		std::size_t name=0; const auto ordinal=Word(p+ordinals+i*2);
		if(!rvaOffset(Dword(p+names+i*4),wanted+1,name)) continue;
		if(std::memcmp(p+name,symbol,wanted) || p[name+wanted]) continue;
		if(ordinal>=functionCount) return false;
		const auto function=Dword(p+functions+ordinal*4); std::size_t code=0;
		// A forwarded symbol does not establish this DLL's native boundary.
		if(!function || (function>=exportRva && std::uint64_t(function)-exportRva<exportBytes)
			|| !rvaOffset(function,1,code) || match) return false;
		match=true;
	}
	return match;
}

inline std::filesystem::path ContainedFile(const std::filesystem::path &root,const char *name) {
	std::error_code error;
	const auto file=std::filesystem::canonical(root/name,error); if(error) return {};
	const auto relative=file.lexically_relative(root);
	if(relative.empty() || relative.is_absolute() || *relative.begin()==".."
		|| !std::filesystem::is_regular_file(file,error) || error) return {};
	return file;
}
inline bool Module(const std::filesystem::path &root,const char *name,const char *symbol) {
	const auto path=ContainedFile(root,name); if(path.empty()) return false;
	std::ifstream file(path,std::ios::binary|std::ios::ate); const auto size=static_cast<std::streamoff>(file.tellg());
	if(!file || size<128 || size>16*1024*1024) return false;
	std::vector<std::uint8_t> data(static_cast<std::size_t>(size)); file.seekg(0);
	return bool(file.read(reinterpret_cast<char *>(data.data()),size)) && HasX86Export(data.data(),data.size(),symbol);
}
inline bool Map(const std::filesystem::path &root,const char *name) {
	const auto path=ContainedFile(root,name); if(path.empty()) return false;
	std::ifstream file(path,std::ios::binary|std::ios::ate); const auto size=static_cast<std::streamoff>(file.tellg());
	std::array<std::uint8_t,120> header{};
	if(!file || size<static_cast<std::streamoff>(header.size()) || size>INT32_MAX) return false;
	file.seekg(0); if(!file.read(reinterpret_cast<char *>(header.data()),header.size())) return false;
	if(std::memcmp(header.data(),"IBSP",4) || Dword(header.data()+4)!=43) return false;
	for(unsigned i=0;i<14;++i) {
		const auto offset=Dword(header.data()+8+i*8),length=Dword(header.data()+12+i*8);
		if(offset>size || length>size-offset || (length && offset<header.size())) return false;
	}
	return true;
}
inline bool Recognizes(const std::filesystem::path &path) {
	std::error_code error; const auto root=std::filesystem::canonical(path,error); if(error) return false;
	const auto config=ContainedFile(root,"baseq3/default.cfg"); if(config.empty()) return false;
	const auto configBytes=std::filesystem::file_size(config,error);
	if(error || !configBytes || configBytes>1024*1024) return false;
	// Test inexpensive asset evidence before reading either module.
	bool map=false;
	for(const char *name : {"baseq3/maps/ihv_test1.bsp","baseq3/maps/tim_dm1.bsp","baseq3/maps/km_portal.bsp","baseq3/maps/test_fog2.bsp"}) {
		if(Map(root,name)) { map=true; break; }
	}
	return map && Module(root,"qagamex86.dll","GetGameAPI") && Module(root,"cgamex86.dll","GetCGameAPI");
}
} // namespace fntech3::q3ihv::install
#endif
