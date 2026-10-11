/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qfiles.h"
#include "../code/ghoul2/ghoul2_mdr.hpp"
#include <cstdlib>
#include <iostream>
#include <numeric>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while(0)
namespace geometry=fntech3::ghoul2::geometry;
namespace pose=fntech3::ghoul2::pose;
namespace mdr=fntech3::ghoul2::mdr;
namespace model=fntech3::jka::model;
using Bytes=std::vector<unsigned char>;
static bool Near(float a,float b,float tolerance=.0001f){return pose::Finite(a)&&pose::Finite(b)&&std::fabs(a-b)<tolerance;}
static geometry::Geometry Fixture(){
	geometry::Geometry result;result.name="models/test/weighted.glm";result.animationName="models/test/pose";
	result.numBones=3;result.version=6;result.lod=1;result.sourceBytes=2048;result.roots={0};result.surfaces.resize(3);
	for(int s=0;s<3;++s){
		auto &surface=result.surfaces[std::size_t(s)];surface.metadata.index=s;
		surface.metadata.name=s?"child":"root";surface.metadata.shader=s?"textures/child":"textures/root";
		surface.metadata.parent=s?0:-1;surface.metadata.flags=std::uint32_t(s);
		surface.metadata.sourceOffset=std::size_t(400+s*400);surface.metadata.sourceBytes=400;
		if(!s)surface.metadata.children={1,2};
		if(s==2)continue;
		for(int v=0;v<4;++v){
			geometry::WeightedVertex vertex;vertex.position={float(2+v+s*10),-3,5};vertex.normal={1,2,3};
			vertex.texCoord={float(v)*.125f,-.25f};vertex.count=v+1;
			float total=0;
			for(int w=0;w<vertex.count;++w){
				const int bones[]{2,0,1,2};vertex.bones[std::size_t(w)]=bones[w];
				const float weight=w+1==vertex.count?1-total:float(100*(w+1))/1023.0f;
				vertex.weights[std::size_t(w)]=weight;total+=weight;
			}
			surface.vertices.push_back(vertex);
		}
		surface.triangles={{0,2,1},{1,3,2}};
	}
	CHECK(geometry::Hierarchy(result));return result;
}
static std::vector<pose::Matrix> Palette(){
	return {{1,0,0,10,0,1,0,20,0,0,1,30},{0,-2,0,40,2,0,0,50,0,0,2,60},{-1,0,0,-5,0,3,0,7,0,0,.5f,-11}};
}

