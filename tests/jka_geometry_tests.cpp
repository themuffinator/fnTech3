/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#define main GeometryOwnershipFixtureMain
#include "raven_ghoul2_server_tests.cpp"
#undef main
#include "../code/ghoul2/ghoul2_geometry.hpp"
#include <array>
#include <limits>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace geometry = fntech3::ghoul2::geometry;
namespace pose = fntech3::ghoul2::pose;

static bool GeometryNear(float a, float b, float tolerance = .00005f) {
	return pose::Finite(a) && pose::Finite(b) && std::fabs(a - b) < tolerance;
}
static void GeometryFloat(Bytes &file, std::size_t at, float value) {
	std::uint32_t bits; std::memcpy(&bits, &value, 4); PutInt(file, at, std::int32_t(bits));
}
static Bytes GeometryFixture() {
	Bytes file;
	AppendInt(file, std::int32_t(model::MeshIdent)); AppendInt(file, model::MeshVersion);
	AppendName(file, "models/players/weighted/model.glm"); AppendName(file, "models/players/weighted/pose");
	AppendInt(file, 0); AppendInt(file, 3); AppendInt(file, 2); AppendInt(file, 0);
	AppendInt(file, 2); AppendInt(file, 172); AppendInt(file, 0);
	AppendInt(file, 0); AppendInt(file, 0);
	for (int i = 0; i < 2; ++i) {
		PutInt(file, 164 + std::size_t(i) * 4, std::int32_t(file.size() - 164));
		AppendName(file, i ? "child" : "root"); AppendInt(file, i ? 2 : 1);
		AppendName(file, i ? "textures/child" : "textures/root"); AppendInt(file, 0);
		AppendInt(file, i ? 0 : -1); AppendInt(file, i ? 0 : 1);
		if (!i) AppendInt(file, 1);
	}
	PutInt(file, 148, std::int32_t(file.size()));
	for (int level = 0; level < 2; ++level) {
		const auto lod = file.size(); AppendInt(file, 0);
		const auto table = file.size(); AppendInt(file, 0); AppendInt(file, 0);
		for (int i = 0; i < 2; ++i) {
			const auto surface = file.size();
			PutInt(file, table + std::size_t(i) * 4, std::int32_t(surface - table));
			AppendSurface(file, level ? 3 : 4, level ? 1 : 2, 3);
			PutInt(file, surface + 4, i);
			model::Reader reader(file.data(), file.size());
			const auto refs = surface + std::size_t(reader.Int(surface + 32));
			PutInt(file, refs, 2); PutInt(file, refs + 4, 0); PutInt(file, refs + 8, 1);
			const int vertices = reader.Int(surface + 12);
			const auto verts = surface + std::size_t(reader.Int(surface + 16));
			for (int v = 0; v < vertices; ++v) {
				const auto at = verts + std::size_t(v) * 32; const int count = v + 1;
				std::uint32_t packed = std::uint32_t(count - 1) << 30;
				for (int w = 0; w < count; ++w) {
					const auto raw = std::uint32_t(100 * (w + 1));
					packed |= std::uint32_t(w % 3) << (w * 5);
					if (w + 1 < count) packed |= (raw & 0x300u) << (12 + w * 2);
					file[at + 28 + std::size_t(w)] = std::uint8_t(raw);
				}
				PutInt(file, at + 24, std::int32_t(packed));
				for (int axis = 0; axis < 3; ++axis) GeometryFloat(file, at + std::size_t(axis) * 4, float(axis + 1));
				GeometryFloat(file, at + 12, float(v + 2)); GeometryFloat(file, at + 16, -3); GeometryFloat(file, at + 20, 5);
				GeometryFloat(file, verts + std::size_t(vertices) * 32 + std::size_t(v) * 8, float(v) * .125f);
				GeometryFloat(file, verts + std::size_t(vertices) * 32 + std::size_t(v) * 8 + 4, -.25f);
			}
		}
		PutInt(file, lod, std::int32_t(file.size() - lod));
	}
	PutInt(file, 160, std::int32_t(file.size()));
	return file;
}
static std::vector<pose::Matrix> GeometryPalette() {
	return {{1,0,0,10, 0,1,0,20, 0,0,1,30},
		{0,-2,0,40, 2,0,0,50, 0,0,2,60},
		{-1,0,0,-5, 0,3,0,7, 0,0,.5f,-11}};
}

