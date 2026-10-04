/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#define FNTECH3_JKA_POSE_EMBEDDED
#include "jka_pose_tests.cpp"
#undef FNTECH3_JKA_POSE_EMBEDDED
#include "jka_animation_blend_retail_cases.hpp"

static void CheckActiveAnimationTransitions() {
	TestFiles files;files.files["models/players/test/model.glm"]=Mesh("models/players/test/skeleton",3);
	files.files["models/players/test/skeleton.gla"]=PoseSkeleton();
	ghoul2::Store enabled(files,false,true),disabled(files);void *handle=nullptr,*baseline=nullptr;LoadError error;
	CHECK(enabled.AddModel(&handle,"models/players/test/model.glm",error)==0);
	CHECK(disabled.AddModel(&baseline,"models/players/test/model.glm",error)==0);
	const auto modelAt=[&](){return enabled.Resolve(handle)->At(0);};
	pose::Palette palette;
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",0,3,0x58,.5f,0,-1,0));
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",1,3,0xc8,1,75,-1,100));
	const auto *playing=enabled.BoneAnimation(handle,0,"model_root");
	CHECK(playing->previousKnown&&playing->blendStart==75&&playing->previous.current==0
		&&playing->previous.next==1&&NearPose(playing->previous.lerp,.25f));
	// The old frame is frozen at .25 by retail's reversed fraction weights.
	// Three parent-relative offsets accumulate before the bind matrices.
	CHECK(enabled.DeformationPalette(*modelAt(),75,palette));
	CHECK(NearPose(palette.deformation[2][7],3.75f)&&NearPose(palette.deformation[2][11],-.75f));
	CHECK(enabled.DeformationPalette(*modelAt(),125,palette));
	CHECK(NearPose(palette.deformation[2][7],6.375f)&&NearPose(palette.deformation[2][11],-3.375f));
	CHECK(enabled.DeformationPalette(*modelAt(),175,palette));
	CHECK(NearPose(palette.deformation[2][7],9)&&NearPose(palette.deformation[2][11],-6));
	// A seeded new clip changes its clock, never the transition's clock.
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",1,3,0xc8,1,75,2,200));
	playing=enabled.BoneAnimation(handle,0,"model_root");
	CHECK(playing->startTime==25&&playing->blendStart==75&&NearPose(playing->previous.lerp,.25f));
	CHECK(enabled.DeformationPalette(*modelAt(),75,palette));CHECK(NearPose(palette.deformation[2][7],3.75f));
	// Duplicate/copy keep an independent frozen transition; cleanup cannot
	// leave a module identity able to reach the old records.
	void *copy=nullptr;CHECK(enabled.Duplicate(handle,&copy)&&copy!=handle);
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",0,3,0xc8,0,125,-1,100));
	CHECK(enabled.DeformationPalette(*modelAt(),125,palette));CHECK(NearPose(palette.deformation[2][7],9));
	CHECK(enabled.DeformationPalette(*modelAt(),175,palette));CHECK(NearPose(palette.deformation[2][7],6));
	pose::Palette copied;CHECK(enabled.DeformationPalette(*enabled.Resolve(copy)->At(0),175,copied));
	CHECK(NearPose(copied.deformation[2][7],6.375f));
	CHECK(enabled.CopyModel(copy,0,handle,0));
	CHECK(enabled.DeformationPalette(*modelAt(),175,palette)&&palette.deformation==copied.deformation);
	// An explicit child clip replaces inherited timing and inherited blending.
	CHECK(enabled.SetBoneAnim(handle,0,"child",0,3,0x48,0,75,-1,0));
	CHECK(enabled.DeformationPalette(*modelAt(),175,palette));CHECK(NearPose(palette.deformation[2][7],4.125f));
	// A transition requested at time0 after a plain active clip preserves the
	// setter's cleared prior frame0, rather than taking a new snapshot.
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",1,3,0x58,1,0,-1,0));
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",2,3,0xc8,0,0,-1,100));
	playing=enabled.BoneAnimation(handle,0,"model_root");CHECK(playing->previousKnown
		&&playing->previous.current==0&&playing->previous.next==0);
	CHECK(enabled.DeformationPalette(*modelAt(),0,palette));CHECK(NearPose(palette.deformation[0][7],0));
	// Empty and negative durations cannot divide by zero or activate a blend.
	for(int duration:{0,-1,INT_MIN}) {
		CHECK(enabled.SetBoneAnim(handle,0,"model_root",2,3,0xc8,0,0,-1,duration));
		CHECK(enabled.DeformationPalette(*modelAt(),0,palette));CHECK(NearPose(palette.deformation[0][7],2));
	}
	// Invalid prior timing may be stored as metadata, but an active transition
	// cannot turn it into a rendered pose or overwrite an earlier palette.
	for(int flags:{0x108,0x20}) {
		CHECK(enabled.SetBoneAnim(handle,0,"model_root",0,3,flags,1,10,-1,0));
		CHECK(enabled.SetBoneAnim(handle,0,"model_root",1,3,0xc8,1,75,-1,100));
		if(flags==0x20)CHECK(!(enabled.BoneAnimation(handle,0,"model_root")->flags&0x80));
		else {const auto prior=palette.deformation;CHECK(!enabled.DeformationPalette(*modelAt(),75,palette));CHECK(palette.deformation==prior);}
	}
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",0,3,0x48,-1,10,-1,0));
	CHECK(enabled.SetBoneAnim(handle,0,"model_root",1,3,0xc8,1,75,-1,100));
	const auto prior=palette.deformation;CHECK(!enabled.DeformationPalette(*modelAt(),75,palette));CHECK(palette.deformation==prior);
	// Default-off stores preserve the old refusal for shared UI/server/SP/JK2.
	CHECK(disabled.SetBoneAnim(baseline,0,"model_root",0,3,0x58,1,0,-1,0));
	CHECK(disabled.SetBoneAnim(baseline,0,"model_root",1,3,0xc8,1,75,-1,100));
	CHECK(!disabled.DeformationPalette(*disabled.Resolve(baseline)->At(0),100,palette));
	CHECK(enabled.Clean(&copy)&&!copy&&enabled.Clean(&handle)&&!handle&&disabled.Clean(&baseline)&&!baseline);
}

