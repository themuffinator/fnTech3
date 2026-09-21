"""Execute SP save publication with real temporary files and controlled I/O failures."""
# Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args
from spawn_view_angle_source_tests import function_body

ROOT = Path(__file__).resolve().parents[1]

PRELUDE = r'''
#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "code/compat/stvef_sp_savegame.hpp"
using byte=unsigned char;using fileHandle_t=int;using qboolean=bool;
constexpr bool qtrue=true,qfalse=false;constexpr int ERR_FATAL=0,MAX_FILE_HANDLES=8,MAX_QPATH=64;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct cvar_t{float value=0;int integer=1;};
struct fileHandleData_t{bool zipFile=false,handleSync=false;int owner=0;struct{struct{FILE *o=nullptr;}file;}handleFiles;};
static fileHandleData_t fsh[MAX_FILE_HANDLES];static bool fs_searchpaths=true;
static bool failOpen=false,failRename=false,failClose=false,streamError=false,allowed=true;
static int failCall=0,writeCalls=0,closeCalls=0,replaces=0,gameCalls=0,gameError=0,ioAfterFailure=0;
static bool writeFailed=false;static std::string messages;
void Com_Error(int,const char*,...){throw 7;}
void Com_Memset(void *p,int c,size_t n){std::memset(p,c,n);}
void Com_DPrintf(const char*,...){}
void Com_Printf(const char *format,...){char text[1024];va_list ap;va_start(ap,format);vsnprintf(text,sizeof(text),format,ap);va_end(ap);messages+=text;}
void Com_sprintf(char *out,int size,const char *format,...){va_list ap;va_start(ap,format);vsnprintf(out,size,format,ap);va_end(ap);}
void Q_strncpyz(char *out,const char *in,int size){std::snprintf(out,size,"%s",in);}
int Com_BlockChecksum(const void *data,int length){CHECK(length>=0&&length<10000);uint32_t v=123;auto p=static_cast<const byte*>(data);for(int i=0;i<length;++i)v=v*31+p[i];return int(v);}
bool FS_WriteRequestIsValid(const void *p,int n,int h){return h>0&&h<MAX_FILE_HANDLES&&n>=0&&(!n||p);}
static size_t ControlledWrite(const void *data,size_t unit,size_t count,FILE *file){
 CHECK(unit==1);++writeCalls;
 if(writeFailed){++ioAfterFailure;return 0;}
 if(failCall&&writeCalls==failCall){writeFailed=true;return std::fwrite(data,1,count/2,file);}
 return std::fwrite(data,unit,count,file);
}
static int ControlledClose(FILE *file){++closeCalls;int result=std::fclose(file);return failClose?EOF:result;}
static int ControlledError(FILE *file){return streamError?1:std::ferror(file);}
#define fwrite ControlledWrite
#define fclose ControlledClose
#define ferror ControlledError
@WRITE@
@CHECKED_CLOSE@
#undef fwrite
#undef fclose
#undef ferror
void FS_FCloseFile(int h){CHECK(h>0&&h<MAX_FILE_HANDLES&&fsh[h].handleFiles.file.o);ControlledClose(fsh[h].handleFiles.file.o);fsh[h]={};}
fileHandle_t FS_FOpenFileWrite(const char *name){CHECK(!fsh[1].handleFiles.file.o);if(failOpen)return 0;
 std::filesystem::create_directories(std::filesystem::path(name).parent_path());fsh[1].handleFiles.file.o=std::fopen(name,"wb");CHECK(fsh[1].handleFiles.file.o);fsh[1].owner=4;return 1;}
void FS_HomeRemove(const char *path){CHECK(!fsh[1].handleFiles.file.o);std::error_code e;std::filesystem::remove(path,e);}
bool FS_HomeReplaceFile(const char *from,const char *to){CHECK(!fsh[1].handleFiles.file.o);++replaces;if(failRename)return false;
 // Owned fixture replacement; the engine's platform replacement is checked separately.
 std::filesystem::create_directories(std::filesystem::path(to).parent_path());std::filesystem::remove(to);std::filesystem::rename(from,to);return true;}
@GLOBALS@
@SAVE_PATH@
@WIPE@
@CLOSE@
@CREATE@
@APPEND@
@IMPORT@
static struct {int time=5300,timeResidual=17,state=2;}sv;
static void Part(uint32_t id){const char text[]="owned fixture payload";SV_STVEFSPAppendChunk(id,text,sizeof(text));}
static bool SV_STVEFSPGameAllowedToSaveHere(bool){return allowed;}
static const char *SV_STVEFSPMapName(){return "tutorial";}
static void SV_STVEFSPWriteComment(bool,const char*){Part(savegame::Comment);}
static void SV_STVEFSPWriteScreenshot(bool,const char*){Part(savegame::Screenshot);}
static void SV_STVEFSPWriteCvars(){Part(savegame::CvarCount);Part(savegame::CvarName);Part(savegame::CvarValue);}
static void SV_STVEFSPWriteGame(bool){Part(savegame::Autosave);}
static void SV_STVEFSPWritePortals(){Part(savegame::Portals);}
static void SV_STVEFSPWriteConfigstrings(){Part(savegame::ConfigstringCount);Part(savegame::ConfigstringIndex);Part(savegame::ConfigstringData);}
qboolean SV_STVEFSPWriteSavegame(const char*,qboolean,qboolean*);
static void GameWrite(int){++gameCalls;
 if(gameError==1)SV_STVEFSPAppendToSave(savegame::Id("BAD!"),nullptr,3);
 if(gameError==2)SV_STVEFSPAppendToSave(savegame::Id("BAD!"),const_cast<char*>("x"),-1);
 if(gameError==3)SV_STVEFSPAppendToSave(savegame::Id("BAD!"),const_cast<char*>("x"),INT_MAX);
 if(gameError==4)CHECK(!SV_STVEFSPWriteSavegame("nested",false,nullptr));
 const char text[]="game-owned fixture";
 SV_STVEFSPAppendToSave(savegame::Id("MOD1"),const_cast<char*>(text),sizeof(text));
 SV_STVEFSPAppendToSave(savegame::Id("MOD2"),nullptr,0);
}
struct Game{void (*WriteLevel)(int);};static Game game{GameWrite},*stvefSpGame=&game;
@MOVE@
@SAVE@
static int stvefSpLoadTransition=0,precache=0,unprecache=0,endSave=0,hubReads=0,mapStarts=0;
static std::string target,cmd="save",argument="slot";
static cvar_t running;static cvar_t *com_sv_running=&running;
constexpr int SS_GAME=2;
int Cmd_Argc(){return 2;}
const char *Cmd_Argv(int n){return n==0?cmd.c_str():(n==1?argument.c_str():"entry");}
int Cvar_VariableIntegerValue(const char*){return 0;}
const char *Cvar_VariableString(const char*){return "tutorial";}
void Cvar_Set(const char *name,const char *value){if(std::string(name)=="spawntarget")target=value;}
int Q_stricmp(const char *a,const char *b){return std::strcmp(a,b);}
void CL_STVEFSPPrecacheScreenshot(){++precache;}
void CL_STVEFSPUnprecacheScreenshot(){++unprecache;}
void SV_STVEFSPPlayerEndOfLevelSave(){++endSave;}
bool SV_STVEFSPReadHubSave(const char*){++hubReads;return false;}
bool SV_STVEFSPStartMap(const char*){++mapStarts;return true;}
int SV_STVEFSPPlayerHealth(){return 100;}
static char stvefSpSaveComment[savegame::CommentBytes];
@COMMAND@
@TRANSITION@
static std::vector<byte> Bytes(const char *path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
static const std::vector<byte> previous={'o','l','d',' ','s','a','v','e'};
static void PutOld(const char *name){std::filesystem::create_directories(std::filesystem::path(name).parent_path());std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(previous.data()),previous.size());}
static void Reset(){CHECK(!fsh[1].handleFiles.file.o);failOpen=failRename=failClose=streamError=writeFailed=false;allowed=true;
 failCall=writeCalls=closeCalls=replaces=gameCalls=gameError=ioAfterFailure=0;messages.clear();stvefSpGame=&game;
 stvefSpLoadTransition=precache=unprecache=endSave=hubReads=mapStarts=0;target.clear();cmd="save";argument="slot";
 std::filesystem::remove("saves/current.sav");PutOld("saves/slot.sav");}
static void Preserved(){CHECK(Bytes("saves/slot.sav")==previous&&!replaces&&!fsh[1].handleFiles.file.o&&!stvefSpSaveWriting);CHECK(!std::filesystem::exists("saves/current.sav"));}
static void Valid(const std::vector<byte>& bytes){size_t offset=0;int chunks=0;while(offset<bytes.size()){
 savegame::Chunk chunk{};auto id=savegame::GetWord(bytes.data()+offset);auto result=savegame::ReadChunk(bytes.data(),bytes.size(),offset,id,0,chunk,[](const byte *p,uint32_t n){return uint32_t(Com_BlockChecksum(p,int(n)));});
 CHECK(result==savegame::ReadResult::Ok);offset=chunk.next;++chunks;}CHECK(offset==bytes.size()&&chunks>8);}
'''