static void CheckGeometryAndWeights() {
	Bytes file = GeometryFixture(); geometry::Geometry mesh;
	CHECK(geometry::Load(file.data(), file.size(), 3, 0, mesh) == geometry::Error::None);
	CHECK(mesh.surfaces.size() == 2 && mesh.lod == 0 && mesh.roots == std::vector<int>{0});
	const auto &root = mesh.surfaces[0]; const auto &child = mesh.surfaces[1];
	CHECK(root.metadata.index == 0 && root.metadata.flags == 1 && root.metadata.parent == -1);
	CHECK(root.metadata.children == std::vector<int>{1} && child.metadata.parent == 0);
	CHECK(child.metadata.flags == 2 && child.metadata.shader == "textures/child");
	CHECK(mesh.sourceBytes == file.size() && root.metadata.sourceBytes > 40);
	CHECK(root.triangles[0] == (geometry::Triangle{0,1,2}) && root.triangles[1] == (geometry::Triangle{1,2,3}));
	model::Mesh parsed; CHECK(model::ParseMesh(file.data(),file.size(),3,parsed) == model::Error::None);
	model::MeshSurface surface; CHECK(model::SurfaceAt(file.data(),file.size(),parsed,0,0,surface));
	for (int v = 0; v < 4; ++v) {
		model::SkinnedVertex decoded; CHECK(model::VertexAt(file.data(),file.size(),surface,v,decoded));
		CHECK(decoded.count == v + 1); float total = 0;
		for (int w = 0; w < decoded.count; ++w) total += decoded.weight[w];
		CHECK(GeometryNear(total, 1));
		const auto expectedLast = 1.0f - float(50 * v * (v + 1)) / 1023.0f;
		CHECK(GeometryNear(decoded.weight[v], expectedLast));
	}
	geometry::DeformedGeometry result;
	CHECK(geometry::Deform(mesh, GeometryPalette(), result) == geometry::Error::None);
	const auto &vertices = result.surfaces[0].vertices;
	CHECK(GeometryNear(vertices[0].position[0], -7) && GeometryNear(vertices[0].position[1], -2)
		&& GeometryNear(vertices[0].position[2], -8.5f));
	CHECK(vertices[0].normal == (geometry::Vector{-1,6,1.5f}));
	const float q = 100.0f / 1023.0f;
	CHECK(GeometryNear(vertices[1].position[0], -8 * q + 13 * (1 - q)));
	CHECK(GeometryNear(vertices[1].normal[1], 6 * q + 2 * (1 - q)));
	// Three distinct transforms: the implicit third weight is723/1023.
	CHECK(GeometryNear(vertices[2].position[0], (-9.0f*100 + 14.0f*200 + 46.0f*723)/1023.0f));
	CHECK(GeometryNear(vertices[2].position[1], (-2.0f*100 + 17.0f*200 + 58.0f*723)/1023.0f));
	// Four weights include a repeated bone. The fourth remains423/1023.
	CHECK(GeometryNear(vertices[3].position[2], (-8.5f*100 + 35.0f*200 + 70.0f*300 - 8.5f*423)/1023.0f));
	CHECK(GeometryNear(vertices[3].normal[2], (1.5f*100 + 3.0f*200 + 6.0f*300 + 1.5f*423)/1023.0f));
	CHECK(vertices[3].texCoord == (geometry::TexCoord{.375f,-.25f}));
	CHECK(result.surfaces[0].triangles == root.triangles && result.surfaces[1].metadata.flags == 2);
	CHECK(geometry::Load(file.data(),file.size(),3,1,mesh) == geometry::Error::None);
	CHECK(mesh.lod == 1 && mesh.surfaces[0].vertices.size() == 3 && mesh.surfaces[0].triangles.size() == 1);
	// Asset lifetime and alignment do not leak into the owned geometry.
	Bytes unaligned(file.size()+2,0xa5); std::memcpy(unaligned.data()+1,file.data(),file.size());
	CHECK(geometry::Load(unaligned.data()+1,file.size(),3,0,mesh) == geometry::Error::None);
	CHECK(unaligned.front()==0xa5 && unaligned.back()==0xa5);
	file.clear(); unaligned.clear(); CHECK(geometry::Deform(mesh,GeometryPalette(),result)==geometry::Error::None);
}