static void CheckReadBack(const geometry::Geometry &geometry,const std::vector<int> &visible,
	const std::vector<pose::Matrix> &palette,const Bytes &bytes)
{
	fnMdrView_t view;CHECK(FnMdr_Open(bytes.data(),bytes.size(),SHADER_MAX_VERTEXES,SHADER_MAX_INDEXES/3,&view));
	CHECK(view.numFrames==1&&view.numBones==palette.size()&&view.numLODs==1&&!view.numTags&&!view.compressed);
	CHECK(view.bytes==bytes.size()&&view.frames==104&&view.frameStride==56+palette.size()*48);
	CHECK(std::string(reinterpret_cast<const char*>(bytes.data()+8))==geometry.name);
	for(std::size_t i=0;i<palette.size();++i){
		float bone[3][4];CHECK(FnMdr_Bone(&view,0,std::uint32_t(i),bone));
		CHECK(std::memcmp(bone,palette[i].data(),48)==0);
	}
	fnMdrLodView_t lod;CHECK(FnMdr_Lod(&view,view.lods,&lod));CHECK(lod.numSurfaces==visible.size());
	geometry::DeformedGeometry expected;CHECK(geometry::Deform(geometry,palette,expected)==geometry::Error::None);
	geometry::Vector minima{},maxima{};CHECK(mdr::Bounds(geometry,visible,palette,minima,maxima)==mdr::Error::None);
	const auto frame=bytes.data()+view.frames;
	for(std::size_t axis=0;axis<3;++axis){
		CHECK(FnMdr_Float(frame+axis*4)==minima[axis]&&FnMdr_Float(frame+12+axis*4)==maxima[axis]);
		CHECK(Near(FnMdr_Float(frame+24+axis*4),float((double(minima[axis])+maxima[axis])*.5)));
	}
	const float radius=FnMdr_Float(frame+36);CHECK(pose::Finite(radius)&&radius>=0);
	std::uint32_t offset=lod.surfaces;
	for(std::size_t s=0;s<visible.size();++s){
		fnMdrSurfaceView_t surface;CHECK(FnMdr_Surface(&lod,offset,&surface));
		const auto &source=geometry.surfaces[std::size_t(visible[s])];
		CHECK(surface.numVerts==source.vertices.size()&&surface.numTriangles==source.triangles.size());
		CHECK(std::string(reinterpret_cast<const char*>(surface.data+4))==source.metadata.name);
		CHECK(std::string(reinterpret_cast<const char*>(surface.data+68))==source.metadata.shader);
		CHECK(FnMdr_U32(surface.data+136)==0u-std::uint32_t(surface.data-bytes.data()));
		std::array<unsigned char,FN_MDR_MAX_BONES> references{};
		for(std::uint32_t i=0;i<surface.numBoneReferences;++i){
			const auto bone=FnMdr_U32(surface.data+surface.boneReferences+std::size_t(i)*4);
			CHECK(bone<palette.size()&&!references[bone]);references[bone]=1;
		}
		std::uint32_t at=surface.vertices;
		for(std::size_t v=0;v<source.vertices.size();++v){
			std::uint32_t count,next;CHECK(FnMdr_Vertex(&surface,at,&count,&next));CHECK(count==std::uint32_t(source.vertices[v].count));
			const auto vertex=surface.data+at;geometry::Vector position{},normal{};
			for(std::size_t axis=0;axis<3;++axis)CHECK(FnMdr_Float(vertex+axis*4)==source.vertices[v].normal[axis]);
			for(std::size_t axis=0;axis<2;++axis)CHECK(FnMdr_Float(vertex+12+axis*4)==source.vertices[v].texCoord[axis]);
			for(std::uint32_t w=0;w<count;++w){
				const auto weight=vertex+24+std::size_t(w)*20;const auto bone=FnMdr_U32(weight);float matrix[3][4];
				CHECK(bone==std::uint32_t(source.vertices[v].bones[w])&&references[bone]);
				CHECK(FnMdr_Bone(&view,0,bone,matrix));const float amount=FnMdr_Float(weight+4);
				CHECK(amount==source.vertices[v].weights[w]);
				for(std::size_t component=0;component<3;++component)CHECK(FnMdr_Float(weight+8+component*4)==source.vertices[v].position[component]);
				for(std::size_t axis=0;axis<3;++axis){
					// Independently consume the canonical records as the MDR backend does.
					const float x=FnMdr_Float(weight+8),y=FnMdr_Float(weight+12),z=FnMdr_Float(weight+16);
					position[axis]+=amount*(matrix[axis][0]*x+matrix[axis][1]*y+matrix[axis][2]*z+matrix[axis][3]);
					normal[axis]+=amount*(matrix[axis][0]*FnMdr_Float(vertex)+matrix[axis][1]*FnMdr_Float(vertex+4)+matrix[axis][2]*FnMdr_Float(vertex+8));
				}
			}
			const auto &predicted=expected.surfaces[std::size_t(visible[s])].vertices[v];
			double distanceSquared=0;
			for(std::size_t axis=0;axis<3;++axis){
				CHECK(Near(position[axis],predicted.position[axis],.0005f)&&Near(normal[axis],predicted.normal[axis],.0005f));
				CHECK(position[axis]>=minima[axis]-.0005f&&position[axis]<=maxima[axis]+.0005f);
				const double distance=double(position[axis])-FnMdr_Float(frame+24+axis*4);distanceSquared+=distance*distance;
			}
			CHECK(distanceSquared<=double(radius)*radius+.01);at=next;
		}
		for(std::size_t t=0;t<source.triangles.size();++t)for(std::size_t corner=0;corner<3;++corner)
			CHECK(FnMdr_U32(surface.data+surface.triangles+t*12+corner*4)==source.triangles[t][corner]);
		offset+=surface.bytes;
	}
	CHECK(offset==lod.bytes);
}