static void CheckBlendBounds() {
	const Bytes bytes=PoseSkeleton();model::Skeleton skeleton;
	CHECK(model::ParseSkeleton(bytes.data(),bytes.size(),skeleton)==model::Error::None);
	pose::Palette out;CHECK(pose::Evaluate(bytes.data(),bytes.size(),skeleton,{}, {},out));
	const auto preserved=out.deformation;
	pose::Animation valid{0,{1,2,.5f},{0,1,.25f},.5f,true};
	for(int kind=0;kind<6;++kind) {
		auto bad=valid;
		if(kind==0)bad.previous.current=-1;
		if(kind==1)bad.previous.next=3;
		if(kind==2)bad.previous.lerp=-.1f;
		if(kind==3)bad.previous.lerp=1.1f;
		if(kind==4)bad.transition=-.1f;
		if(kind==5)bad.transition=1.1f;
		CHECK(!pose::Evaluate(bytes.data(),bytes.size(),skeleton,{bad},{},out)&&out.deformation==preserved);
	}
	for(std::uint32_t bits:{0x7f800000u,0xff800000u,0x7fc00001u}) {
		float scalar;std::memcpy(&scalar,&bits,4);
		auto bad=valid;bad.previous.lerp=scalar;
		CHECK(!pose::Evaluate(bytes.data(),bytes.size(),skeleton,{bad},{},out)&&out.deformation==preserved);
		bad=valid;bad.transition=scalar;
		CHECK(!pose::Evaluate(bytes.data(),bytes.size(),skeleton,{bad},{},out)&&out.deformation==preserved);
	}
	for(float progress:{0.0f,.5f,1.0f}) {
		auto at=valid;at.transition=progress;
		CHECK(pose::Evaluate(bytes.data(),bytes.size(),skeleton,{at},{},out));
		CHECK(NearPose(out.deformation[2][7],3+3*(.25f*(1-progress)+1.5f*progress)));
	}
}

static void CheckRetailBlendMemory() {
#ifdef _WIN32
	_setmode(_fileno(stdin),_O_BINARY);
#endif
	TestFiles files;
	for(int which=0;which<2;++which) {
		std::uint32_t sizes[2];std::cin.read(reinterpret_cast<char*>(sizes),sizeof(sizes));CHECK(std::cin.good());
		CHECK(sizes[0]>0&&sizes[0]<256&&sizes[1]>0&&sizes[1]<=ghoul2::MaxModelBytes);
		std::string name(sizes[0],'\0');Bytes bytes(sizes[1]);
		std::cin.read(name.data(),sizes[0]);std::cin.read(reinterpret_cast<char*>(bytes.data()),sizes[1]);CHECK(std::cin.good());
		CHECK(files.files.emplace(name,std::move(bytes)).second);
	}
	ghoul2::Store store(files,false,true);std::array<void*,8> owned{};LoadError error;
	for(auto &handle:owned)CHECK(store.AddModel(&handle,"models/players/kyle/model.glm",error)==0);
	std::size_t command=0;double maximum=0;
	for(const auto &query:RetailBlendQueries) {
		while(command<RetailBlendCommands.size()&&RetailBlendCommands[command].time<=query.suppliedTime) {
			const auto &play=RetailBlendCommands[command++];
			CHECK(store.SetBoneAnim(owned[play.model],0,play.bone,play.first,play.end,play.flags,play.speed,play.time,-1,play.duration));
		}
		const int bolt=store.AddBolt(owned[query.model],0,query.bone);CHECK(bolt>=0);
		float raw[12],placed[12];const float zero[]{0,0,0},unit[]{1,1,1};
		CHECK(store.BoltMatrix(owned[query.model],0,bolt,query.poseTime,raw)&&pose::PlaceBolt(raw,zero,zero,unit,false,placed));
		for(int n=0;n<12;++n) {
			float expected;std::memcpy(&expected,&query.words[n],4);
			maximum=std::max(maximum,double(std::fabs(expected-placed[n])));
			if(!NearPose(expected,placed[n]))std::cerr<<"Blend query model"<<query.model<<" "<<query.bone<<" time"<<query.poseTime<<" element"<<n<<" actual"<<placed[n]<<" expected"<<expected<<"\n";
			CHECK(NearPose(expected,placed[n]));
		}
	}
	for(auto &handle:owned)CHECK(store.Clean(&handle)&&!handle);
	std::cout<<"Retail active blend memory queries="<<RetailBlendQueries.size()<<" maximumError="<<maximum<<"\n";
}
int main(int argc,char **argv) {
	if(argc==2&&std::string(argv[1])=="--retail-blend") {CheckRetailBlendMemory();return 0;}
	CheckActiveAnimationTransitions();CheckBlendBounds();
	std::cout<<"Academy active animation blend state and numeric guards passed\n";
}