static void CheckGeometryFailures() {
	const Bytes file=GeometryFixture(); geometry::Geometry out;
	CHECK(geometry::Load(file.data(),file.size(),3,0,out)==geometry::Error::None);
	const auto sourceSize=out.sourceBytes, vertices=out.surfaces[0].vertices.size();
	model::Mesh parsed;CHECK(model::ParseMesh(file.data(),file.size(),3,parsed)==model::Error::None);
	model::MeshSurface surface;CHECK(model::SurfaceAt(file.data(),file.size(),parsed,0,0,surface));
	const auto hierarchy=std::size_t(172), childHierarchy=hierarchy+148;
	for(int kind=0;kind<22;++kind){
		Bytes damaged=file;
		if(kind==0)PutInt(damaged,160,std::int32_t(file.size()-1));
		if(kind==1)PutInt(damaged,144,geometry::MaxLODs+1);
		if(kind==2)PutInt(damaged,152,int(geometry::MaxSurfaces)+1);
		if(kind==3)PutInt(damaged,140,257);
		if(kind==4)PutInt(damaged,parsed.lodOffset+4,0);
		if(kind==5)PutInt(damaged,surface.offset+36,41);
		if(kind==6)PutInt(damaged,surface.offset+16,8);
		if(kind==7)PutInt(damaged,surface.triangleOffset,4);
		if(kind==8)PutInt(damaged,surface.boneReferenceOffset,3);
		if(kind==9)PutInt(damaged,hierarchy+136,0);
		if(kind==10)PutInt(damaged,childHierarchy+136,-1);
		if(kind==11)PutInt(damaged,hierarchy+144,0);
		if(kind==12)PutInt(damaged,hierarchy+140,2);
		if(kind==13)PutInt(damaged,surface.vertexOffset+12,0x7fc00001);
		if(kind==14)PutInt(damaged,surface.vertexOffset,0x7f800000);
		if(kind==15)PutInt(damaged,surface.texCoordOffset,std::int32_t(0xff800000u));
		if(kind==16)PutInt(damaged,surface.offset+12,std::numeric_limits<int>::max());
		if(kind==17)damaged.resize(surface.vertexOffset+17);
		if(kind==18)PutInt(damaged,hierarchy+64,std::int32_t(0x80000000u));
		if(kind==19){
			const auto vertex=surface.vertexOffset+2*32;
			PutInt(damaged,vertex+24,std::int32_t(0x80000000u | (1u<<5) | (2u<<10) | (0x300u<<12) | (0x300u<<14)));
			damaged[vertex+28]=255;damaged[vertex+29]=255;
		}
		if(kind==20)PutInt(damaged,surface.offset+32,4);
		if(kind==21)PutInt(damaged,160,-1);
		const auto error=geometry::Load(damaged.data(),damaged.size(),3,0,out);
		if(kind==18){CHECK(error==geometry::Error::None&&out.surfaces[0].metadata.flags==0x80000000u);
			CHECK(geometry::Load(file.data(),file.size(),3,0,out)==geometry::Error::None);}
		else CHECK(error!=geometry::Error::None);
		CHECK(out.sourceBytes==sourceSize&&out.surfaces[0].vertices.size()==vertices&&out.surfaces[0].metadata.flags==1);
	}
	CHECK(geometry::Load(nullptr,file.size(),3,0,out)==geometry::Error::Bounds);
	CHECK(geometry::Load(file.data(),geometry::MaxFileBytes+1,3,0,out)==geometry::Error::Bounds);
	CHECK(geometry::Load(file.data(),file.size(),0,0,out)==geometry::Error::Bounds);
	CHECK(geometry::Load(file.data(),file.size(),3,-1,out)==geometry::Error::Bounds);
	CHECK(geometry::Load(file.data(),file.size(),3,2,out)==geometry::Error::Bounds);
	geometry::DeformedGeometry result;CHECK(geometry::Deform(out,GeometryPalette(),result)==geometry::Error::None);
	const auto prior=result.surfaces[0].vertices[0].position;
	for(int kind=0;kind<9;++kind){
		auto broken=out;auto palette=GeometryPalette();
		if(kind==0)palette.resize(2);
		if(kind==1){std::uint32_t bits=0x7fc00001u;std::memcpy(&palette[0][3],&bits,4);}
		if(kind==2)broken.surfaces[0].vertices[0].count=5;
		if(kind==3)broken.surfaces[0].vertices[0].weights[0]=.5f;
		if(kind==4)broken.surfaces[0].vertices[0].bones[0]=3;
		if(kind==5)broken.surfaces[0].triangles[0][0]=4;
		if(kind==6)broken.surfaces[0].metadata.children.push_back(1);
		if(kind==7)broken.surfaces[0].vertices[0].position[1]=std::numeric_limits<float>::max();
		if(kind==8)broken.surfaces[0].vertices[0].normal[1]=std::numeric_limits<float>::max();
		CHECK(geometry::Deform(broken,palette,result)!=geometry::Error::None);
		CHECK(result.surfaces[0].vertices[0].position==prior);
	}
}

