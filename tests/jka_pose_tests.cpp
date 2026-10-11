/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#define main RavenOwnershipFixtureMain
#include "raven_ghoul2_server_tests.cpp"
#undef main
#include <array>
#include <limits>
#include "jka_pose_retail_cases.hpp"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace pose = fntech3::ghoul2::pose;

static bool NearPose(float a, float b, float tolerance = .00005f) {
	return pose::Finite(a) && pose::Finite(b) && std::fabs(a-b) < tolerance;
}
static std::size_t BoneRecord(const Bytes &data, int bone) {
	return 100 + std::size_t(model::Reader(data.data(), data.size()).Int(100+std::size_t(bone)*4));
}
static void PutFloat(Bytes &data, std::size_t at, float value) {
	std::uint32_t word; std::memcpy(&word, &value, 4); PutInt(data, at, std::int32_t(word));
}
static Bytes PoseSkeleton() {
	Bytes file = Skeleton("models/players/test/skeleton", 3);
	const auto oldFrames = std::size_t(model::Reader(file.data(),file.size()).Int(80));
	file.resize(oldFrames);
	PutInt(file, 76, 3);
	for (int frame=0; frame<3; ++frame) for (int bone=0; bone<3; ++bone) {
		file.push_back(std::uint8_t(frame*3+bone)); file.push_back(0); file.push_back(0);
	}
	while (file.size()%4) file.push_back(0);
	PutInt(file,88,std::int32_t(file.size()));
	for (int frame=0; frame<3; ++frame) for (int bone=0; bone<3; ++bone) {
		const int values[] = {49149,32766,32766,32766,
			32768+64*(bone+frame),32768,32768-64*frame};
		for (int word : values) { file.push_back(std::uint8_t(word)); file.push_back(std::uint8_t(word>>8)); }
	}
	PutInt(file,96,std::int32_t(file.size()));
	for (int bone=0; bone<3; ++bone) {
		const auto at=BoneRecord(file,bone);
		const float scale=.5f, shift=float((bone+1)*4);
		for (int n=0;n<12;++n) {
			PutFloat(file,at+72+n*4,(n==0||n==5||n==10)?scale:n==3?shift:0);
			PutFloat(file,at+120+n*4,(n==0||n==5||n==10)?2:n==3?-shift*2:0);
		}
	}
	return file;
}