CHECKS = r'''
int main(){
 int cases=0;
 for(bool autosave:{false,true}){
  Reset();CHECK(SV_STVEFSPWriteSavegame("slot",autosave));Valid(Bytes("saves/slot.sav"));int calls=writeCalls;CHECK(calls>10&&closeCalls==1&&replaces==1);++cases;
  // Fail every write boundary: engine headers/payloads/trailers and game imports.
  for(int boundary=1;boundary<=calls;++boundary){Reset();failCall=boundary;
   const bool saved=SV_STVEFSPWriteSavegame("slot",autosave);
   if(saved)std::fprintf(stderr,"failed write %d: reported success; original preserved=%d; replacements=%d\n",boundary,int(Bytes("saves/slot.sav")==previous),replaces);
   CHECK(!saved);Preserved();CHECK(closeCalls==1);
   CHECK(ioAfterFailure<=2);++cases;
   // A failed save does not poison the next independent save.
   Reset();CHECK(SV_STVEFSPWriteSavegame("slot",autosave));Valid(Bytes("saves/slot.sav"));
  }
  for(int failure=0;failure<6;++failure){Reset();
   if(failure==0)failOpen=true;if(failure==1)failClose=true;if(failure==2)streamError=true;
   if(failure>=3)gameError=failure-2;
   CHECK(!SV_STVEFSPWriteSavegame("slot",autosave));Preserved();++cases;
  }
  Reset();failRename=true;CHECK(!SV_STVEFSPWriteSavegame("slot",autosave));
  CHECK(Bytes("saves/slot.sav")==previous&&replaces==1&&closeCalls==1&&!stvefSpSaveWriting);Valid(Bytes("saves/current.sav"));++cases;
  Reset();stvefSpGame=nullptr;CHECK(!SV_STVEFSPWriteSavegame("slot",autosave));Preserved();++cases;
 }
 Reset();allowed=false;CHECK(!SV_STVEFSPWriteSavegame("slot",false));Preserved();CHECK(!gameCalls&&!writeCalls&&!closeCalls);++cases;
 Reset();allowed=false;CHECK(SV_STVEFSPWriteSavegame("slot",true));++cases;
 // The dry-run callback accepts chunks without any file; real saves still write.
 Reset();cvar_t test;test.value=1;stvefSpTestSave=&test;
 CHECK(SV_STVEFSPAppendToSave(savegame::Id("TEST"),nullptr,99));CHECK(!writeCalls);
 CHECK(SV_STVEFSPWriteSavegame("slot",false)&&writeCalls);stvefSpTestSave=nullptr;++cases;
 // An explicit save must not announce success after a failed transaction.
 Reset();failCall=1;SV_STVEFSPSave_f();Preserved();CHECK(messages.find("Done.")==std::string::npos);++cases;
 Reset();SV_STVEFSPSave_f();CHECK(messages.find("Done.")!=std::string::npos);++cases;
 // A failed hub save leaves the current level running rather than losing it.
 Reset();PutOld("saves/hub/tutorial.sav");failCall=1;argument="borg2";
 SV_STVEFSPLoadTransition_f();CHECK(Bytes("saves/hub/tutorial.sav")==previous);
 CHECK(!hubReads&&!mapStarts&&!stvefSpLoadTransition&&target.empty()&&unprecache==1);++cases;
 Reset();argument="borg2";SV_STVEFSPLoadTransition_f();CHECK(hubReads==1&&mapStarts==1&&target=="entry");++cases;
 Reset();argument="borg2";allowed=false;SV_STVEFSPLoadTransition_f();CHECK(!writeCalls&&!replaces&&hubReads==1&&mapStarts==1);++cases;
 Reset();gameError=4;CHECK(SV_STVEFSPWriteSavegame("slot",false));Valid(Bytes("saves/slot.sav"));CHECK(gameCalls==1&&replaces==1);++cases;
 // Close validation must leave unrelated archive handles and invalid slots alone.
 @CLOSE_CHECKS@
 std::printf("%d primary save scenarios plus successful retries; every partial-write boundary, close/error/rename, recovery and command checks passed\n",cases);
}
'''