static Bytes GeometrySkeleton(){
	Bytes data=Skeleton("models/players/weighted/pose",3);model::Reader reader(data.data(),data.size());
	for(int bone=0;bone<3;++bone){
		const auto at=100+std::size_t(reader.Int(100+std::size_t(bone)*4));
		if(bone==2){std::memset(data.data()+at,0,64);std::memcpy(data.data()+at,"leaf",4);}
		for(int element=0;element<12;++element){
			const bool diagonal=element==0||element==5||element==10;
			GeometryFloat(data,at+72+std::size_t(element)*4,diagonal?.5f:element==3?float((bone+1)*4):0.0f);
			GeometryFloat(data,at+120+std::size_t(element)*4,diagonal?2.0f:element==3?float(-(bone+1)*8):0.0f);
		}
	}
	const auto pool=std::size_t(reader.Int(88));const unsigned words[]{49149,32766,32766,32766,32768,32768,32768};
	for(std::size_t i=0;i<7;++i){data[pool+i*2]=std::uint8_t(words[i]);data[pool+i*2+1]=std::uint8_t(words[i]>>8);}
	return data;
}
static void CheckOwnedStorePalette(){
	TestFiles files;files.files["models/players/weighted/model.glm"]=GeometryFixture();
	files.files["models/players/weighted/pose.gla"]=GeometrySkeleton();
	ghoul2::Store store(files);void *instance=nullptr;LoadError error;
	CHECK(store.AddModel(&instance,"models/players/weighted/model.glm",error)==0);
	const auto *model=store.Resolve(instance)->At(0);CHECK(model);
	pose::Palette palette;CHECK(store.DeformationPalette(*model,0,palette));
	geometry::Geometry mesh;CHECK(geometry::Load(model->meshBytes->data(),model->meshBytes->size(),model->skeleton.numBones,0,mesh)==geometry::Error::None);
	geometry::DeformedGeometry result;CHECK(geometry::Deform(mesh,palette.deformation,result)==geometry::Error::None);
	// Bind scale.5/translatedbones must not be applied to weighted positions.
	const auto &vertex=result.surfaces[0].vertices[0];
	const auto originalPosition=vertex.position;
	CHECK(GeometryNear(vertex.position[0],3)&&GeometryNear(vertex.position[1],2)&&GeometryNear(vertex.position[2],5));
	CHECK(GeometryNear(vertex.normal[0],-2)&&GeometryNear(vertex.normal[1],1)&&GeometryNear(vertex.normal[2],3));
	std::vector<float> bones;CHECK(store.BoneMatrices(*model,0,bones));
	for(std::size_t i=0;i<palette.bones.size();++i)for(std::size_t j=0;j<12;++j)CHECK(bones[i*12+j]==palette.bones[i][j]);
	const float bend[]{10,20,30};CHECK(store.SetBoneAngles(instance,0,"child",bend,2,1,3,2,0,0));
	CHECK(store.DeformationPalette(*model,0,palette));
	CHECK(geometry::Deform(mesh,palette.deformation,result)==geometry::Error::None);
	CHECK(result.surfaces[0].vertices[0].position!=originalPosition);
	const auto before=palette.deformation;
	CHECK(store.SetBoneAngles(instance,0,"child",bend,7,1,3,2,0,0));
	CHECK(!store.DeformationPalette(*model,0,palette)&&palette.deformation==before);
	CHECK(store.Clean(&instance)&&!instance);
}