static void CheckHierarchyAndInheritance() {
	const Bytes file=PoseSkeleton();model::Skeleton skeleton;
	CHECK(model::ParseSkeleton(file.data(),file.size(),skeleton)==model::Error::None);
	pose::Palette palette;
	CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{}, {},palette));
	CHECK(palette.deformation.size()==3&&palette.bones.size()==3);
	// Local frame-zero x offsets (0,1,2) accumulate through the fixed root.
	CHECK(NearPose(palette.deformation[2][3],0)&&NearPose(palette.deformation[2][7],3));
	// The nonidentity bind adds12 modelunits to the leaf, with scale .5 retained.
	CHECK(NearPose(palette.bones[2][7],15)&&NearPose(palette.bones[2][1],-.5f));
	CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{{0,{1,2,.5f}}},{},palette));
	CHECK(NearPose(palette.deformation[2][7],7.5f)&&NearPose(palette.deformation[2][11],-4.5f));
	// An explicit child clock replaces inherited timing for that whole subtree.
	CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{{0,{1,2,.5f}},{1,{0,0,0}}},{},palette));
	CHECK(NearPose(palette.deformation[2][7],4.5f)&&NearPose(palette.deformation[2][11],-1.5f));
	pose::Matrix post;const float bend[]{10,20,30};
	std::vector<int> parents;std::vector<pose::Matrix> base,inverse;
	CHECK(pose::Hierarchy(file.data(),file.size(),skeleton,parents,base,inverse));
	CHECK(pose::PostAngle(base[1],inverse[1],bend,1,3,2,post));
	pose::Palette unchanged=palette;
	CHECK(pose::Evaluate(file.data(),file.size(),skeleton,{{0,{1,2,.5f}},{1,{0,0,0}}},{{1,post}},palette));
	CHECK(palette.bones[0]==unchanged.bones[0]&&palette.bones[1]!=unchanged.bones[1]&&palette.bones[2]!=unchanged.bones[2]);
	// The bend pivots around its bind location; the bone origin remains fixed.
	CHECK(NearPose(palette.bones[1][3],unchanged.bones[1][3])&&NearPose(palette.bones[1][7],unchanged.bones[1][7]));
	for(int up=1;up<=6;++up)for(int left=1;left<=6;++left)for(int forward=1;forward<=6;++forward){
		const bool valid=(up-1)%3!=(left-1)%3&&(up-1)%3!=(forward-1)%3&&(left-1)%3!=(forward-1)%3;
		CHECK(pose::PostAngle(base[1],inverse[1],bend,up,left,forward,post)==valid);
		if(valid)CHECK(pose::Finite(post));
	}
	// Damaged graphs and numeric input leave a previously valid palette intact.
	const auto prior=palette.deformation;
	for(int kind=0;kind<8;++kind){
		Bytes broken=file;auto at=BoneRecord(broken,1);
		if(kind==0)PutInt(broken,at+68,1);
		if(kind==1)PutInt(broken,at+68,2);
		if(kind==2)PutInt(broken,BoneRecord(broken,0)+model::BoneBytes,2);
		if(kind==3)PutInt(broken,at+72,0x7fc00001);
		if(kind==4)PutFloat(broken,at+120,0);
		if(kind==5)PutInt(broken,104,-1);
		if(kind==6)PutInt(broken,at+168,-1);
		if(kind==7)broken.resize(at+100);
		CHECK(!pose::Evaluate(broken.data(),broken.size(),skeleton,{}, {},palette));CHECK(palette.deformation==prior);
	}
	for(const auto frame:std::vector<pose::Frame>{{-1,0,0},{0,3,0},{0,1,-.1f},{0,1,1.1f}}){
		CHECK(!pose::Evaluate(file.data(),file.size(),skeleton,{{0,frame}}, {},palette));CHECK(palette.deformation==prior);
	}
	CHECK(!pose::Evaluate(file.data(),file.size(),skeleton,{{0,{0,0,0}},{0,{0,0,0}}}, {},palette));
	CHECK(!pose::Evaluate(file.data(),file.size(),skeleton,{{3,{0,0,0}}}, {},palette));
	for(std::uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
		float bad;std::memcpy(&bad,&bits,4);const float invalid[]{bad,0,0};
		CHECK(!pose::PostAngle(base[1],inverse[1],invalid,1,3,2,post));
		CHECK(!pose::Evaluate(file.data(),file.size(),skeleton,{{0,{0,1,bad}}}, {},palette));
	}
}

