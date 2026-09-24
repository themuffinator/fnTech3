/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
/* Optional x86 host for the real engine game-data adapter and original game
 * DLL. Reuse the hash check, owned cvars/files and strict unused imports. */
#define FNTECH3_IHV_REFERENCE_NO_MAIN
#include "q3_ihv_reference_game_probe.cpp"
#include "../code/server/server_game_data.h"

namespace {
using namespace fntech3::server;
std::map<int,std::string> configstrings;
std::vector<std::string> commands;
fntech3_q3_ihv_game_export_t *activeGame;
fntech3_q3_ihv_entity_t *connectingEntity;
char connectingInfo[1024]{};
char *connectionResult;
void __cdecl ConnectCallback() { connectionResult=activeGame->ClientConnect(connectingEntity,connectingInfo); }
bool ValidateClient(void *context,const void *record,std::int32_t client,std::size_t bytes) {
	return memory.OwnsRecord(record,client,*static_cast<int *>(context),1464,bytes);
}
void __cdecl CaptureCommand(fntech3_q3_ihv_entity_t *entity,const char *fmt,...) {
	++calls[17];if(entity)Fail("Unexpected addressed client command");
	char text[4096]{};va_list args;va_start(args,fmt);
	const int count=std::vsnprintf(text,sizeof(text),fmt,args);va_end(args);
	if(count<0||count>=static_cast<int>(sizeof(text)))Fail("Command overflow");
	commands.emplace_back(text);
}
void __cdecl SetString(int index,const char *value) {
	++calls[18];if(index<0||index>=1024||!value||std::strlen(value)>=1024)Fail("Configstring bounds");
	configstrings[index]=value;
}
char *__cdecl GetString(int index) {
	++calls[19];if(index<0||index>=1024)Fail("Configstring bounds");return configstrings[index].data();
}
}
int main(int argc,char **argv) {
	if(argc!=3)return 77;
	HMODULE module=nullptr;ServerGameDataView view;
	try {
		Verify(argv[1]);assetRoot=fs::canonical(argv[2]);Check(fs::is_directory(assetRoot),"Asset root");
		const auto dll=fs::canonical(argv[1]);
		module=LoadLibraryExW(dll.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
		Check(module!=nullptr,"Load native game DLL");
		auto getter=reinterpret_cast<fntech3_q3_ihv_GetGameAPI_t>(GetProcAddress(module,"GetGameAPI"));Check(getter!=nullptr,"GetGameAPI");
		auto imports=DefaultImports();imports.SendServerCommand=CaptureCommand;
		imports.SetConfigstring=SetString;imports.GetConfigstring=GetString;
		activeGame=getter(&imports);Check(activeGame&&activeGame->apiVersion==4&&activeGame->entityStride==556,"Game contract");
		Check(CallModule(activeGame->Init),errorText);
		int clients=static_cast<int>(variables.at("maxclients")->mirror.Record()->value);
		Check(clients==8&&activeGame->entityCount==9&&activeGame->entityCapacity==1024,"Original Init counts");
		NativeGameDataBinding binding;
		binding.entityBase=activeGame->entities;binding.entitySpanBytes=memory.AllocationSize(activeGame->entities);
		binding.entityCount=activeGame->entityCount;binding.entityCapacity=activeGame->entityCapacity;
		binding.entityStride=activeGame->entityStride;binding.clientCount=clients;
		binding.validateClientRecord=ValidateClient;binding.clientValidationContext=&clients;
		Check(view.BindNative(COM_GAME_QUAKE3_IHV,binding)==GameDataBindError::None,"Bind original native allocation");
		for(int index=0;index<clients;++index)Check(!view.ClientPointer(index),"Unpublished Init client pointer");
		GameEntityRef first;Check(view.EntityRefAt(1,first),"First native entity");
		connectingEntity=static_cast<fntech3_q3_ihv_entity_t *>(view.EntityPointer(first));
		CvarSet("password","secret");std::snprintf(connectingInfo,sizeof(connectingInfo),"\\name\\Probe0\\password\\wrong");
		Check(CallModule(ConnectCallback),errorText);
		Check(connectionResult&&!std::strcmp(connectionResult,"Invalid password")&&!view.ClientPointer(0)
			&&configstrings.empty()&&commands.empty(),"Password rejection");
		std::cout<<"{\"scope\":\"engine native game-data adapter with original Init/ClientConnect/Shutdown; no level or network\",\"clients\":[";
		for(int slot:{0,7}) {
			GameEntityRef reference;Check(view.EntityRefAt(slot+1,reference),"Native client entity slot");
			connectingEntity=static_cast<fntech3_q3_ihv_entity_t *>(view.EntityPointer(reference));
			std::snprintf(connectingInfo,sizeof(connectingInfo),"\\name\\Probe%d\\password\\secret\\model\\xian\\skin\\default",slot);
			Check(CallModule(ConnectCallback),errorText);Check(!connectionResult,"Client refused");
			auto *client=static_cast<unsigned char *>(view.ClientPointer(slot));Check(client!=nullptr,"Validate original client allocation");
			fntech3_q3_ihv_playerState_t player{};Check(view.ReadPlayerState(slot,player),"Read original player state");
			Check(!std::memcmp(player.bytes,client,276),"Exact player transfer");
			std::array<unsigned char,1464> before;std::memcpy(before.data(),client,before.size());
			const int ping=37+slot;Check(view.SetPlayerPing(slot,ping),"Set native ping");int readPing=-1;
			Check(view.GetPlayerPing(slot,readPing)&&readPing==ping,"Read native ping");
			std::memcpy(before.data()+272,&ping,4);Check(!std::memcmp(before.data(),client,before.size()),"Ping preserved private client bytes");
			fntech3_q3_ihv_entityState_t state{};entityState_t shared{};
			Check(view.ReadEntityState(reference,state)&&view.ReadEntityState(reference,shared),"Read original entity");
			std::array<unsigned char,556> entityBefore;std::memcpy(entityBefore.data(),connectingEntity,556);
			Check(view.WriteEntityState(reference,shared)&&!std::memcmp(entityBefore.data(),connectingEntity,556),"Native entity round trip");
			Check(configstrings.count(544+slot)==1,"Native configstring slot");
			if(slot)std::cout<<',';std::cout<<"{\"slot\":"<<slot<<",\"entity\":"<<slot+1<<",\"ping\":"<<readPing<<",\"configstring\":";
			Quote(configstrings.at(544+slot));std::cout<<'}';
		}
		view.Invalidate();Check(CallModule(activeGame->Shutdown),errorText);
		Check(memory.Count()==0&&memory.Bytes()==0,"Shutdown ownership");
		FreeLibrary(module);module=nullptr;
		std::cout<<"],\"entity_bytes\":164,\"player_bytes\":276,\"remaining_allocations\":0,\"result\":\"PASS\"}\n";return 0;
	}catch(const std::exception &error) {
		view.Invalidate();if(module)FreeLibrary(module);memory.Clear();std::fprintf(stderr,"%s\n",error.what());return 1;
	}
}