static void CheckBuilder(){
	const auto geometry=Fixture();const auto palette=Palette();Bytes out{0xa5};
	CHECK(mdr::Build(geometry,{1,0,2},palette,out)==mdr::Error::None);CheckReadBack(geometry,{1,0,2},palette,out);
	CHECK(FnMdr_U32(out.data())==MDR_IDENT&&FnMdr_U32(out.data()+4)==MDR_VERSION);
	CHECK(mdr::Build(geometry,{0},palette,out)==mdr::Error::None);CheckReadBack(geometry,{0},palette,out);
	// Known one-weight result from the independently specified nonidentity matrix.
	geometry::Vector minima{},maxima{};CHECK(mdr::Bounds(geometry,{0},palette,minima,maxima)==mdr::Error::None);
	CHECK(minima[0]<=-7&&minima[1]<=-2&&minima[2]<=-8.5f);
	const auto prior=out;CHECK(mdr::Build(geometry,{},palette,out)==mdr::Error::Empty&&out==prior);
	CHECK(mdr::Build(geometry,{2},palette,out)==mdr::Error::Empty&&out==prior);
	minima={-777,-777,-777};maxima={888,888,888};const auto oldMinima=minima,oldMaxima=maxima;
	CHECK(mdr::Bounds(geometry,{},palette,minima,maxima)==mdr::Error::Empty&&minima==oldMinima&&maxima==oldMaxima);
	CHECK(mdr::Bounds(geometry,{2},palette,minima,maxima)==mdr::Error::Empty&&minima==oldMinima&&maxima==oldMaxima);
	Bytes unaligned(out.size()+2,0xa5);
	std::memcpy(unaligned.data()+1,out.data(),out.size());fnMdrView_t view;
	CHECK(FnMdr_Open(unaligned.data()+1,out.size(),SHADER_MAX_VERTEXES,SHADER_MAX_INDEXES/3,&view));
	CHECK(unaligned.front()==0xa5&&unaligned.back()==0xa5);
}
static void CheckLimitsAndRollback(){
	const auto input=Fixture();const auto palette=Palette();Bytes output{1,2,3,4};
	CHECK(mdr::Build(input,{0},palette,output)==mdr::Error::None);const auto prior=output;
	for(int kind=0;kind<22;++kind){
		auto broken=input;auto bones=palette;std::vector<int> visible{0};
		if(kind==0)visible={-1};
		if(kind==1)visible={3};
		if(kind==2)visible={0,0};
		if(kind==3)bones.resize(129);
		if(kind==4)bones.resize(2);
		if(kind==5)broken.numBones=129;
		if(kind==6)broken.surfaces[0].vertices[0].bones[0]=3;
		if(kind==7)broken.surfaces[0].vertices[0].count=5;
		if(kind==8)broken.surfaces[0].vertices[0].weights[0]=.5f;
		if(kind==9)broken.surfaces[0].vertices[0].weights[0]=-.1f;
		if(kind==10)broken.surfaces[0].triangles[0][0]=4;
		if(kind==11)broken.name=std::string(64,'n');
		if(kind==12)broken.surfaces[0].metadata.name=std::string(64,'n');
		if(kind==13)broken.surfaces[0].metadata.shader=std::string(64,'s');
		if(kind==14)broken.surfaces[0].metadata.shader=std::string("a\0b",3);
		if(kind==15)broken.surfaces[0].vertices.resize(SHADER_MAX_VERTEXES,broken.surfaces[0].vertices[0]);
		if(kind==16)broken.surfaces[0].triangles.resize(SHADER_MAX_INDEXES/3,broken.surfaces[0].triangles[0]);
		if(kind==17)broken.surfaces[0].metadata.children.push_back(1);
		if(kind==18)broken.version=7;
		if(kind==19)broken.sourceBytes=geometry::MaxFileBytes+1;
		if(kind==20)broken.surfaces[0].vertices[0].normal[1]=std::numeric_limits<float>::max();
		if(kind==21)broken.surfaces[0].vertices[0].position[1]=std::numeric_limits<float>::max();
		geometry::Vector low{-777,-777,-777},high{888,888,888};const auto oldLow=low,oldHigh=high;
		CHECK(mdr::Build(broken,visible,bones,output)!=mdr::Error::None&&output==prior);
		CHECK(mdr::Bounds(broken,visible,bones,low,high)!=mdr::Error::None&&low==oldLow&&high==oldHigh);
	}
	for(std::uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u})for(int target=0;target<5;++target){
		auto broken=input;auto bones=palette;float value;std::memcpy(&value,&bits,4);
		if(target==0)bones[0][3]=value;
		if(target==1)broken.surfaces[0].vertices[0].position[0]=value;
		if(target==2)broken.surfaces[0].vertices[0].normal[0]=value;
		if(target==3)broken.surfaces[0].vertices[0].texCoord[0]=value;
		if(target==4)broken.surfaces[0].vertices[0].weights[0]=value;
		geometry::Vector low{-777,-777,-777},high{888,888,888};const auto oldLow=low,oldHigh=high;
		CHECK(mdr::Build(broken,{0},bones,output)!=mdr::Error::None&&output==prior);
		CHECK(mdr::Bounds(broken,{0},bones,low,high)!=mdr::Error::None&&low==oldLow&&high==oldHigh);
	}
	auto boundary=input;boundary.surfaces[0].vertices.resize(SHADER_MAX_VERTEXES-1,boundary.surfaces[0].vertices[0]);
	boundary.surfaces[0].triangles.resize((SHADER_MAX_INDEXES-1)/3,boundary.surfaces[0].triangles[0]);
	CHECK(mdr::Build(boundary,{0},palette,output)==mdr::Error::None);CheckReadBack(boundary,{0},palette,output);
	fnMdrView_t view;fnMdrLodView_t lod;fnMdrSurfaceView_t surface;
	CHECK(FnMdr_Open(output.data(),output.size(),SHADER_MAX_VERTEXES,SHADER_MAX_INDEXES/3,&view));
	CHECK(FnMdr_Lod(&view,view.lods,&lod)&&FnMdr_Surface(&lod,lod.surfaces,&surface)&&surface.numBoneReferences==3);
	// Allocation budget checked before producing a large serialized body.
	auto large=input;large.surfaces.resize(200);large.surfaces[0].metadata.children.clear();
	for(std::size_t i=0;i<large.surfaces.size();++i){
		auto &s=large.surfaces[i];s.metadata.index=int(i);s.metadata.parent=i?0:-1;s.metadata.name="body";s.metadata.shader="skin";
		if(i)large.surfaces[0].metadata.children.push_back(int(i));
		s.vertices.assign(SHADER_MAX_VERTEXES-1,input.surfaces[0].vertices[3]);s.triangles={{0,1,2}};
	}
	std::vector<int> all(large.surfaces.size());std::iota(all.begin(),all.end(),0);const auto oldOutput=output;
	CHECK(mdr::Build(large,all,palette,output)==mdr::Error::Bounds&&output==oldOutput);
	// Finite deformed positions can still require an unrepresentable radius.
	auto enormous=input;const pose::Matrix identity{1,0,0,0,0,1,0,0,0,0,1,0};
	for(std::size_t i=0;i<enormous.surfaces[0].vertices.size();++i){
		auto &vertex=enormous.surfaces[0].vertices[i];vertex.count=1;vertex.bones[0]=0;vertex.weights[0]=1;
		const float extent=i%2?std::numeric_limits<float>::max():-std::numeric_limits<float>::max();
		vertex.position={extent,extent,0};
	}
	CHECK(mdr::Build(enormous,{0},{identity,identity,identity},output)==mdr::Error::Numeric&&output==oldOutput);
	// Exact field limits are accepted, without truncating names or shaders.
	auto names=input;names.name=std::string(63,'n');names.surfaces[0].metadata.name=std::string(63,'s');
	names.surfaces[0].metadata.shader=std::string(63,'t');
	CHECK(mdr::Build(names,{0},palette,output)==mdr::Error::None);CheckReadBack(names,{0},palette,output);
	CHECK(std::string(mdr::ErrorName(static_cast<mdr::Error>(99)))=="unknown MDR bridge error");
}