static void CheckPlacementAndStore() {
	TestFiles files;files.files["models/players/test/model.glm"]=Mesh("models/players/test/skeleton",3);
	files.files["models/players/test/skeleton.gla"]=PoseSkeleton();
	ghoul2::Store store(files);void *instance=nullptr;LoadError error;
	CHECK(store.AddModel(&instance,"models/players/test/model.glm",error)==0);
	const int bolt=store.AddBolt(instance,0,"child");CHECK(bolt==0);
	float raw[12],world[12];const float angles[]{0,0,0},position[]{7,-11,13},scale[]{2,.5f,0};
	CHECK(store.BoltMatrix(instance,0,bolt,0,raw));
	CHECK(pose::PlaceBolt(raw,angles,position,scale,false,world));
	CHECK(NearPose(world[0],1)&&NearPose(world[5],1)&&NearPose(world[10],1));
	CHECK(NearPose(world[3],7)&&NearPose(world[7],-6.5f)&&NearPose(world[11],13));
	CHECK(pose::PlaceBolt(raw,angles,position,scale,true,world));
	CHECK(NearPose(world[0],0)&&NearPose(world[1],-1)&&NearPose(world[4],1));
	std::array<unsigned char,50> guarded;guarded.fill(0xa5);
	CHECK(pose::PlaceBolt(raw,angles,position,scale,false,reinterpret_cast<float*>(guarded.data()+1)));
	CHECK(guarded.front()==0xa5&&guarded.back()==0xa5);
	const auto before=guarded;
	pose::Matrix damaged{};CHECK(!pose::PlaceBolt(damaged.data(),angles,position,scale,false,reinterpret_cast<float*>(guarded.data()+1)));
	CHECK(guarded==before);
	for(std::uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u}){
		float bad;std::memcpy(&bad,&bits,4);const float invalid[]{bad,0,0};
		CHECK(!pose::PlaceBolt(raw,invalid,position,scale,false,reinterpret_cast<float*>(guarded.data()+1)));
		CHECK(!pose::PlaceBolt(raw,angles,invalid,scale,false,reinterpret_cast<float*>(guarded.data()+1)));
		CHECK(!pose::PlaceBolt(raw,angles,position,invalid,false,reinterpret_cast<float*>(guarded.data()+1)));
		CHECK(guarded==before);
	}
	CHECK(store.SetBoneAnim(instance,0,"model_root",0,3,0x10,1,0,-1,0));
	CHECK(store.BoltMatrix(instance,0,bolt,75,raw));CHECK(NearPose(raw[7],12));
	// Retail UI animation requests carry duration150 without the blend flag.
	// The primary setter ignores it; both menu flag variants retain the same
	// non-blended pose as duration0. Actual BONE_ANIM_BLEND remains refused.
	for (int flags : {0x48, 0x58}) {
		CHECK(store.SetBoneAnim(instance,0,"model_root",0,3,flags,1,0,-1,0));
		CHECK(store.BoltMatrix(instance,0,bolt,75,raw));
		std::array<float,12> unblended;std::copy(raw,raw+12,unblended.begin());
		CHECK(store.SetBoneAnim(instance,0,"model_root",0,3,flags,1,0,-1,150));
		CHECK(store.BoltMatrix(instance,0,bolt,75,raw));
		CHECK(std::equal(raw,raw+12,unblended.begin()));
	}
	CHECK(store.SetBoneAnim(instance,0,"child",0,3,0x8,0,0,-1,0));
	CHECK(store.BoltMatrix(instance,0,bolt,75,raw));CHECK(NearPose(raw[7],10.5f));
	const float bend[]{10,20,30};CHECK(store.SetBoneAngles(instance,0,"child",bend,2,1,3,2,0,0));
	CHECK(store.BoltMatrix(instance,0,bolt,75,raw));
	for(int flag:{1,4}){
		CHECK(store.SetBoneAngles(instance,0,"child",bend,flag,1,3,2,0,0));
		CHECK(store.BoltMatrix(instance,0,bolt,75,raw));
	}
	for(int flag:{3,5,6,7,0x2000}){
		CHECK(store.SetBoneAngles(instance,0,"child",bend,flag,1,3,2,0,0));
		std::fill(raw,raw+12,-777.0f);CHECK(!store.BoltMatrix(instance,0,bolt,75,raw));
		for(float v:raw)CHECK(v==-777);
	}
	CHECK(store.SetBoneAngles(instance,0,"child",bend,0,0,0,0,0,0));
	CHECK(store.SetBoneAnim(instance,0,"model_root",0,3,0x80|0x8,1,0,-1,100));
	CHECK(!store.BoltMatrix(instance,0,bolt,75,raw));
	CHECK(store.Clean(&instance)&&!instance);
}