static void CheckInstalledMemory(){
#ifdef _WIN32
	_setmode(_fileno(stdin),_O_BINARY);
#endif
	std::uint32_t count;std::cin.read(reinterpret_cast<char*>(&count),4);CHECK(std::cin.good()&&count>0&&count<4096);
	std::size_t levels=0,vertices=0,triangles=0,posed=0,inlineModels=0;
	for(std::uint32_t entry=0;entry<count;++entry){
		std::uint32_t sizes[3];std::cin.read(reinterpret_cast<char*>(sizes),12);CHECK(std::cin.good());
		CHECK(sizes[0]>0&&sizes[0]<256&&sizes[1]>=164&&sizes[1]<=geometry::MaxFileBytes&&sizes[2]<=geometry::MaxFileBytes);
		std::string name(sizes[0],'\0');Bytes mesh(sizes[1]),skeleton(sizes[2]);
		std::cin.read(name.data(),sizes[0]);std::cin.read(reinterpret_cast<char*>(mesh.data()),sizes[1]);
		if(sizes[2])std::cin.read(reinterpret_cast<char*>(skeleton.data()),sizes[2]);
		CHECK(std::cin.good());
		model::Reader reader(mesh.data(),mesh.size());int bones=reader.Int(140);pose::Palette palette;
		if(!skeleton.empty()){
			model::Skeleton parsed;CHECK(model::ParseSkeleton(skeleton.data(),skeleton.size(),parsed)==model::Error::None);
			bones=parsed.numBones;CHECK(pose::Evaluate(skeleton.data(),skeleton.size(),parsed,{}, {},palette));
		}else{CHECK(std::string(reinterpret_cast<const char*>(mesh.data())+72)=="*default");++inlineModels;}
		for(int level=0;level<reader.Int(144);++level){
			geometry::Geometry source;const auto loaded=geometry::Load(mesh.data(),mesh.size(),bones,level,source);
			if(loaded!=geometry::Error::None)std::cerr<<"Load failure "<<name<<" lod"<<level<<" error"<<int(loaded)<<"\n";
			CHECK(loaded==geometry::Error::None);++levels;
			geometry::DeformedGeometry result;
			if(!skeleton.empty()){CHECK(geometry::Deform(source,palette.deformation,result)==geometry::Error::None);++posed;}
			for(std::size_t s=0;s<source.surfaces.size();++s){
				vertices+=source.surfaces[s].vertices.size();triangles+=source.surfaces[s].triangles.size();
				if(!skeleton.empty()){
					CHECK(result.surfaces[s].triangles==source.surfaces[s].triangles);
					for(std::size_t v=0;v<source.surfaces[s].vertices.size();++v)
						CHECK(result.surfaces[s].vertices[v].texCoord==source.surfaces[s].vertices[v].texCoord);
				}
			}
		}
	}
	std::cout<<count<<" installed GLMs: "<<levels<<" LODs, "<<vertices<<" vertices, "<<triangles
		<<" triangles; "<<posed<<" posed LODs; "<<inlineModels<<" inline models parsed without invented poses\n";
}
int main(int argc,char **argv){
	CheckGeometryAndWeights();CheckGeometryFailures();CheckOwnedStorePalette();
	if(argc==2&&std::strcmp(argv[1],"--installed-memory")==0)CheckInstalledMemory();
	std::cout<<"Ghoul2 bounded weighted geometry tests passed\n";return 0;
}