class STVEFSPSaveWriteTests(unittest.TestCase):
    def test_save_failures_preserve_existing_files_and_commands(self):
        save = (ROOT / 'code/server/server_stvef_sp_save.hpp').read_text()
        files = (ROOT / 'code/qcommon/files.c').read_text()
        source = PRELUDE
        a = save.index('namespace savegame =')
        b = save.index('/* A save being read', a)
        # Actual transaction globals, plus actual complete append/commit functions.
        pieces = {'GLOBALS': save[a:b], 'WRITE': function_body(files, 'FS_Write'),
                  'CHECKED_CLOSE': function_body(files, 'FS_FCloseFileWrite')}
        for key, name in [('SAVE_PATH','SV_STVEFSPSavePath'),('WIPE','SV_STVEFSPWipeSave'),
                          ('CLOSE','SV_STVEFSPCloseWrite'),('CREATE','SV_STVEFSPCreateSave'),
                          ('APPEND','SV_STVEFSPAppendChunk'),('IMPORT','SV_STVEFSPAppendToSave'),
                          ('MOVE','SV_STVEFSPMoveSave'),('SAVE','SV_STVEFSPWriteSavegame'),
                          ('COMMAND','SV_STVEFSPSave_f'),('TRANSITION','SV_STVEFSPLoadTransition_f')]:
            pieces[key] = function_body(save,name)
            self.assertTrue(pieces[key],name)
        for key,value in pieces.items(): source=source.replace('@'+key+'@',value)
        close_checks = r'''
 for(int handle:{-1,0,MAX_FILE_HANDLES,INT_MAX})CHECK(!FS_FCloseFileWrite(handle));
 CHECK(!FS_FCloseFileWrite(2));
 Reset();int h=FS_FOpenFileWrite("saves/current.sav");fsh[h].zipFile=true;CHECK(!FS_FCloseFileWrite(h)&&closeCalls==0&&fsh[h].handleFiles.file.o);
 fsh[h].zipFile=false;CHECK(FS_FCloseFileWrite(h)&&closeCalls==1&&!fsh[h].handleFiles.file.o&&fsh[h].owner==0);
 Reset();h=FS_FOpenFileWrite("saves/current.sav");fs_searchpaths=false;bool threw=false;
 try{FS_FCloseFileWrite(h);}catch(int){threw=true;}CHECK(threw&&!closeCalls&&fsh[h].handleFiles.file.o);
 fs_searchpaths=true;CHECK(FS_FCloseFileWrite(h));
 ''' if pieces['CHECKED_CLOSE'] else ''
        source += CHECKS.replace('@CLOSE_CHECKS@',close_checks)
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++17 compiler unavailable')
        folder_root=ROOT/'.tmp';folder_root.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-save-write-',dir=folder_root) as folder:
            folder=Path(folder);cpp=folder/'probe.cpp';exe=folder/('probe.exe' if os.name=='nt' else 'probe');cpp.write_text(source)
            sanitize=[] if os.name=='nt' else ['-fsanitize=address,undefined','-fno-sanitize-recover=all']
            result=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17','-O1',*sanitize,'-I',str(ROOT),str(cpp),'-o',str(exe)],cwd=folder,capture_output=True,text=True,timeout=120)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            options={'creationflags':subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
            result=subprocess.run([str(exe)],cwd=folder,capture_output=True,text=True,timeout=60,**options)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            print(result.stdout.strip())

if __name__=='__main__':unittest.main()