static void CheckRetailMemoryInput() {
#ifdef _WIN32
	_setmode(_fileno(stdin),_O_BINARY);
#endif
	TestFiles files;
	for (int file=0;file<2;++file) {
		std::uint32_t sizes[2];std::cin.read(reinterpret_cast<char*>(sizes),sizeof(sizes));CHECK(std::cin.good());
		CHECK(sizes[0]>0&&sizes[0]<256&&sizes[1]>0&&sizes[1]<=ghoul2::MaxModelBytes);
		std::string name(sizes[0],'\0');Bytes bytes(sizes[1]);
		std::cin.read(name.data(),sizes[0]);std::cin.read(reinterpret_cast<char*>(bytes.data()),sizes[1]);CHECK(std::cin.good());
		CHECK(files.files.emplace(name,std::move(bytes)).second);
	}
	ghoul2::Store store(files);void *instance=nullptr;LoadError error;
	CHECK(store.AddModel(&instance,"models/players/kyle/model.glm",error)==0);
	std::string phase="initial";double maximumError=0;
	for(const auto &observation:RetailPoseCases) {
		if(observation.phase!=phase) {
			phase=observation.phase;
			if(phase=="root_animation")CHECK(store.SetBoneAnim(instance,0,"model_root",0,30,0x8,1,0,-1,0));
			if(phase=="angle_override"){const float bend[]{10,20,30};CHECK(store.SetBoneAngles(instance,0,"thoracic",bend,2,1,3,2,0,100));}
			if(phase.find("fresh_f")==0){
				CHECK(store.Clean(&instance));CHECK(store.AddModel(&instance,"models/players/kyle/model.glm",error)==0);
				const int mode=phase[7]-'0',axis=phase.back()-'0';const int axes[3][3]{{1,3,2},{4,6,5},{2,1,3}};
				const float bend[]{10,20,30};CHECK(mode>=1&&mode<=4&&axis>=0&&axis<3);
				CHECK(store.SetBoneAngles(instance,0,"thoracic",bend,mode,axes[axis][0],axes[axis][1],axes[axis][2],0,observation.time));
			}
		}
		const int bolt=store.AddBolt(instance,0,observation.bone);CHECK(bolt>=0);
		float raw[12],placed[12];float angles[]{0,0,0},position[]{0,0,0},scale[]{1,1,1};
		if(phase.find("spatial_op")==0){angles[0]=15;angles[1]=25;angles[2]=35;position[0]=7;position[1]=-11;position[2]=13;scale[0]=2;scale[1]=.5f;scale[2]=0;}
		CHECK(store.BoltMatrix(instance,0,bolt,observation.time,raw));
		CHECK(pose::PlaceBolt(raw,angles,position,scale,phase=="spatial_op589",placed));
		for(int i=0;i<12;++i){float expected;std::memcpy(&expected,&observation.bits[i],4);
			maximumError=std::max(maximumError,double(std::fabs(placed[i]-expected)));
			if(!NearPose(placed[i],expected))std::cerr<<"Retail case "<<phase<<" "<<observation.bone<<" element"<<i<<" got"<<placed[i]<<" expected"<<expected<<"\n";
			CHECK(NearPose(placed[i],expected));
		}
	}
	CHECK(store.Clean(&instance)&&!instance);
	std::cout<<"90 retail asset-memory pose queries passed; max error "<<maximumError<<"\n";
}

#ifdef FNTECH3_JKA_POSE_EMBEDDED
#define main AcademyUnblendedPoseFixtureMain
#endif
static void CheckAcademyAngleBlendMetadata() {
	TestFiles files;files.files["models/players/test/model.glm"]=Mesh("models/players/test/skeleton",3);
	files.files["models/players/test/skeleton.gla"]=PoseSkeleton();
	ghoul2::Store store(files,true,true,true);void *instance=nullptr;LoadError error;
	CHECK(store.AddModel(&instance,"models/players/test/model.glm",error)==0);
	const int bolt=store.AddBolt(instance,0,"child");CHECK(bolt==0);
	const float bend[]{10,20,30};
	for(int mode:{1,2}) {
		CHECK(store.SetBoneAngles(instance,0,"child",bend,mode,1,3,2,0,100));
		float expected[12];CHECK(store.BoltMatrix(instance,0,bolt,100,expected));
		for(int duration:{-80,1,80,INT_MAX}) {
			CHECK(store.SetBoneAngles(instance,0,"child",bend,mode,1,3,2,duration,100));
			for(int now:{0,99,100,140,180,1000}) {
				float actual[12];CHECK(store.BoltMatrix(instance,0,bolt,now,actual));
				for(int n=0;n<12;++n)CHECK(NearPose(actual[n],expected[n]));
			}
		}
	}
	// REPLACE has a different timed path; a nonzero duration remains refused.
	CHECK(store.SetBoneAngles(instance,0,"child",bend,4,1,3,2,80,100));
	float untouched[12];std::fill(untouched,untouched+12,-777.0f);
	CHECK(!store.BoltMatrix(instance,0,bolt,140,untouched));
	for(float value:untouched)CHECK(value==-777.0f);
	CHECK(store.Clean(&instance)&&!instance);
}
int main(int argc,char **argv) {
	CheckHierarchyAndInheritance();CheckPlacementAndStore();CheckAcademyAngleBlendMetadata();
	if(argc==2&&std::strcmp(argv[1],"--retail-memory")==0)CheckRetailMemoryInput();
	std::cout<<"JKA/JK2 Ghoul2 shared pose kernel tests passed\n";
	return 0;
}

#ifdef FNTECH3_JKA_POSE_EMBEDDED
#undef main
#endif