static void CheckInstalledMemory(){
#ifdef _WIN32
	_setmode(_fileno(stdin),_O_BINARY);
#endif
	std::uint32_t count;std::cin.read(reinterpret_cast<char*>(&count),4);CHECK(std::cin.good()&&count>0&&count<4096);
	std::size_t converted=0,inlineModels=0,bytesTotal=0;
	for(std::uint32_t entry=0;entry<count;++entry){
		std::uint32_t sizes[3];std::cin.read(reinterpret_cast<char*>(sizes),12);CHECK(std::cin.good());
		CHECK(sizes[0]>0&&sizes[0]<256&&sizes[1]>=164&&sizes[1]<=geometry::MaxFileBytes&&sizes[2]<=geometry::MaxFileBytes);
		std::string name(sizes[0],'\0');Bytes mesh(sizes[1]),skeleton(sizes[2]);
		std::cin.read(name.data(),sizes[0]);std::cin.read(reinterpret_cast<char*>(mesh.data()),sizes[1]);
		if(sizes[2])std::cin.read(reinterpret_cast<char*>(skeleton.data()),sizes[2]);
		CHECK(std::cin.good());
		if(skeleton.empty()){++inlineModels;continue;}
		model::Skeleton parsed;CHECK(model::ParseSkeleton(skeleton.data(),skeleton.size(),parsed)==model::Error::None);
		pose::Palette palette;CHECK(pose::Evaluate(skeleton.data(),skeleton.size(),parsed,{}, {},palette));
		model::Reader reader(mesh.data(),mesh.size());
		for(int level=0;level<reader.Int(144);++level){
			geometry::Geometry source;CHECK(geometry::Load(mesh.data(),mesh.size(),parsed.numBones,level,source)==geometry::Error::None);
			std::vector<int> visible;for(std::size_t i=0;i<source.surfaces.size();++i)visible.push_back(int(i));
			Bytes bytes;const auto error=mdr::Build(source,visible,palette.deformation,bytes);
			if(error!=mdr::Error::None)std::cerr<<name<<" LOD"<<level<<" "<<mdr::ErrorName(error)<<"\n";
			CHECK(error==mdr::Error::None);CheckReadBack(source,visible,palette.deformation,bytes);
			bytesTotal+=bytes.size();++converted;
		}
	}
	std::cout<<converted<<" installed GLA-posed LODs converted/read back; "<<bytesTotal<<" MDR bytes checked in memory; "
		<<inlineModels<<" inline models left without invented palettes\n";
}
int main(int argc,char **argv){
	CheckBuilder();CheckLimitsAndRollback();
	if(argc==2&&std::strcmp(argv[1],"--installed-memory")==0)CheckInstalledMemory();
	std::cout<<"Ghoul2 canonical MDR builder tests passed\n";return 0;
}
