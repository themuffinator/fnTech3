"""Exercise the actual Academy music facade/cache with authored codec IO."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import engine_fixture_args
from rtcw_music_fixture import function
ROOT=Path(__file__).resolve().parents[1]
def fixture():
    codec=(ROOT/'code/client/audio/codecs/snd_codec.cpp').read_text()
    facade=(ROOT/'code/client/audio/snd_main.cpp').read_text()
    owners=codec[codec.index('fntech3::audio::jka::music::Cache<snd_stream_t>'):codec.index('bool S_STVEFGermanVoice(')]
    code=r'''
    #include "code/qcommon/q_shared.h"
    #include "code/qcommon/game_profile.h"
    #include "code/client/audio/shared/JKAMusicPreload.h"
    #include <cassert>
    #include <csetjmp>
    #include <cstdlib>
    #include <map>
    #include <string>
    #include <new>
    struct snd_info_t {};
    struct snd_stream_t;
    struct snd_codec_t {void (*close)(snd_stream_t *);};
    struct snd_stream_t {snd_codec_t *codec;int file;snd_info_t info;int length,pos;void *ptr;};
    static comGameId_t profile=COM_GAME_JKA_MP;
    int Com_GameIsJKAMP(){return profile==COM_GAME_JKA_MP;}
    static std::map<int,std::string> files;
    static std::map<std::string,int> lengths{{"music/a.mp3",10},{"music/b.mp3",20},{"music/large.mp3",64*1024*1024+1}};
    static int nextFile=1,opens,closedFiles,decoders,decoderCloses,warnings,stopCalls,playCalls;
    static int dropOpenAt;
    static bool jumpRecovery,dropClose;
    static std::jmp_buf recovery;
    struct Drop{};
    extern "C" void S_ClearJKAMusicPreload();
    void QDECL Com_Error(errorParm_t,const char *,...){
     S_ClearJKAMusicPreload();if(jumpRecovery)std::longjmp(recovery,1);throw Drop{};
    }
    void QDECL Com_DPrintf(const char *format,...){if(std::strstr(format,"64 MiB"))++warnings;}
    static void CloseFile(int handle){assert(files.erase(handle)==1);++closedFiles;}
    struct ScopedFileHandle {
     int handle=0;~ScopedFileHandle(){if(handle)CloseFile(handle);}
     explicit operator bool()const{return handle!=0;}
     int release(){int value=handle;handle=0;return value;}
    };
    static int OpenFileRead(const char *name,ScopedFileHandle &file,qboolean unique){
     assert(unique);++opens;auto found=lengths.find(name);if(found==lengths.end())return -1;
     file.handle=nextFile++;files[file.handle]=name;return found->second;
    }
    struct ScopedZoneMemory {
     void *pointer;~ScopedZoneMemory(){std::free(pointer);}void *get(){return pointer;}
     template<class T>T *as(){return static_cast<T *>(pointer);}
     void *release(){void *value=pointer;pointer=nullptr;return value;}
    };
    static ScopedZoneMemory AllocateZoneMemory(std::size_t bytes,const char *,const char *,int){return {std::calloc(1,bytes)};}
    static void Z_Free(void *memory){std::free(memory);}
    static void *S_CodecGetSound(const char *,snd_info_t *,bool quiet=false);
    extern "C" void S_CodecCloseStream(snd_stream_t *);
    ''' + owners + '\n' + function(codec,'extern "C" void S_ClearJKAMusicPreload(')+'\n'+ \
     function(codec,'extern "C" void S_CodecPreloadJKAMusic(')+'\n'+ \
     function(codec,'extern "C" snd_stream_t *S_CodecOpenStream(')+'\n'+ \
     function(codec,'extern "C" void S_CodecCloseStream(')+'\n'+ \
     function(codec,'extern "C" snd_stream_t *S_CodecUtilOpen(')+'\n'+ \
     function(codec,'extern "C" void S_CodecUtilClose(') + r'''
    static void CloseDecoder(snd_stream_t *stream){
     ++decoderCloses;S_CodecUtilClose(&stream);
     if(dropClose){dropClose=false;Com_Error(ERR_DROP,"close fixture failure");}
    }
    static snd_codec_t fixtureCodec{CloseDecoder};
    static void *S_CodecGetSound(const char *name,snd_info_t *,bool quiet){
     assert(!quiet);if(++decoders==dropOpenAt)Com_Error(ERR_DROP,"open fixture failure");
     return S_CodecUtilOpen(name,&fixtureCodec);
    }
    static snd_stream_t *playing;
    static std::string playedLoop;
    static void Shutdown(){}
    static void StartMarker(const char *,const char *){}
    static struct {void (*Shutdown)();void (*StartBackgroundTrack)(const char *,const char *);} si;
    extern "C" void S_StopBackgroundTrack(){
     ++stopCalls;if(playing){auto *stream=playing;playing=nullptr;S_CodecCloseStream(stream);}
    }
    extern "C" void S_StartBackgroundTrack(const char *intro,const char *loop){
     S_StopBackgroundTrack();++playCalls;playedLoop=loop;playing=S_CodecOpenStream(intro);
    }
    ''' + function(facade,'extern "C" void S_JKAStartBackgroundTrack(') + r'''
    template<class F>static void Refuse(F action){bool caught=false;try{action();}catch(Drop){caught=true;}assert(caught);}
    int main(){
     // Sound disabled returns before examining a dynamic set or opening files.
     S_JKAStartBackgroundTrack("dynamicset",nullptr,qtrue);assert(!opens&&!playCalls&&!stopCalls);
     si.Shutdown=Shutdown;si.StartBackgroundTrack=StartMarker;
     S_JKAStartBackgroundTrack("music/a","music/b",qtrue);
     assert(jkaMusicPreload.Count()==2&&files.size()==2&&decoders==2&&!playCalls&&!playing);
     assert(!jkaMusicOpening&&stopCalls==1);
     const int warmed=opens;
     S_JKAStartBackgroundTrack("music/a","music/b",qfalse);
     assert(playing&&playedLoop=="music/b.mp3"&&playCalls==1&&opens==warmed&&jkaMusicPreload.Count()==1);
     // The active player owns the transferred decoder; cgame cleanup releases
     // only the remaining cache, preserving the player's real owner.
     S_ClearJKAMusicPreload();assert(jkaMusicPreload.Count()==0&&files.size()==1&&playing);
     S_StopBackgroundTrack();assert(files.empty());
     S_JKAStartBackgroundTrack("music/a","music/a",qtrue);assert(jkaMusicPreload.Count()==1);
     S_JKAStartBackgroundTrack("music/a","music/b",qtrue);assert(jkaMusicPreload.Count()==2&&files.size()==2);
     Refuse([]{S_JKAStartBackgroundTrack("../escape",nullptr,qtrue);});assert(files.empty());
     Refuse([]{S_JKAStartBackgroundTrack("dynamicset",nullptr,qtrue);});
     const int oldWarnings=warnings;S_JKAStartBackgroundTrack("music/large",nullptr,qtrue);
     assert(!jkaMusicPreload.Count()&&files.empty()&&warnings==oldWarnings+1&&!jkaMusicOpening);
     S_JKAStartBackgroundTrack("music/missing",nullptr,qtrue);assert(!jkaMusicPreload.Count()&&files.empty());
     S_JKAStartBackgroundTrack("",nullptr,qfalse);assert(!playing&&files.empty());
     // Actual owner cleanup before longjmp closes the first decoder when the
     // second provider open fails; a close failure cannot close that owner twice.
     dropOpenAt=decoders+2;jumpRecovery=true;
     if(!setjmp(recovery)){S_JKAStartBackgroundTrack("music/a","music/b",qtrue);assert(false);}
     assert(files.empty()&&!jkaMusicOpening&&!jkaMusicPreload.Count());dropOpenAt=0;jumpRecovery=false;
     S_JKAStartBackgroundTrack("music/a","music/b",qtrue);dropClose=true;jumpRecovery=true;
     if(!setjmp(recovery)){S_ClearJKAMusicPreload();assert(false);}
     jumpRecovery=false;assert(files.empty()&&!jkaMusicOpening&&!jkaMusicPreload.Count());S_ClearJKAMusicPreload();
     const int priorOpens=opens,priorPlays=playCalls;profile=COM_GAME_QUAKE3;
     Refuse([]{S_JKAStartBackgroundTrack("music/a",nullptr,qtrue);});
     Refuse([]{S_CodecPreloadJKAMusic("music/a",nullptr);});
     assert(opens==priorOpens&&playCalls==priorPlays&&files.empty());
    }
    '''
    return code

class JKAMusicHostTests(unittest.TestCase):
    def test_disabled_preload_handoff_bounds_profile_and_engine_recovery(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler unavailable')
        scratch=ROOT/'.tmp/tests/jka-music-host'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            source=Path(folder)/'music.cpp';source.write_text(fixture())
            binary=Path(folder)/('music.exe' if os.name=='nt' else 'music')
            result=subprocess.run([compiler,*engine_fixture_args(compiler),'-std=c++17','-O2','-ffast-math',
                '-Wall','-Wextra','-Werror','-I',str(ROOT),str(source),'-o',str(binary)],
                cwd=ROOT,text=True,capture_output=True,timeout=120)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(binary)],cwd=ROOT,text=True,capture_output=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
